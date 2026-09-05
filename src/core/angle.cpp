// SPDX-License-Identifier: AGPL-3.0-or-later
//
// angle.cpp - the degrees/radians pair of glintfx::gltfx_angle. The
// leader's decision that an angle carries its unit, and why radians is
// the canonical storage, live in include/glintfx/core/angle.hpp.

#include <numbers>

#include <glintfx/core/angle.hpp>

namespace glintfx {
namespace {

// One constant, used by BOTH directions - the second function divides
// by exactly what the first multiplied by, which is what makes the
// round trip land back where it started instead of drifting through
// two independently rounded constants.
constexpr double k_radians_per_degree = std::numbers::pi / 180.0;

} // namespace

gltfx_angle gltfx_angle_from_degrees(double degrees) noexcept {
    return gltfx_angle{.radians = degrees * k_radians_per_degree};
}

double gltfx_angle_to_degrees(gltfx_angle a) noexcept { return a.radians / k_radians_per_degree; }

} // namespace glintfx
