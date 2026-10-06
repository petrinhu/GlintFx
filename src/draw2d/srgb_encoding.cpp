// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/srgb_encoding.hpp"

#include <cmath>

namespace glintfx::draw2d {

float srgb_encode(float linear) noexcept {
    if (!(linear > 0.0F)) {
        return 0.0F; // zero, negative (and a NaN, which this never sees: the frame descriptor is
                     // checked)
    }
    if (linear < 0.0031308F) {
        return linear * 12.92F;
    }
    return 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
}

glintfx::gltfx_rgba srgb_encode(glintfx::gltfx_rgba linear) noexcept {
    return glintfx::gltfx_rgba{srgb_encode(linear.red), srgb_encode(linear.green),
                               srgb_encode(linear.blue), linear.alpha};
}

} // namespace glintfx::draw2d
