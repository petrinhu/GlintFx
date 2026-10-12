// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_release.hpp - R2D-TEXTURE, fatia T2a-fix1: the release of the GL texture object of
// one texture, apart from its making (texture_upload.hpp), because the two change for different
// reasons: the release is where the deferral of the T3 lands. Over an already loaded
// gl_function_table, so it runs against a fake context and a fake table
// (tests/texture_release_test.cpp).
namespace glintfx::draw2d {

// Makes the texture's context current and deletes its GL object, then deletes the record. Never
// refused: if the context cannot be made current, the GL object goes with the context and nothing
// is deleted through it. Null is harmless.
void release_texture_object(glintfx::texture_2d_impl *texture) noexcept;

} // namespace glintfx::draw2d
