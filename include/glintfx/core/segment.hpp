// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/segment.hpp - R2D-SHAPES. A straight segment in WORLD space: its two
// ends. A value type, like gltfx_rect_world: the layout is the contract
// (GODS_LAWS.md L-19, L-26). Pure geometry, no drawing, no GL, no operating
// system.

namespace glintfx {

struct gltfx_segment_world {
    gltfx_vec2_world start;
    gltfx_vec2_world end;
};

} // namespace glintfx
