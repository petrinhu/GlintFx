// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// core/image_view.hpp - R2D-TEXTURE. A NON-OWNING view of
// pixels the caller already has in memory. Pure value type, no GL, no
// operating system: it lives in core/ next to gltfx_rgba8 (color.hpp),
// the 8-bit boundary type it describes.
//
// THE POINTER LIFETIME (docs/api-conventions.md R5): the library reads
// `pixels` only during the call that receives the view, and copies what
// it keeps. The caller owns the bytes and frees them; the library never
// frees them and never keeps the pointer.
//
// ROW STRIDE: `bytes_per_row` is the distance in BYTES from the first
// byte of one row to the first byte of the next. It may be larger than
// width * 4 (padded rows); it may not be smaller. A smaller value, or a
// `pixels` span shorter than height * bytes_per_row, is refused by the
// call that receives the view with invalid_argument, rejected_value()
// "bytes_per_row" or "pixels".
//
// PIXEL FORMAT: append-only (GODS_LAWS.md L-26). A value is never
// renumbered and never reused. Names are spelled out (GODS_LAWS.md
// L-28): the reader sees the color space and the alpha convention
// without opening this header. An 8-bit alpha-only format for glyph
// coverage is added later by appending a value, in R2D-TEXT.
// A value this build does not know is refused by name, never read as
// undefined behaviour (docs/api-conventions.md R4).

namespace glintfx {

enum class gltfx_pixel_format : std::uint8_t {
    // Not a format. It is the value of a view that does not say its format,
    // and it is REFUSED by name (invalid_argument, rejected_value()
    // "pixel_format"): a view without a format gets an error, never a wrong
    // colour.
    unspecified = 0,
    // Four 8-bit channels per pixel. Red, green and blue are sRGB-encoded
    // (the way a PNG or a camera stores them); alpha is straight, that
    // is, not multiplied into the colour.
    rgba8_srgb_straight = 1,
    // Same channels, sRGB-encoded, but the colour is ALREADY multiplied
    // by alpha in the encoding the caller chose.
    rgba8_srgb_premultiplied = 2,
    // Red, green and blue are linear light (the library's own convention,
    // core/color.hpp); alpha is straight.
    rgba8_linear_straight = 3,
    // Linear light, colour already multiplied by alpha.
    rgba8_linear_premultiplied = 4,
};

struct gltfx_image_view {
    // Read only during the call that receives this view (see above).
    std::span<const std::byte> pixels{};
    // Size in pixels. Zero is refused by the texture call, not here.
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Bytes from one row to the next (see above).
    std::size_t bytes_per_row = 0;
    // No default on purpose: the field must be written. The value
    // `unspecified` is refused by name.
    gltfx_pixel_format pixel_format = gltfx_pixel_format::unspecified;
};

} // namespace glintfx
