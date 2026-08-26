// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/color.hpp>

#include <algorithm>
#include <cmath>

// color.cpp - CORE-COLOR (TODO.md, GODS_LAWS.md L-17: one table/one
// function per concern, no switch growing a case per feature): the
// sRGB transfer function pair (IEC 61966-2-1 - public, standard math,
// read under GODS_LAWS.md L-29, never a line of RmlUi/SDL3 code) that
// gltfx_rgba_from_srgb8()/gltfx_rgba_to_srgb8() apply per COLOR
// channel. Alpha never goes through either function here - see
// color.hpp's own header comment for why.
//
// color_test.cpp is the TDD red/green witness for every function in
// this file (GODS_LAWS.md L-20): the round trip over all 256 byte
// values, the linear-vs-encoded semantic check that is decision 3's
// whole reason to exist, and the headroom-clamp check for decision 1.

namespace glintfx {

namespace {

// The IEC 61966-2-1 piecewise split point, in each direction. Encoded
// and linear space use DIFFERENT threshold constants because the
// curve is not a single power function - a straight power curve alone
// diverges from the standard's own shape near black, which is why the
// standard defines a short LINEAR segment there instead of forcing a
// single formula through zero.
constexpr float k_srgb_encoded_threshold = 0.04045F;
constexpr float k_srgb_linear_threshold = 0.0031308F;
constexpr float k_byte_max = 255.0F;

// Display-encoded -> linear light, ONE channel.
float srgb_channel_to_linear(float encoded) {
    if (encoded <= k_srgb_encoded_threshold) {
        return encoded / 12.92F;
    }
    return std::pow((encoded + 0.055F) / 1.055F, 2.4F);
}

// Linear light -> display-encoded, ONE channel - the inverse of the
// function above. Clamps FIRST: this is the one lossy step decision 6
// accepts for values outside what the display's 8-bit format can
// represent at all (see this function's call sites in
// gltfx_rgba_to_srgb8() below).
float linear_channel_to_srgb(float linear) {
    const float clamped = std::clamp(linear, 0.0F, 1.0F);
    if (clamped <= k_srgb_linear_threshold) {
        return clamped * 12.92F;
    }
    return 1.055F * std::pow(clamped, 1.0F / 2.4F) - 0.055F;
}

// 0..1 -> 0..255, rounded to the nearest byte - shared by every
// channel INCLUDING alpha, which skips the transfer function above
// but still needs this same quantization step. Clamps again on
// purpose: alpha never passes through linear_channel_to_srgb(), so
// this is the ONLY clamp an out-of-[0,1] alpha value ever gets.
std::uint8_t unit_to_byte(float unit) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * k_byte_max));
}

} // namespace

gltfx_rgba gltfx_rgba_from_srgb8(gltfx_rgba8 encoded) noexcept {
    return gltfx_rgba{
        .r = srgb_channel_to_linear(static_cast<float>(encoded.r) / k_byte_max),
        .g = srgb_channel_to_linear(static_cast<float>(encoded.g) / k_byte_max),
        .b = srgb_channel_to_linear(static_cast<float>(encoded.b) / k_byte_max),
        .a = static_cast<float>(encoded.a) / k_byte_max,
    };
}

gltfx_rgba8 gltfx_rgba_to_srgb8(gltfx_rgba color) noexcept {
    return gltfx_rgba8{
        .r = unit_to_byte(linear_channel_to_srgb(color.r)),
        .g = unit_to_byte(linear_channel_to_srgb(color.g)),
        .b = unit_to_byte(linear_channel_to_srgb(color.b)),
        .a = unit_to_byte(color.a),
    };
}

// Decision 6's second half. Straight multiplication, no clamp: unlike
// gltfx_rgba_to_srgb8() above, this never has to fit into 8 bits -
// the result is still a gltfx_rgba, still allowed decision 1's own
// headroom above white.
gltfx_rgba gltfx_rgba_premultiplied(gltfx_rgba color) noexcept {
    return gltfx_rgba{
        .r = color.r * color.a,
        .g = color.g * color.a,
        .b = color.b * color.a,
        .a = color.a,
    };
}

} // namespace glintfx
