// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_update.hpp"

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
#include "draw2d/texture_update_kept.hpp"
#include "gl_functions.hpp"

// draw2d/texture_update.cpp - R2D-TEXTURE, fatia T3 (T3-fix1: the kept texels left to
// texture_update_kept.cpp, and given back when the card refuses): the writing of
// texture_update.hpp, in its order. Every GL enumerator below is the value of
// third_party/khronos/gl.xml, as in texture_upload.cpp. Only the rectangle each level touches is
// written to the card.
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

// After the writes: the state left as it was, the binding at zero, and the first GL error named:
// GL_OUT_OF_MEMORY is out_of_memory, any other is platform_failure, with the GL code. The errors
// after the first are drained too (OpenGL keeps one flag per kind), so none is left to be blamed on
// a later call of the consumer.
[[nodiscard]] gltfx_rslt<void> finish_writes(const gl_function_table &gl) noexcept {
    reset_unpack(gl);
    const GLenum error = gl.glGetError();
    drain_gl_errors(gl); // the flags after the first: none is blamed on a later call
    gl.glBindTexture(k_gl_texture_2d, 0);
    if (error == k_gl_no_error) {
        return gltfx_rslt<void>::ok();
    }
    const gltfx_err_code code = error == k_gl_out_of_memory ? gltfx_err_code::out_of_memory
                                                            : gltfx_err_code::platform_failure;
    return gltfx_rslt<void>::err(update_refusal(code, static_cast<std::int64_t>(error)));
}

// A texture that keeps nothing: level 0 written at the rectangle from the stored texels, tightly
// packed.
[[nodiscard]] gltfx_rslt<void> write_stored(const gl_function_table &gl,
                                            const texture_2d_impl &texture, const texel_rect &rect,
                                            std::span<const gltfx_rgba8> stored) noexcept {
    prepare_writes(gl, texture);
    write_level(gl, level_write{.level = 0U, .rect = rect, .row_length = 0U, .texels = stored});
    return finish_writes(gl);
}

// The kept block of the texture, as texture_update_kept.hpp reads it.
[[nodiscard]] kept_chain chain_of(texture_2d_impl &texture) noexcept {
    return kept_chain{.texels = texture.kept.texels(),
                      .width = texture.width,
                      .height = texture.height,
                      .reduced_levels = texture.reduced_levels};
}

// Level `level` of the kept block as the card reads it: its touched rectangle, rows of the width
// of that level, from the start of that level.
[[nodiscard]] level_write kept_level_write(const kept_chain &chain, std::uint32_t level,
                                           const texel_rect &rect) noexcept {
    const std::uint32_t width = reduced_side(chain.width, level);
    const std::size_t texels = static_cast<std::size_t>(width) * reduced_side(chain.height, level);
    return level_write{.level = level,
                       .rect = rect,
                       .row_length = width,
                       .texels =
                           std::span<const gltfx_rgba8>(chain.texels)
                               .subspan(level_offset(chain.width, chain.height, level), texels)};
}

// A texture with kept texels: every level, level 0 included, written at its touched rectangle from
// the kept block.
[[nodiscard]] gltfx_rslt<void> write_kept(const gl_function_table &gl,
                                          const texture_2d_impl &texture, const kept_chain &chain,
                                          const touched_levels &touched) noexcept {
    prepare_writes(gl, texture);
    for (std::uint32_t level = 0U; level < touched.count; ++level) {
        write_level(gl, kept_level_write(chain, level, touched.rects[level]));
    }
    return finish_writes(gl);
}

// The update of a texture with kept texels: swapped into the kept block, written to the card from
// it, and swapped back out when the card refuses, so the block is again what it was and no refused
// texel can reach the card later (texture_update_kept.hpp). The second swap cannot fail where the
// first passed (same block, same rectangle, same buffer); if it ever did, the refusal says so.
[[nodiscard]] gltfx_rslt<void> update_kept(const gl_function_table &gl, texture_2d_impl &texture,
                                           const texel_rect &rect,
                                           std::span<gltfx_rgba8> stored) noexcept {
    const kept_chain chain = chain_of(texture);
    touched_levels touched{};
    if (!swap_update(chain, rect, stored, touched)) {
        return gltfx_rslt<void>::err(update_refusal(gltfx_err_code::unknown, 0));
    }
    gltfx_rslt<void> written = write_kept(gl, texture, chain, touched); // not const: moved out
    if (written.has_error() && !swap_update(chain, rect, stored, touched)) {
        return gltfx_rslt<void>::err(update_refusal(gltfx_err_code::unknown, 0));
    }
    return written;
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
    const std::span<gltfx_rgba8> texels(stored.data, stored.size);
    const gltfx_rslt<void> written = texture.reduced_levels > 0U
                                         ? update_kept(gl, texture, rect, texels)
                                         : write_stored(gl, texture, rect, texels);
    allocator.release(stored.data);
    return written;
}

} // namespace glintfx::draw2d
