// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/light_tint_validation.hpp"

#include <cmath>
#include <string_view>

#include <glintfx/core/err_code.hpp>

// core/light_tint_validation.cpp - R2D-SPRITE-SHADE, H4: the refusals of
// core/light_tint_validation.hpp, one check per field.
namespace glintfx {
namespace {

using check_result = gltfx_rslt<void>;

// A refusal names the field that was refused, in rejected_value(), with invalid_argument.
check_result refused(std::string_view field) noexcept {
    return check_result::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field));
}

bool is_finite_point(gltfx_vec2_world point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool is_light_channel(float channel) noexcept { return std::isfinite(channel) && channel >= 0.0F; }

// A known law of fall-off. `unspecified` is refused; a value this build does not know is refused
// by the return below, never by a default: a new enumerator must be decided, not silently taken.
bool is_known_falloff(gltfx_light_falloff law) noexcept {
    switch (law) {
    case gltfx_light_falloff::linear:
    case gltfx_light_falloff::smooth:
        return true;
    case gltfx_light_falloff::unspecified:
        break;
    }
    return false; // unspecified, or a value this build does not know
}

check_result check_vertices(std::span<const gltfx_vec2_world> vertices) noexcept {
    for (const gltfx_vec2_world &vertex : vertices) {
        if (!is_finite_point(vertex)) {
            return refused("vertices");
        }
    }
    return check_result::ok();
}

check_result check_light(const gltfx_light_source_2d &light) noexcept {
    if (!is_finite_point(light.position)) {
        return refused("position");
    }
    if (!(std::isfinite(light.range) && light.range > 0.0)) {
        return refused("range");
    }
    if (!std::isfinite(light.intensity) || light.intensity < 0.0) {
        return refused("intensity");
    }
    if (!is_light_amount(light.color)) {
        return refused("color");
    }
    if (!is_known_falloff(light.falloff)) {
        return refused("falloff");
    }
    return check_result::ok();
}

check_result check_lights(std::span<const gltfx_light_source_2d> lights) noexcept {
    for (const gltfx_light_source_2d &light : lights) {
        const check_result checked = check_light(light);
        if (checked.has_error()) {
            return checked;
        }
    }
    return check_result::ok();
}

} // namespace

bool is_light_amount(gltfx_rgba light) noexcept {
    return is_light_channel(light.red) && is_light_channel(light.green) &&
           is_light_channel(light.blue);
}

gltfx_rslt<void> validate_light_tint_input(std::span<const gltfx_vec2_world> vertices,
                                           std::span<const gltfx_light_source_2d> lights,
                                           gltfx_rgba ambient, std::size_t tint_count) noexcept {
    if (tint_count != vertices.size()) {
        return refused("out_tints");
    }
    if (auto vertices_ok = check_vertices(vertices); vertices_ok.has_error()) {
        return vertices_ok;
    }
    if (!is_light_amount(ambient)) {
        return refused("ambient");
    }
    return check_lights(lights);
}

} // namespace glintfx
