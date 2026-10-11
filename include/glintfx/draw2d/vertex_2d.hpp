// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

// draw2d/vertex_2d.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9).
// The PUBLIC vertex of fill_triangles(). The 32-byte internal vertex the
// renderer uploads to the graphics card stays internal: it is not part of
// this header and never will be (GODS_LAWS.md L-19).
//
// Position: WORLD space, double precision, like every world position in the
// library. Colour: linear light with straight alpha, like every gltfx_rgba.
// The library premultiplies it itself before upload.
//
// Aggregate, trivially copyable; the layout IS the contract (GODS_LAWS.md L-19, L-26).
// It is NOT trivially default-constructible: position and color carry {} (errata E1, D-W8-366).

namespace glintfx {

struct gltfx_vertex_2d_world {
    gltfx_vec2_world position{};
    gltfx_rgba color{};
};

} // namespace glintfx
