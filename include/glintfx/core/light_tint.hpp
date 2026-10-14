// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/export.hpp>

// core/light_tint.hpp - R2D-SPRITE-SHADE (H4; D-W8-325 and D-W8-355). The light
// that point lights put on ground vertices: a PURE function of the core, with
// no drawing, no GL and no operating system. The consumer passes the result to
// the drawing as a colour per vertex.
//
// The light's height does not enter here: it belongs to the shadow
// (core/light_2d.hpp). A field that this function ignored would describe it
// wrongly (GODS_LAWS.md L-28), so the light has its own type.

namespace glintfx {

enum class gltfx_light_falloff : std::uint8_t {
    // The default of the field; refused by name: the law of fall-off is the
    // consumer's choice.
    unspecified = 0,
    linear = 1, // f = clamp(1 - d / range, 0, 1)
    smooth = 2, // f = clamp(1 - (d / range)^2, 0, 1)^2
};

struct gltfx_light_source_2d {
    gltfx_vec2_world position{};
    double range = 0.0;     // finite and greater than zero
    gltfx_rgba color{};     // linear light; red, green, blue finite, zero or more
    double intensity = 0.0; // finite, zero or more
    gltfx_light_falloff falloff = gltfx_light_falloff::unspecified;
};

// Writes in out_tints[i] the light that reaches vertices[i]: for red, green and
// blue, ambient plus, for each light, color * (intensity * f(d)), with d the
// distance ON THE PLANE, clamped to [0, 1]. Values above 1 in ambient and color
// are light brighter than white, and are allowed. Light has no opacity: the
// alpha of ambient and of each color is not read, and every tint has alpha 1, so
// a tint multiplied into a colour or a texture never changes its opacity.
//
// Refused with invalid_argument and the field's name: out_tints.size() !=
// vertices.size() ("out_tints"), a vertex not finite ("vertices"), an ambient
// red, green or blue not finite or negative ("ambient"), and each light field out of range
// ("position", "range", "intensity", "color", "falloff"). On a refusal nothing
// is written. Which field is named when several are out of range is not part
// of this contract.
[[nodiscard]] GLINTFX_API gltfx_rslt<void>
gltfx_light_tint(std::span<const gltfx_vec2_world> vertices,
                 std::span<const gltfx_light_source_2d> lights, gltfx_rgba ambient,
                 std::span<gltfx_rgba> out_tints) noexcept;

} // namespace glintfx
