// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/stroke.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9;
// decisions D-W8-05 and D-W8-08 of the W8 plan).
//
// THE THICKNESS UNIT is explicit (D-W8-05). World is the default: a line
// drawn with thickness 2.0 in world units scales with the batch transform,
// the way raylib, Cairo and the browser canvas do. Pixel keeps the thickness
// on the screen whatever the zoom, the way Dear ImGui does; the library
// expands a pixel-unit stroke AFTER the batch transform is applied.
//
// THE LINE CAP is a value, never a sign: `flat` ends the line at its exact
// end point (the default; `butt` in SVG and in the browser canvas), `square`
// extends it by half the thickness, and `rounded` closes it with a half-disc.
// Append-only: a value is never renumbered and never reused.
//
// A VALUE OUTSIDE THESE ENUMERATIONS (a consumer's static_cast) refuses the
// whole call, counted as `unknown_option` in the frame report (D-W8-285).
// It never falls back to a default silently.
//
// THE RECTANGLE ALIGNMENT lives in gltfx_rect_outline_style, read by
// stroke_rect(), not here: lines and ellipses have no inside or outside, and a
// field that a method ignores is a lie about what that method does.

namespace glintfx {

enum class gltfx_stroke_unit : std::uint8_t {
    world = 0,
    pixel = 1,
};

enum class gltfx_line_cap : std::uint8_t {
    flat = 0,
    square = 1,
    rounded = 2,
};

enum class gltfx_stroke_align : std::uint8_t {
    centered = 0,
    inside = 1,
    outside = 2,
};

struct gltfx_stroke {
    // Zero draws nothing and covers no pixel. A negative value is refused
    // and counted in the frame report (refusal `negative_size`).
    double thickness = 1.0;
    gltfx_stroke_unit unit = gltfx_stroke_unit::world;
};

// The style of a line (D-W8-354): the thickness and the end cap. Grouped so
// that stroke_line() keeps within four parameters (CONTRACT.md §6.2).
struct gltfx_line_style {
    gltfx_stroke stroke{};
    gltfx_line_cap cap = gltfx_line_cap::flat;
};

// The style of a rectangle outline (D-W8-354): the thickness and the
// alignment on the edge. The alignment belongs to the rectangle, and not to
// gltfx_stroke, because lines and ellipses ignore it.
struct gltfx_rect_outline_style {
    gltfx_stroke stroke{};
    gltfx_stroke_align align = gltfx_stroke_align::centered;
};

} // namespace glintfx
