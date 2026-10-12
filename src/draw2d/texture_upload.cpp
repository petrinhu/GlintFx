// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_upload.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/pixel_premultiply.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/texture_request_check.hpp"
#include "draw2d/texture_sampling.hpp"
#include "gl_functions.hpp"

// draw2d/texture_upload.cpp - R2D-TEXTURE, fatia T2a: the making and the release of one GL texture
// object, in the order texture_upload.hpp lists. Every GL enumerator below is the value of
// third_party/khronos/gl.xml. Nothing here allocates after the first GL call: the texel storage of
// level 0 and of level 1 is reserved before it, so a refusal of the library's memory creates no GL
// object.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_rgba8;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d_desc;
using glintfx::texture_2d_impl;
using render::gl_function_table;
using render::GLenum;
using render::GLint;
using render::GLsizei;
using render::GLuint;

constexpr GLenum k_gl_texture_2d = 0x0DE1;
constexpr GLenum k_gl_srgb8_alpha8 = 0x8C43;
constexpr GLenum k_gl_rgba = 0x1908;
constexpr GLenum k_gl_unsigned_byte = 0x1401;
constexpr GLenum k_gl_texture_base_level = 0x813C;
constexpr GLenum k_gl_texture_max_level = 0x813D;
constexpr GLenum k_gl_texture_mag_filter = 0x2800;
constexpr GLenum k_gl_texture_min_filter = 0x2801;
constexpr GLenum k_gl_texture_wrap_s = 0x2802;
constexpr GLenum k_gl_texture_wrap_t = 0x2803;
constexpr GLenum k_gl_unpack_alignment = 0x0CF5;
constexpr GLenum k_gl_unpack_row_length = 0x0CF2;
constexpr GLenum k_gl_unpack_skip_rows = 0x0CF3;
constexpr GLenum k_gl_unpack_skip_pixels = 0x0CF4;
constexpr GLenum k_gl_pixel_unpack_buffer = 0x88EC;
constexpr GLenum k_gl_max_texture_size = 0x0D33;
constexpr GLenum k_gl_texture0 = 0x84C0;
constexpr GLenum k_gl_no_error = 0;
constexpr GLenum k_gl_out_of_memory = 0x0505;
constexpr std::uint32_t k_max_error_drain = 32U;

// The two texel levels that take turns: level k is computed into the buffer that held level k - 2.
// Both are sized before the first GL call, for level 0 and for level 1, the largest reduced level.
struct texel_levels {
    pod_buffer<gltfx_rgba8> first;
    pod_buffer<gltfx_rgba8> second;
};

// What one creation needs, gathered once: the request, its checked number of reduced levels, and
// the allocator of the texel storage.
struct creation {
    const texture_gl_access &access;
    const gltfx_image_view &image;
    gltfx_texture_2d_desc desc;
    std::uint32_t levels = 0U;
    batch_allocator allocator = default_batch_allocator();
    texture_sampling_parameters sampling{};
};

[[nodiscard]] gltfx_err texture_refusal(gltfx_err_code code, std::string_view field,
                                        std::int64_t os_code) noexcept {
    gltfx_err failure(code);
    failure.with_rejected_value(field).with_os_error_code(os_code);
    return failure;
}

[[nodiscard]] gltfx_err memory_refusal() noexcept {
    return texture_refusal(gltfx_err_code::out_of_memory, "texture_create", 0);
}

[[nodiscard]] gltfx_err gl_refusal(GLenum error) noexcept {
    const gltfx_err_code code = error == k_gl_out_of_memory ? gltfx_err_code::out_of_memory
                                                            : gltfx_err_code::platform_failure;
    return texture_refusal(code, "texture_create", static_cast<std::int64_t>(error));
}

void drain_gl_errors(const gl_function_table &gl) noexcept {
    for (std::uint32_t attempt = 0; attempt < k_max_error_drain; ++attempt) {
        if (gl.glGetError() == k_gl_no_error) {
            return;
        }
    }
}

// The card's limit is read from the card; each side is compared with it on its own, so a width or a
// height above the limit is refused by its name.
[[nodiscard]] gltfx_rslt<void> check_card_limit(const gl_function_table &gl,
                                                const gltfx_image_view &image) noexcept {
    GLint max_texture_size = 0;
    gl.glGetIntegerv(k_gl_max_texture_size, &max_texture_size);
    const auto max_side = static_cast<std::uint32_t>(std::max(max_texture_size, 0));
    if (image.width > max_side) {
        return gltfx_rslt<void>::err(texture_refusal(gltfx_err_code::unsupported, "width", 0));
    }
    if (image.height > max_side) {
        return gltfx_rslt<void>::err(texture_refusal(gltfx_err_code::unsupported, "height", 0));
    }
    return gltfx_rslt<void>::ok();
}

[[nodiscard]] std::size_t level_texel_count(std::uint32_t width, std::uint32_t height,
                                            std::uint32_t level) noexcept {
    return static_cast<std::size_t>(reduced_side(width, level)) * reduced_side(height, level);
}

