// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/draw_order.hpp"

#include <algorithm>

namespace glintfx::draw2d {

void sort_draw_keys(std::span<draw_key> keys) noexcept {
    // std::sort is in place and allocates nothing. The keys are unique (the submission index of
    // each piece of a frame is different), so the strict order draw_key_before is total and the
    // result is the same however the input was arranged.
    std::sort(keys.begin(), keys.end(), [](draw_key first, draw_key second) noexcept {
        return draw_key_before(first, second);
    });
}

} // namespace glintfx::draw2d
