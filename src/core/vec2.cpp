// SPDX-License-Identifier: AGPL-3.0-or-later
//
// vec2.cpp - the two crossings between the project's precision
// families. The reasoning, the leader's decision and the measured
// sixteen-million limit live in include/glintfx/core/vec2.hpp; this
// file is only the definitions, kept out of line so the conversion is
// compiled inside the library (docs/api-conventions.md R5).

#include <glintfx/core/vec2.hpp>

namespace glintfx {

gltfx_vec2_screen gltfx_vec2_world_to_screen(gltfx_vec2_world v) noexcept {
    // The narrowing is EXPLICIT here, in the one place the library
    // performs it, so no compiler warning has to be silenced anywhere
    // else and no reader has to wonder whether it was intended.
    return gltfx_vec2_screen{.x = static_cast<float>(v.x), .y = static_cast<float>(v.y)};
}

gltfx_vec2_world gltfx_vec2_screen_to_world(gltfx_vec2_screen v) noexcept {
    return gltfx_vec2_world{.x = static_cast<double>(v.x), .y = static_cast<double>(v.y)};
}

} // namespace glintfx
