// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>

#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_update.hpp - R2D-TEXTURE, fatia T3 (T3-fix1: the kept texels given back when the
// card refuses): the writing of an accepted update_texture() (texture_update_check.hpp checked it,
// and the rectangle is not empty: an empty update succeeded in the renderer before this, with no GL
// call) into the GL texture object, over an already loaded gl_function_table (no window, no
// operating system), so it runs on all five systems against a fake context and a fake table
// (tests/texture_update_test.cpp).
//
// IN THIS ORDER:
//   1. the texture's context made current (the renderer's own); its error is passed on as it came,
//      and nothing was touched;
//   2. GL errors left by the caller drained, so a stale error is never blamed on this call;
//   3. the library's memory for the stored texels of the image: out_of_memory "texture_update",
//      os_error_code() 0, and nothing on the card or in the kept texels changed;
//   4. the stored texels (pixel_premultiply.hpp); with reduced levels, they are SWAPPED into level
//      0 of the kept texels, and the texels of each reduced level that the change touches are
//      recomputed there, from the level above, exactly as the making computed them
//      (texture_update_kept.hpp), all before any write to the card;
//   5. GL: no pixel-unpack buffer, unpack alignment 1, texture unit 0, the texture bound. Without
//      reduced levels, level 0 written by glTexSubImage2D at the rectangle from the stored texels,
//      tightly packed; with them, every level (level 0 included) written at its touched rectangle
//      from the kept texels, read with GL_UNPACK_ROW_LENGTH the width of that level and
//      GL_UNPACK_SKIP_PIXELS / GL_UNPACK_SKIP_ROWS the corner of the rectangle. Row length and
//      skips set back to 0. glGenerateMipmap is never called;
//   6. the first GL error after the writes: GL_OUT_OF_MEMORY is out_of_memory "texture_update",
//      any other is platform_failure "texture_update", os_error_code() the GL code; the errors
//      after it are drained, so none is blamed on a later call. With reduced levels, the same swap
//      is then made again, so the kept texels are again exactly what they were before the call
//      and the refused texels never reach the card through a later update. What the card holds is
//      then unspecified (renderer_2d.hpp, update_texture()). The texture binding of unit 0 is left
//      at zero in every case.
namespace glintfx::draw2d {

[[nodiscard]] glintfx::gltfx_rslt<void>
update_texture_object(glintfx::texture_2d_impl &texture, const texel_rect &rect,
                      const glintfx::gltfx_image_view &image,
                      const batch_allocator &allocator) noexcept;

} // namespace glintfx::draw2d
