// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>

// draw2d/gl_load_refusal.hpp - R2D-BATCH, fatia B4 (errata sec. 19, point 3): the error of
// gltfx_renderer_2d::open() when the driver lacks a GL function the renderer needs. The loader of
// src/render/ says `not_found` and names the first function it could not resolve; the public
// text of open() says `unsupported`, with rejected_value() naming that function (for example
// "glGenVertexArrays"). This pure function is the one place the word changes, so a test can prove
// it with no context: the real open() needs a compositor, this does not.
namespace glintfx::draw2d {

// `not_found` becomes `unsupported` and keeps rejected_value(); any other error is returned as it
// came (the loader has no other, and a new one must not be renamed by accident).
[[nodiscard]] glintfx::gltfx_err gl_load_refusal(const glintfx::gltfx_err &loader_error) noexcept;

} // namespace glintfx::draw2d
