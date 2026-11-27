// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/color.hpp>

#include <algorithm>
#include <cmath>

#include "core/srgb_transfer.hpp"

// color.cpp - CORE-COLOR (TODO.md, GODS_LAWS.md L-17: one table/one
// function per concern, no switch growing a case per feature): the
// byte conversions gltfx_rgba_from_srgb8()/gltfx_rgba_to_srgb8(),
// which apply the sRGB transfer curve of core/srgb_transfer.hpp
// (IEC 61966-2-1, shared with draw2d since R2D-TEXTURE T1-fix1) per
// COLOR channel. Alpha never goes through either function here - see
// color.hpp's own header comment for why.
//
// color_test.cpp is the TDD red/green witness for every function in
// this file (GODS_LAWS.md L-20): the round trip over all 256 byte
// values, the linear-vs-encoded semantic check that is decision 3's
// whole reason to exist, and the headroom-clamp check for decision 1.

namespace glintfx {

namespace {

constexpr float k_byte_max = 255.0F;

// Linear light -> display-encoded, ONE channel, under this file's range policy: clamp to [0, 1]
// FIRST. This is the one lossy step decision 6 accepts for values outside what the display's 8-bit
// format can represent at all (see this function's call sites in gltfx_rgba_to_srgb8() below).
float linear_channel_to_srgb(float linear) {
    return linear_channel_to_srgb_unclamped(std::clamp(linear, 0.0F, 1.0F));
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
        .red = srgb_channel_to_linear(static_cast<float>(encoded.red) / k_byte_max),
        .green = srgb_channel_to_linear(static_cast<float>(encoded.green) / k_byte_max),
        .blue = srgb_channel_to_linear(static_cast<float>(encoded.blue) / k_byte_max),
        .alpha = static_cast<float>(encoded.alpha) / k_byte_max,
    };
}

gltfx_rgba8 gltfx_rgba_to_srgb8(gltfx_rgba color) noexcept {
    return gltfx_rgba8{
        .red = unit_to_byte(linear_channel_to_srgb(color.red)),
        .green = unit_to_byte(linear_channel_to_srgb(color.green)),
        .blue = unit_to_byte(linear_channel_to_srgb(color.blue)),
        .alpha = unit_to_byte(color.alpha),
    };
}

// Decision 6's second half. Straight multiplication, no clamp: unlike
// gltfx_rgba_to_srgb8() above, this never has to fit into 8 bits -
// the result is still a gltfx_rgba, still allowed decision 1's own
// headroom above white.
gltfx_rgba gltfx_rgba_premultiplied(gltfx_rgba color) noexcept {
    return gltfx_rgba{
        .red = color.red * color.alpha,
        .green = color.green * color.alpha,
        .blue = color.blue * color.alpha,
        .alpha = color.alpha,
    };
}

} // namespace glintfx
