// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_sampling.hpp"

#include <cstdint>
#include <optional>

#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "gl_abi.hpp"

// draw2d/texture_sampling.cpp - R2D-TEXTURE, fatia T2b: the table of texture_sampling.hpp. Values
// are the GL enumerators of third_party/khronos/gl.xml. No state, no GL call: a pure function.
namespace glintfx::draw2d {

namespace {

using render::GLint;

constexpr GLint k_gl_nearest = 0x2600;
constexpr GLint k_gl_linear = 0x2601;
constexpr GLint k_gl_nearest_mipmap_nearest = 0x2700;
constexpr GLint k_gl_linear_mipmap_linear = 0x2703;
constexpr GLint k_gl_clamp_to_edge = 0x812F;

// Both directions clamp to the edge texel, whatever the filter (the header says why).
[[nodiscard]] texture_sampling_parameters clamped(GLint magnification,
                                                  GLint minification) noexcept {
    return texture_sampling_parameters{
        .magnification = magnification,
        .minification = minification,
        .wrap_s = k_gl_clamp_to_edge,
        .wrap_t = k_gl_clamp_to_edge,
    };
}

} // namespace

std::optional<texture_sampling_parameters>
sampling_parameters_of(glintfx::gltfx_texture_filter filter,
                       std::uint32_t reduced_levels) noexcept {
    const bool has_reduced_levels = reduced_levels > 0U;
    switch (filter) {
    case glintfx::gltfx_texture_filter::nearest:
        return clamped(k_gl_nearest,
                       has_reduced_levels ? k_gl_nearest_mipmap_nearest : k_gl_nearest);
    case glintfx::gltfx_texture_filter::linear:
        return clamped(k_gl_linear, has_reduced_levels ? k_gl_linear_mipmap_linear : k_gl_linear);
    }
    return std::nullopt;
}

} // namespace glintfx::draw2d
