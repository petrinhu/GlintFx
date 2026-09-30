// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/draw_layer.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): the OPTIONAL order key of one drawn piece (the leader's decision
// of 27/08/2026, TODO.md line R2D-BATCH).
//
// THE ORDER, frozen: inside one frame, between two flush() barriers,
// pieces are painted by (layer, order of submission). A lower layer is
// painted first, so a higher layer lands on top. Two pieces on the SAME
// layer keep the order you submitted them in, every frame, on every
// system. A piece drawn without a layer is on layer 0, so a program
// that never names a layer gets plain submission order.
// Proved by: draw_order_test (the closed set of eight cells {no layer,
// equal, lower, higher} x {submitted A then B, B then A}, and a
// thousand pieces on one layer kept in submission order).
//
// WHY A TYPE AND NOT A BARE INTEGER: `gltfx_draw_layer{3}` at a call
// site says what the 3 is. A bare 3 does not, and a bare integer
// converts silently from a size, a count or a coordinate.

namespace glintfx {

struct gltfx_draw_layer {
    std::int32_t value = 0;
};

} // namespace glintfx
