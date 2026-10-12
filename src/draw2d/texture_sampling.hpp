// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "gl_abi.hpp"

// draw2d/texture_sampling.hpp - R2D-TEXTURE, fatia T2b: the PURE translation of how a texture is
// sampled (its filter and its number of reduced levels) into the four GL texture parameters the
// texture object is made with. It knows nothing of the context, of the card or of the upload, and
// it takes values and returns values, so the whole table is proved without GL
// (tests/texture_sampling_test.cpp); texture_upload.cpp applies what it answers.
//
// THE TABLE (the values are the GL enumerators of third_party/khronos/gl.xml):
//   filter    reduced levels   magnification   minification
//   nearest   0                GL_NEAREST      GL_NEAREST
//   nearest   1 or more        GL_NEAREST      GL_NEAREST_MIPMAP_NEAREST
//   linear    0                GL_LINEAR       GL_LINEAR
//   linear    1 or more        GL_LINEAR       GL_LINEAR_MIPMAP_LINEAR
// `nearest` keeps the colour of ONE texel of ONE level, so a reduced level is chosen, never blended
// with the next one. `linear` blends the four nearest texels and, with reduced levels, the two
// nearest levels (the trilinear filter), as the public comment of `linear` says
// (texture_2d_desc.hpp, amended in the same fatia T2b).
//
// THE EDGES: both directions clamp to the edge texel (GL_CLAMP_TO_EDGE), whatever the filter. A
// drawn region never leaves the texture (it is refused before), so the only sample that reaches an
// edge is the linear filter's neighbour of an edge texel, and it must read the edge texel itself,
// never the opposite side of the texture (GL_REPEAT, the GL default). Repetition (tiling) is not a
// sampling mode of this fatia (D-W8-11).
//
// A FILTER THIS BUILD DOES NOT KNOW answers nothing (std::nullopt): never a default in its place.
namespace glintfx::draw2d {

struct texture_sampling_parameters {
    render::GLint magnification = 0; // GL_TEXTURE_MAG_FILTER
    render::GLint minification = 0;  // GL_TEXTURE_MIN_FILTER
    render::GLint wrap_s = 0;        // GL_TEXTURE_WRAP_S
    render::GLint wrap_t = 0;        // GL_TEXTURE_WRAP_T
};

[[nodiscard]] std::optional<texture_sampling_parameters>
sampling_parameters_of(glintfx::gltfx_texture_filter filter, std::uint32_t reduced_levels) noexcept;

} // namespace glintfx::draw2d
