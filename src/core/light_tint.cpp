// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/light_tint.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/light_tint_validation.hpp"

// core/light_tint.cpp - R2D-SPRITE-SHADE, H4 (docs/auditoria-api-sprite-shade.md, section 2.1;
// decisions D-W8-325, D-W8-355 and D-W8-380). The tints of an accepted input, one step per
// function. The refusals live in light_tint_validation.cpp, their own atom (GODS_LAWS.md L-17).
namespace glintfx {
namespace {

// f(s) for s = d / range: linear is 1 - s; smooth is (1 - s^2)^2. Both clamped to [0, 1].
double falloff_factor(gltfx_light_falloff law, double s) noexcept {
    if (law == gltfx_light_falloff::linear) {
        return std::clamp(1.0 - s, 0.0, 1.0);
    }
    const double base = std::clamp(1.0 - (s * s), 0.0, 1.0);
    return base * base;
}

// The reach of one light on one vertex: intensity * f(d), computed BEFORE the colour, so that
// colour * reach never meets infinity times zero (D-W8-380).
double reach_of(const gltfx_light_source_2d &light, gltfx_vec2_world vertex) noexcept {
    const double distance = std::hypot(vertex.x - light.position.x, vertex.y - light.position.y);
    return light.intensity * falloff_factor(light.falloff, distance / light.range);
}

float clamped(double channel) noexcept { return static_cast<float>(std::clamp(channel, 0.0, 1.0)); }

// Ambient plus each light's colour * reach, red, green and blue only; the tint leaves with alpha
// 1 (light has no opacity, D-W8-397).
gltfx_rgba tint_at(gltfx_vec2_world vertex, std::span<const gltfx_light_source_2d> lights,
                   gltfx_rgba ambient) noexcept {
    double red = ambient.red;
    double green = ambient.green;
    double blue = ambient.blue;
    for (const gltfx_light_source_2d &light : lights) {
        const double reach = reach_of(light, vertex);
        red += light.color.red * reach;
        green += light.color.green * reach;
        blue += light.color.blue * reach;
    }
    return gltfx_rgba{clamped(red), clamped(green), clamped(blue), 1.0F};
}

} // namespace

gltfx_rslt<void> gltfx_light_tint(std::span<const gltfx_vec2_world> vertices,
                                  std::span<const gltfx_light_source_2d> lights, gltfx_rgba ambient,
                                  std::span<gltfx_rgba> out_tints) noexcept {
    if (auto input_ok = validate_light_tint_input(vertices, lights, ambient, out_tints.size());
        input_ok.has_error()) {
        return input_ok;
    }
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        out_tints[i] = tint_at(vertices[i], lights, ambient);
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx
