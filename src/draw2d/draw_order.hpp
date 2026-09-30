// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

// draw2d/draw_order.hpp - R2D-BATCH, fatia B2b (docs/plano-w7d.md sec. 4.3, D-W7D-10): the PURE
// atom that puts a frame's pieces in painting order. It knows nothing of GL, of the operating
// system or of what a piece is: it orders KEYS, and includes nothing but the standard library
// (GODS_LAWS.md L-19: src/draw2d/ never reaches src/platform/).
//
// THE ORDER (frozen in the public header of the layer, B4): pieces are painted by (layer, order of
// submission). A lower layer is painted first, so a higher layer lands on top; two pieces on the
// SAME layer keep the order they were submitted in, every frame, on every system; a piece drawn
// without a layer is on layer 0 (k_default_draw_layer).
//
// WHY A KEY THAT IS UNIQUE BY CONSTRUCTION, AND NO stable_sort (D-W7D-10): every piece is recorded
// with the composite key (layer, submission), and the submission index is different for every
// piece of a frame, so no two keys are equal. An ordinary sort over UNIQUE keys is a TOTAL order:
// stable in effect, and reproducible - the same result however the input was arranged -
// WITHOUT std::stable_sort, which may allocate a temporary buffer and so could throw std::bad_alloc
// out of a `noexcept` function (GODS_LAWS.md L-22, docs/api-conventions.md R3). std::sort works in
// place and allocates nothing.
//
// PRECONDITION: the submission indices of the keys handed to one sort are pairwise different (the
// caller numbers the pieces of a frame 0, 1, 2 ...). Two equal keys are not "before" each other,
// so their relative order would be unspecified: the precondition is what makes the order total.
//
// The layer is a plain std::int32_t here; the public strong type gltfx_draw_layer (B4) wraps the
// same 32-bit signed value, and its default of 0 is k_default_draw_layer.
namespace glintfx::draw2d {

inline constexpr std::int32_t k_default_draw_layer = 0;

// The composite key of one piece: the layer it asked for, and the number it was submitted under.
struct draw_key {
    std::int32_t layer = k_default_draw_layer;
    std::uint32_t submission = 0;
};

// The strict order: the lower layer first; on the same layer, the lower submission first.
[[nodiscard]] constexpr bool draw_key_before(draw_key first, draw_key second) noexcept {
    if (first.layer != second.layer) {
        return first.layer < second.layer;
    }
    return first.submission < second.submission;
}

// Sorts `keys` in place into painting order. No allocation.
void sort_draw_keys(std::span<draw_key> keys) noexcept;

} // namespace glintfx::draw2d
