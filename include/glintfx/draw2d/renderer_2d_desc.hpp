// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

// draw2d/renderer_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::open() needs besides the context.
//
// LAYOUT IS THE CONTRACT (GODS_LAWS.md L-19): adding a field is an ABI
// change. Before 1.0 (SOVERSION 0, GODS_LAWS.md L-26) that is allowed
// and is announced by the version; from 1.0 on, a new knob arrives as a
// new descriptor type with its own open() overload, never as a field
// appended here.

namespace glintfx {

struct gltfx_renderer_2d_desc {
    // A HINT, never a limit and never a promise: roughly how many pieces
    // (one call of fill_rect() or fill_quad() is one piece) you expect
    // to submit in one frame. The renderer reserves room for that many
    // at open(), so the first frames do not grow storage while you draw.
    // Zero lets the renderer choose. Submitting more than this is always
    // allowed: storage grows, and if growing fails the pieces that did
    // not fit are counted in the frame report (never a crash).
    std::size_t reserve_pieces = 0;

    // The largest distance, in PHYSICAL pixels of the surface, between a
    // true curve and the straight segments drawn in its place. Read by
    // fill_ellipse() and stroke_ellipse() (renderer_2d.hpp). The rounded cap
    // of stroke_line() does not read it yet: it keeps the fixed 0.25 pixel its
    // own comment states. A method added later that draws a curve says in its
    // own comment whether it reads it.
    // Smaller is smoother and costs more triangles. Must be a finite number
    // greater than zero; anything else is refused by open() as
    // invalid_argument, rejected_value() "curve_max_error_pixels", before
    // anything is made. There is no other interval (D-W8-284). The cost has
    // a ceiling whatever the value: one ellipse is drawn with at most 1024
    // straight segments for its fill, and its outline with at most 2048
    // points on each edge. A value so small that the ceiling is reached is
    // still drawn, with a distance larger than asked for (renderer_2d.hpp,
    // fill_ellipse(), says when the distance is kept). Default 0.25.
    double curve_max_error_pixels = 0.25;
};

} // namespace glintfx
