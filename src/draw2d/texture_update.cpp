// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_update.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>

#include "draw2d/kept_texels.hpp"
#include "draw2d/mipmap_chain.hpp"
#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/pixel_premultiply.hpp"
#include "draw2d/pod_buffer.hpp"
#include "gl_functions.hpp"

// draw2d/texture_update.cpp - R2D-TEXTURE, fatia T3: the writing of texture_update.hpp, in its
// order. Every GL enumerator below is the value of third_party/khronos/gl.xml, as in
// texture_upload.cpp. The reduced texels are recomputed here from the kept texels, and only the
// rectangle each level touches is written to the card.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_rgba8;
using glintfx::gltfx_rslt;
using glintfx::texture_2d_impl;
using render::gl_function_table;
using render::GLenum;
using render::GLint;
using render::GLsizei;
using render::GLuint;

constexpr GLenum k_gl_texture_2d = 0x0DE1;
constexpr GLenum k_gl_rgba = 0x1908;
constexpr GLenum k_gl_unsigned_byte = 0x1401;
constexpr GLenum k_gl_unpack_alignment = 0x0CF5;
constexpr GLenum k_gl_unpack_row_length = 0x0CF2;
constexpr GLenum k_gl_unpack_skip_rows = 0x0CF3;
constexpr GLenum k_gl_unpack_skip_pixels = 0x0CF4;
constexpr GLenum k_gl_pixel_unpack_buffer = 0x88EC;
constexpr GLenum k_gl_texture0 = 0x84C0;
constexpr GLenum k_gl_no_error = 0;
constexpr GLenum k_gl_out_of_memory = 0x0505;
constexpr std::uint32_t k_max_error_drain = 32U;

