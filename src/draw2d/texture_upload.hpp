// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_upload.hpp - R2D-TEXTURE, fatia T2a: the making and the release of the GL texture
// object of one texture, over an already loaded gl_function_table (no window, no operating system),
// so it runs on all five systems against a fake context and a fake table
// (tests/texture_upload_test.cpp).
//
// MAKING, in this order (each refusal leaves no GL object alive and nothing bound):
//   1. the request (texture_request_check.hpp): refused before the context is touched;
//   2. the context made current (the renderer's own); its error is passed on as it came;
//   3. GL errors left by the caller drained, so a stale error is never blamed on this call;
//   4. the card's limit, read with glGetIntegerv(GL_MAX_TEXTURE_SIZE): a larger width is
//      unsupported "width", a larger height unsupported "height";
//   5. the library's memory (the texels of level 0 and of the reduced levels, and the record):
//      out_of_memory "texture_create", os_error_code() 0;
//   6. the stored texels (pixel_premultiply.hpp) and the reduced levels (mipmap_reduce.hpp);
//   7. GL: no pixel-unpack buffer, unpack alignment 1, row length and skips 0, texture unit 0; one
//      new texture with GL_TEXTURE_BASE_LEVEL 0 and GL_TEXTURE_MAX_LEVEL the number of reduced
//      levels; every level uploaded with glTexImage2D as GL_SRGB8_ALPHA8 from GL_RGBA /
//      GL_UNSIGNED_BYTE; glGenerateMipmap is never called;
//   8. the first GL error after the upload: GL_OUT_OF_MEMORY is out_of_memory "texture_create",
//      any other is platform_failure "texture_create", os_error_code() the GL code, and the texture
//      is deleted. The texture binding of unit 0 is left at zero in every case.
namespace glintfx::draw2d {

[[nodiscard]] glintfx::gltfx_rslt<glintfx::texture_2d_impl *>
create_texture_object(const texture_gl_access &access, const glintfx::gltfx_image_view &image,
                      const glintfx::gltfx_texture_2d_desc &desc,
                      const batch_allocator &allocator) noexcept;

// Makes the texture's context current and deletes its GL object, then deletes the record. Never
// refused: if the context cannot be made current, the GL object goes with the context. Null is
// harmless.
void release_texture_object(glintfx::texture_2d_impl *texture) noexcept;

} // namespace glintfx::draw2d
