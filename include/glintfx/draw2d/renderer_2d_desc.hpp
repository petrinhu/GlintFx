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
};

} // namespace glintfx