// Makes room for `count` texels in `buffer` and sets its size to them. False when the allocator
// refused; the buffer then holds no texels, and its old block (if any) is untouched.
[[nodiscard]] bool take_texels(const batch_allocator &allocator, pod_buffer<gltfx_rgba8> &buffer,
                               std::size_t count) noexcept {
    buffer.size = 0;
    if (!ensure_room(allocator, buffer, count)) {
        return false;
    }
    buffer.size = count;
    return true;
}

[[nodiscard]] gltfx_rslt<void> reserve_levels(const creation &plan, texel_levels &texels) noexcept {
    const std::size_t level_zero = level_texel_count(plan.image.width, plan.image.height, 0U);
    if (!take_texels(plan.allocator, texels.first, level_zero)) {
        return gltfx_rslt<void>::err(memory_refusal());
    }
    if (plan.levels == 0U) {
        return gltfx_rslt<void>::ok();
    }
    const std::size_t level_one = level_texel_count(plan.image.width, plan.image.height, 1U);
    if (!take_texels(plan.allocator, texels.second, level_one)) {
        return gltfx_rslt<void>::err(memory_refusal());
    }
    return gltfx_rslt<void>::ok();
}

// Reserves the storage, then stores level 0 in its stored form (pixel_premultiply.hpp).
[[nodiscard]] gltfx_rslt<void> prepare_texels(const creation &plan, texel_levels &texels) noexcept {
    const gltfx_rslt<void> reserved = reserve_levels(plan, texels);
    if (reserved.has_error()) {
        return gltfx_rslt<void>::err(reserved.err());
    }
    return to_stored_texels(plan.image,
                            std::span<gltfx_rgba8>(texels.first.data, texels.first.size));
}

void release_levels(const batch_allocator &allocator, texel_levels &texels) noexcept {
    allocator.release(texels.first.data);
    allocator.release(texels.second.data);
    texels = texel_levels{};
}

[[nodiscard]] texture_2d_impl *new_record(const creation &plan) noexcept {
    return new (std::nothrow) texture_2d_impl{
        .access = plan.access,
        .gl_name = 0,
        .width = plan.image.width,
        .height = plan.image.height,
        .filter = plan.desc.filter,
        .reduced_levels = plan.levels,
    };
}

// Client memory is read as tightly packed rows, from no buffer, with no skips.
void set_unpack_state(const gl_function_table &gl) noexcept {
    gl.glBindBuffer(k_gl_pixel_unpack_buffer, 0);
    gl.glPixelStorei(k_gl_unpack_alignment, 1);
    gl.glPixelStorei(k_gl_unpack_row_length, 0);
    gl.glPixelStorei(k_gl_unpack_skip_rows, 0);
    gl.glPixelStorei(k_gl_unpack_skip_pixels, 0);
}

// One new texture object on unit 0, complete with the levels the record says exist.
// The four sampling parameters of the new texture, set while it is bound.
void apply_sampling(const gl_function_table &gl,
                    const texture_sampling_parameters &sampling) noexcept {
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_mag_filter, sampling.magnification);
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_min_filter, sampling.minification);
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_wrap_s, sampling.wrap_s);
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_wrap_t, sampling.wrap_t);
}

void prepare_gl_texture(const gl_function_table &gl, texture_2d_impl &record,
                        const texture_sampling_parameters &sampling) noexcept {
    gl.glActiveTexture(k_gl_texture0);
    GLuint name = 0;
    gl.glGenTextures(1, &name);
    record.gl_name = name;
    gl.glBindTexture(k_gl_texture_2d, name);
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_base_level, 0);
    gl.glTexParameteri(k_gl_texture_2d, k_gl_texture_max_level,
                       static_cast<GLint>(record.reduced_levels));
    apply_sampling(gl, sampling);
}

// One level as it goes to the card: its number, its size in texels, and its stored texels.
struct level_image {
    std::uint32_t level = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::span<const gltfx_rgba8> texels;
};

void upload_level(const gl_function_table &gl, const level_image &image) noexcept {
    gl.glTexImage2D(k_gl_texture_2d, static_cast<GLint>(image.level),
                    static_cast<GLint>(k_gl_srgb8_alpha8), static_cast<GLsizei>(image.width),
                    static_cast<GLsizei>(image.height), 0, k_gl_rgba, k_gl_unsigned_byte,
                    image.texels.data());
}

// Computes reduced level `level` from `source` (the level above) into `target`. False when the
// storage does not fit the level, which the reservation makes impossible.
[[nodiscard]] bool reduce_one_level(const texture_2d_impl &record, std::uint32_t level,
                                    const pod_buffer<gltfx_rgba8> &source,
                                    pod_buffer<gltfx_rgba8> &target) noexcept {
    const std::uint32_t source_width = reduced_side(record.width, level - 1U);
    const std::uint32_t source_height = reduced_side(record.height, level - 1U);
    target.size = level_texel_count(record.width, record.height, level);
    return reduce_to_next_level(std::span<const gltfx_rgba8>(source.data, source.size),
                                source_width, source_height,
                                std::span<gltfx_rgba8>(target.data, target.size));
}

