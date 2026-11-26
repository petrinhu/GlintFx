// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>

// core/srgb_transfer.hpp - R2D-TEXTURE T1-fix1: the sRGB transfer curve of IEC 61966-2-1, one
// channel in each direction (public, standard math, read under GODS_LAWS.md L-29). INTERNAL header:
// it lives under src/core/, never under include/glintfx/, so it is not API and its signatures are
// free to change. The curve is written HERE and nowhere else; every caller in the library uses it:
// core/color.cpp (the public byte conversions), draw2d/srgb_encoding.cpp (the clear color) and
// draw2d/pixel_premultiply.cpp (the decode of a FRACTIONAL encoded value). draw2d including core is
// the allowed direction (GODS_LAWS.md L-19), the one draw2d already uses for core/log/emit.hpp.
//
// THE RANGE POLICY STAYS WITH THE CALLER, not here: core/color.cpp clamps a linear value to [0, 1]
// before encoding (the 8-bit format cannot hold more), while draw2d/srgb_encoding.cpp sends zero,
// negatives and NaN to zero and leaves a value above one above one (the surface clips it). The two
// policies are different contracts; the curve under them is one.
namespace glintfx {

// The IEC 61966-2-1 piecewise split point, in each direction. Encoded and linear space use
// DIFFERENT threshold constants because the curve is not a single power function - a straight
// power curve alone diverges from the standard's own shape near black, which is why the standard
// defines a short LINEAR segment there instead of forcing a single formula through zero.
inline constexpr float k_srgb_encoded_threshold = 0.04045F;
inline constexpr float k_srgb_linear_threshold = 0.0031308F;

// Display-encoded -> linear light, ONE channel.
[[nodiscard]] inline float srgb_channel_to_linear(float encoded) noexcept {
    if (encoded <= k_srgb_encoded_threshold) {
        return encoded / 12.92F;
    }
    return std::pow((encoded + 0.055F) / 1.055F, 2.4F);
}

// Linear light -> display-encoded, ONE channel - the inverse of the function above. No clamp: the
// caller applies its own range policy first (see the header comment). Above one, the power segment
// simply continues.
[[nodiscard]] inline float linear_channel_to_srgb_unclamped(float linear) noexcept {
    if (linear <= k_srgb_linear_threshold) {
        return linear * 12.92F;
    }
    return 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
}

} // namespace glintfx