// Where one level lies in the kept block, and its size.
struct level_plane {
    std::size_t offset = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

// One level as it goes to the card: its number, the rectangle written, the row length of the
// texels read (0 for tightly packed rows) and the texels the rectangle is read from.
struct level_write {
    std::uint32_t level = 0U;
    texel_rect rect{};
    std::uint32_t row_length = 0U;
    std::span<const gltfx_rgba8> texels{};
};

[[nodiscard]] gltfx_err update_refusal(gltfx_err_code code, std::int64_t os_code) noexcept {
    gltfx_err failure(code);
    failure.with_rejected_value("texture_update").with_os_error_code(os_code);
    return failure;
}

void drain_gl_errors(const gl_function_table &gl) noexcept {
    for (std::uint32_t attempt = 0; attempt < k_max_error_drain; ++attempt) {
        if (gl.glGetError() == k_gl_no_error) {
            return;
        }
    }
}

[[nodiscard]] std::uint32_t side_of(const covered_range &range) noexcept {
    return range.last - range.first + 1U;
}

[[nodiscard]] std::size_t texels_of(const level_plane &plane) noexcept {
    return static_cast<std::size_t>(plane.width) * plane.height;
}

[[nodiscard]] level_plane plane_of(const texture_2d_impl &texture, std::uint32_t level) noexcept {
    return level_plane{
        .offset = level_offset(texture.width, texture.height, level),
        .width = reduced_side(texture.width, level),
        .height = reduced_side(texture.height, level),
    };
}

// The texels of one level of the kept block, read-only.
[[nodiscard]] std::span<const gltfx_rgba8> kept_level(texture_2d_impl &texture,
                                                      const level_plane &plane) noexcept {
    return std::span<const gltfx_rgba8>(texture.kept.texels())
        .subspan(plane.offset, texels_of(plane));
}

// True when the kept block holds exactly the texels of every level of the texture.
[[nodiscard]] bool kept_is_whole(texture_2d_impl &texture) noexcept {
    const std::size_t texels =
        chain_texel_count(texture.width, texture.height, texture.reduced_levels);
    return texture.kept.texels().size() == texels;
}

// Level 0 of the kept block takes the stored texels of the rectangle, row by row. False when the
// block does not hold the whole texture (never, by construction; never ignored).
[[nodiscard]] bool keep_level_zero(texture_2d_impl &texture, const texel_rect &rect,
                                   std::span<const gltfx_rgba8> stored) noexcept {
    if (!kept_is_whole(texture)) {
        return false;
    }
    const std::span<gltfx_rgba8> kept = texture.kept.texels();
    const std::size_t width = side_of(rect.columns);
    for (std::uint32_t row = rect.rows.first; row <= rect.rows.last; ++row) {
        const std::size_t from = static_cast<std::size_t>(row - rect.rows.first) * width;
        const std::size_t to = static_cast<std::size_t>(row) * texture.width + rect.columns.first;
        std::copy_n(stored.begin() + static_cast<std::ptrdiff_t>(from), width,
                    kept.begin() + static_cast<std::ptrdiff_t>(to));
    }
    return true;
}

// The rectangle of level `level` that the rectangle of the level above touches turns `rect` into
// that rectangle (targets_touching(), both axes), and the texels in it are recomputed in the kept
// block from the level above (reduce_region_to_next_level()). False when the block does not fit.
[[nodiscard]] bool reduce_touched(texture_2d_impl &texture, std::uint32_t level,
                                  texel_rect &rect) noexcept {
    const level_plane above = plane_of(texture, level - 1U);
    const level_plane here = plane_of(texture, level);
    rect = texel_rect{
        .columns = targets_touching(rect.columns, above.width, here.width),
        .rows = targets_touching(rect.rows, above.height, here.height),
    };
    const std::span<gltfx_rgba8> kept = texture.kept.texels();
    const source_level source{
        .texels = kept_level(texture, above),
        .width = above.width,
        .height = above.height,
    };
    return reduce_region_to_next_level(source, kept.subspan(here.offset, texels_of(here)), rect);
}

// One level as the card receives it: the unpack state of its rectangle, then glTexSubImage2D.
void write_level(const gl_function_table &gl, const level_write &level) noexcept {
    const bool strided = level.row_length != 0U;
    const GLint skip_rows = strided ? static_cast<GLint>(level.rect.rows.first) : 0;
    const GLint skip_pixels = strided ? static_cast<GLint>(level.rect.columns.first) : 0;
    gl.glPixelStorei(k_gl_unpack_row_length, static_cast<GLint>(level.row_length));
    gl.glPixelStorei(k_gl_unpack_skip_rows, skip_rows);
    gl.glPixelStorei(k_gl_unpack_skip_pixels, skip_pixels);
    gl.glTexSubImage2D(k_gl_texture_2d, static_cast<GLint>(level.level),
                       static_cast<GLint>(level.rect.columns.first),
                       static_cast<GLint>(level.rect.rows.first),
                       static_cast<GLsizei>(side_of(level.rect.columns)),
                       static_cast<GLsizei>(side_of(level.rect.rows)), k_gl_rgba,
                       k_gl_unsigned_byte, level.texels.data());
}

// The state every write needs: no pixel-unpack buffer, rows packed to one byte, unit 0 bound to
// the texture.
void prepare_writes(const gl_function_table &gl, const texture_2d_impl &texture) noexcept {
    gl.glBindBuffer(k_gl_pixel_unpack_buffer, 0);
    gl.glPixelStorei(k_gl_unpack_alignment, 1);
    gl.glActiveTexture(k_gl_texture0);
    gl.glBindTexture(k_gl_texture_2d, texture.gl_name);
}

// The row length and the skips back to 0, as create_texture() leaves them.
void reset_unpack(const gl_function_table &gl) noexcept {
    gl.glPixelStorei(k_gl_unpack_row_length, 0);
    gl.glPixelStorei(k_gl_unpack_skip_rows, 0);
    gl.glPixelStorei(k_gl_unpack_skip_pixels, 0);
}

// The reduced levels, from the first to the last: each one recomputed where the change touches it,
// then written to the card at that rectangle, from the kept block, with the row length of the
// level.
[[nodiscard]] bool write_reduced_levels(const gl_function_table &gl, texture_2d_impl &texture,
                                        texel_rect rect) noexcept {
    for (std::uint32_t level = 1U; level <= texture.reduced_levels; ++level) {
        if (!reduce_touched(texture, level, rect)) {
            return false;
        }
        const level_plane plane = plane_of(texture, level);
        write_level(gl, level_write{.level = level,
                                    .rect = rect,
                                    .row_length = plane.width,
                                    .texels = kept_level(texture, plane)});
    }
    return true;
}

// The GL writes of an accepted update: the state prepared, level 0 from the stored texels, the
// reduced levels, the state left as it was, and the first error after the writes.
[[nodiscard]] gltfx_rslt<void> write_to_card(const gl_function_table &gl, texture_2d_impl &texture,
                                             const texel_rect &rect,
                                             std::span<const gltfx_rgba8> stored) noexcept {
    prepare_writes(gl, texture);
    write_level(gl, level_write{.level = 0U, .rect = rect, .row_length = 0U, .texels = stored});
    const bool reduced = texture.reduced_levels == 0U || write_reduced_levels(gl, texture, rect);
    reset_unpack(gl);
    const GLenum error = gl.glGetError();
    gl.glBindTexture(k_gl_texture_2d, 0);
    if (!reduced) {
        return gltfx_rslt<void>::err(update_refusal(gltfx_err_code::unknown, 0));
    }
    if (error == k_gl_no_error) {
        return gltfx_rslt<void>::ok();
    }
    const gltfx_err_code code = error == k_gl_out_of_memory ? gltfx_err_code::out_of_memory
                                                            : gltfx_err_code::platform_failure;
    return gltfx_rslt<void>::err(update_refusal(code, static_cast<std::int64_t>(error)));
}

// The stored texels of the image, in the library's memory: the view is converted as the making
// converts it. Nothing is written to the card or to the kept texels here.
[[nodiscard]] gltfx_rslt<void> stored_texels_of(const gltfx_image_view &image,
                                                const batch_allocator &allocator,
                                                pod_buffer<gltfx_rgba8> &stored) noexcept {
    const std::size_t count = static_cast<std::size_t>(image.width) * image.height;
    stored.size = 0U;
    if (!ensure_room(allocator, stored, count)) {
        return gltfx_rslt<void>::err(update_refusal(gltfx_err_code::out_of_memory, 0));
    }
    stored.size = count;
    return to_stored_texels(image, std::span<gltfx_rgba8>(stored.data, stored.size));
}

} // namespace

gltfx_rslt<void> update_texture_object(texture_2d_impl &texture, const texel_rect &rect,
                                       const gltfx_image_view &image,
                                       const batch_allocator &allocator) noexcept {
    const gltfx_rslt<void> current = texture.access.make_current(texture.access.context);
    if (current.has_error()) {
        return gltfx_rslt<void>::err(current.err());
    }
    const gl_function_table &gl = *texture.access.gl;
    drain_gl_errors(gl);
    pod_buffer<gltfx_rgba8> stored{};
    const gltfx_rslt<void> converted = stored_texels_of(image, allocator, stored);
    if (converted.has_error()) {
        allocator.release(stored.data);
        return gltfx_rslt<void>::err(converted.err());
    }
    const std::span<const gltfx_rgba8> texels(stored.data, stored.size);
    if (texture.reduced_levels > 0U) {
        if (!keep_level_zero(texture, rect, texels)) {
            allocator.release(stored.data);
            return gltfx_rslt<void>::err(update_refusal(gltfx_err_code::unknown, 0));
        }
    }
    const gltfx_rslt<void> written = write_to_card(gl, texture, rect, texels);
    allocator.release(stored.data);
    return written;
}

} // namespace glintfx::draw2d