// Uploads level 0, then each reduced level right after it is computed, from the level above.
[[nodiscard]] bool upload_levels(const gl_function_table &gl, const texture_2d_impl &record,
                                 texel_levels &texels) noexcept {
    upload_level(gl,
                 level_image{
                     .level = 0U,
                     .width = record.width,
                     .height = record.height,
                     .texels = std::span<const gltfx_rgba8>(texels.first.data, texels.first.size),
                 });
    pod_buffer<gltfx_rgba8> *source = &texels.first;
    pod_buffer<gltfx_rgba8> *target = &texels.second;
    for (std::uint32_t level = 1; level <= record.reduced_levels; ++level) {
        if (!reduce_one_level(record, level, *source, *target)) {
            return false;
        }
        upload_level(gl, level_image{
                             .level = level,
                             .width = reduced_side(record.width, level),
                             .height = reduced_side(record.height, level),
                             .texels = std::span<const gltfx_rgba8>(target->data, target->size),
                         });
        std::swap(source, target);
    }
    return true;
}

void delete_gl_texture(const gl_function_table &gl, const texture_2d_impl &record) noexcept {
    const GLuint name = record.gl_name;
    gl.glDeleteTextures(1, &name);
}

// The GL object: unpack state, the texture, every level, then the first error after the upload.
// The binding of unit 0 is cleared in every case, and a failed texture is deleted.
[[nodiscard]] gltfx_rslt<texture_2d_impl *>
create_gl_texture(const creation &plan, texture_2d_impl &record, texel_levels &texels) noexcept {
    const gl_function_table &gl = *plan.access.gl;
    set_unpack_state(gl);
    prepare_gl_texture(gl, record, plan.sampling);
    const bool uploaded = upload_levels(gl, record, texels);
    const GLenum error = gl.glGetError();
    gl.glBindTexture(k_gl_texture_2d, 0);
    if (!uploaded) {
        delete_gl_texture(gl, record);
        return gltfx_rslt<texture_2d_impl *>::err(
            texture_refusal(gltfx_err_code::unknown, "texture_create", 0));
    }
    if (error != k_gl_no_error) {
        delete_gl_texture(gl, record);
        return gltfx_rslt<texture_2d_impl *>::err(gl_refusal(error));
    }
    return gltfx_rslt<texture_2d_impl *>::ok(&record);
}

[[nodiscard]] gltfx_rslt<texture_2d_impl *> build_texture(const creation &plan) noexcept {
    texel_levels texels;
    const gltfx_rslt<void> prepared = prepare_texels(plan, texels);
    if (prepared.has_error()) {
        release_levels(plan.allocator, texels);
        return gltfx_rslt<texture_2d_impl *>::err(prepared.err());
    }
    texture_2d_impl *record = new_record(plan);
    if (record == nullptr) {
        release_levels(plan.allocator, texels);
        return gltfx_rslt<texture_2d_impl *>::err(memory_refusal());
    }
    const gltfx_rslt<texture_2d_impl *> made = create_gl_texture(plan, *record, texels);
    release_levels(plan.allocator, texels);
    if (made.has_error()) {
        delete record;
        return gltfx_rslt<texture_2d_impl *>::err(made.err());
    }
    return gltfx_rslt<texture_2d_impl *>::ok(record);
}

} // namespace

gltfx_rslt<texture_2d_impl *> create_texture_object(const texture_gl_access &access,
                                                    const gltfx_image_view &image,
                                                    const gltfx_texture_2d_desc &desc,
                                                    const batch_allocator &allocator) noexcept {
    const gltfx_rslt<std::uint32_t> levels = check_texture_request(image, desc);
    if (levels.has_error()) {
        return gltfx_rslt<texture_2d_impl *>::err(levels.err());
    }
    const std::optional<texture_sampling_parameters> sampling =
        sampling_parameters_of(desc.filter, levels.value());
    if (!sampling.has_value()) {
        return gltfx_rslt<texture_2d_impl *>::err(
            texture_refusal(gltfx_err_code::invalid_argument, "filter", 0));
    }
    const gltfx_rslt<void> current = access.make_current(access.context);
    if (current.has_error()) {
        return gltfx_rslt<texture_2d_impl *>::err(current.err());
    }
    const gl_function_table &gl = *access.gl;
    drain_gl_errors(gl);
    const gltfx_rslt<void> limit = check_card_limit(gl, image);
    if (limit.has_error()) {
        return gltfx_rslt<texture_2d_impl *>::err(limit.err());
    }
    return build_texture(creation{
        .access = access,
        .image = image,
        .desc = desc,
        .levels = levels.value(),
        .allocator = allocator,
        .sampling = *sampling,
    });
}

} // namespace glintfx::draw2d
