// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/srgb_encoding.hpp"

#include "core/srgb_transfer.hpp"

namespace glintfx::draw2d {

float srgb_encode(float linear) noexcept {
    if (!(linear > 0.0F)) {
        return 0.0F; // zero, negative (and a NaN, which this never sees: the frame descriptor is
                     // checked)
    }
    return glintfx::linear_channel_to_srgb_unclamped(linear);
}

glintfx::gltfx_rgba srgb_encode(glintfx::gltfx_rgba linear) noexcept {
    return glintfx::gltfx_rgba{srgb_encode(linear.red), srgb_encode(linear.green),
                               srgb_encode(linear.blue), linear.alpha};
}

} // namespace glintfx::draw2d
