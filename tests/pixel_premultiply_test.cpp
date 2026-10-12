// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <print>
#include <utility>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>

#include "draw2d/pixel_premultiply.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// pixel_premultiply_test.cpp - R2D-TEXTURE, fatia T1: the conversion of a caller's image view into
// the texels the library stores (sRGB encoded color, straight alpha). Every expected value is
// written here as a literal taken from the formula of IEC 61966-2-1 in double precision (the table
// of the T1 order, checked by an independent script before it was typed in). Never read back from
// the code under test. A stored channel is accepted within one level (the reference is a real
// number, the texel an integer), except in the two exact cases at the end of this file, which check
// the rounding the header promises.

using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rgba8;
using glintfx::gltfx_rslt;
using glintfx::draw2d::to_stored_texels;

namespace {

// Padding bytes are nonzero on purpose: a row read from the wrong offset must not pass by accident.
constexpr std::byte k_padding{0xAB};

struct table_row {
    gltfx_rgba8 input;
    gltfx_pixel_format format;
    std::array<double, 4> expected; // stored texel: red, green, blue, alpha, on the 0..255 scale
};

[[nodiscard]] std::vector<std::byte> to_bytes(std::initializer_list<std::uint8_t> values) {
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const std::uint8_t value : values) {
        bytes.push_back(std::byte{value});
    }
    return bytes;
}

[[nodiscard]] gltfx_image_view view_over(const std::vector<std::byte> &pixels, std::uint32_t width,
                                         std::uint32_t height, std::size_t bytes_per_row,
                                         gltfx_pixel_format format) {
    return gltfx_image_view{.pixels = pixels,
                            .width = width,
                            .height = height,
                            .bytes_per_row = bytes_per_row,
                            .pixel_format = format};
}

[[nodiscard]] bool within_one_level(std::uint8_t stored, double expected) {
    return std::fabs(static_cast<double>(stored) - expected) <= 1.0;
}

// The caller of to_stored_texels owns the storage: the test sizes it to width * height, so only the
// view's own checks can refuse in the cases that use this helper.
[[nodiscard]] gltfx_rslt<std::vector<gltfx_rgba8>> convert_all(const gltfx_image_view &view) {
    std::vector<gltfx_rgba8> texels(static_cast<std::size_t>(view.width) * view.height);
    const gltfx_rslt<void> status = to_stored_texels(view, texels);
    if (status.has_error()) {
        return gltfx_rslt<std::vector<gltfx_rgba8>>::err(status.err());
    }
    return gltfx_rslt<std::vector<gltfx_rgba8>>::ok(std::move(texels));
}

} // namespace

GLINTFX_TEST(table_rows_match_the_formula_within_one_level_and_keep_alpha_exact) {
    const std::array<table_row, 10> rows{{
        {{255, 0, 0, 128}, gltfx_pixel_format::rgba8_srgb_straight, {187.845, 0.0, 0.0, 128.0}},
        {{128, 128, 128, 255},
         gltfx_pixel_format::rgba8_srgb_straight,
         {128.0, 128.0, 128.0, 255.0}},
        {{255, 255, 255, 0}, gltfx_pixel_format::rgba8_srgb_straight, {0.0, 0.0, 0.0, 0.0}},
        {{255, 0, 0, 128}, gltfx_pixel_format::rgba8_linear_straight, {187.845, 0.0, 0.0, 128.0}},
        {{128, 0, 0, 128},
         gltfx_pixel_format::rgba8_srgb_premultiplied,
         {187.845, 0.0, 0.0, 128.0}},
        {{128, 0, 0, 128},
         gltfx_pixel_format::rgba8_linear_premultiplied,
         {187.845, 0.0, 0.0, 128.0}},
        {{64, 128, 255, 64},
         gltfx_pixel_format::rgba8_srgb_straight,
         {29.837, 65.814, 137.207, 64.0}},
        {{188, 0, 0, 255}, gltfx_pixel_format::rgba8_linear_straight, {222.912, 0.0, 0.0, 255.0}},
        {{200, 0, 0, 100},
         gltfx_pixel_format::rgba8_srgb_premultiplied,
         {168.113, 0.0, 0.0, 100.0}},
        {{50, 0, 0, 0}, gltfx_pixel_format::rgba8_srgb_premultiplied, {0.0, 0.0, 0.0, 0.0}},
    }};
    for (const table_row &row : rows) {
        const std::vector<std::byte> pixels =
            to_bytes({row.input.red, row.input.green, row.input.blue, row.input.alpha});
        const auto result = convert_all(view_over(pixels, 1, 1, 4, row.format));
        GLINTFX_CHECK(result.has_value());
        const gltfx_rgba8 texel = result.value().front();
        GLINTFX_CHECK(within_one_level(texel.red, row.expected[0]));
        GLINTFX_CHECK(within_one_level(texel.green, row.expected[1]));
        GLINTFX_CHECK(within_one_level(texel.blue, row.expected[2]));
        GLINTFX_CHECK(static_cast<double>(texel.alpha) ==
                      row.expected[3]); // alpha is never encoded
    }
}

GLINTFX_TEST(unspecified_format_is_refused_by_name) {
    const std::vector<std::byte> pixels = to_bytes({255, 0, 0, 255});
    const auto result = convert_all(view_over(pixels, 1, 1, 4, gltfx_pixel_format::unspecified));
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.err().rejected_value() == "pixel_format");
}

GLINTFX_TEST(format_enumeration_is_closed_and_exactly_four_values_are_known) {
    const std::vector<std::byte> pixels = to_bytes({255, 0, 0, 255});
    std::size_t known = 0;
    for (std::uint32_t raw = 0; raw <= 255; ++raw) {
        const auto format = static_cast<gltfx_pixel_format>(static_cast<std::uint8_t>(raw));
        const auto result = convert_all(view_over(pixels, 1, 1, 4, format));
        if (raw >= 1 && raw <= 4) {
            GLINTFX_CHECK(result.has_value());
            ++known;
            continue;
        }
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
        GLINTFX_CHECK(result.err().rejected_value() == "pixel_format");
    }
    GLINTFX_CHECK(known == 4);
    std::println("pixel_premultiply_test: formatos={}", known);
}

GLINTFX_TEST(row_stride_smaller_than_the_row_is_refused_by_name) {
    const std::vector<std::byte> pixels(16, k_padding);
    const auto result =
        convert_all(view_over(pixels, 2, 2, 7, gltfx_pixel_format::rgba8_srgb_straight));
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.err().rejected_value() == "bytes_per_row");
}

GLINTFX_TEST(pixel_span_shorter_than_the_image_is_refused_by_name) {
    const std::vector<std::byte> pixels(15, k_padding); // 2 rows of 8 bytes need 16
    const auto result =
        convert_all(view_over(pixels, 2, 2, 8, gltfx_pixel_format::rgba8_srgb_straight));
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.err().rejected_value() == "pixels");
}

GLINTFX_TEST(padded_rows_are_read_at_the_stride_and_the_padding_is_ignored) {
    // Two rows of two pixels, each row 16 bytes: 8 bytes of pixels, then 8 bytes of padding.
    const std::vector<std::byte> pixels = {
        std::byte{255}, std::byte{0},   std::byte{0},   std::byte{128}, // row 0, pixel 0
        std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255}, // row 0, pixel 1
        k_padding,      k_padding,      k_padding,      k_padding,
        k_padding,      k_padding,      k_padding,      k_padding,
        std::byte{188}, std::byte{0},   std::byte{0},   std::byte{255}, // row 1, pixel 0
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{0},   // row 1, pixel 1
        k_padding,      k_padding,      k_padding,      k_padding,
        k_padding,      k_padding,      k_padding,      k_padding,
    };
    const auto result =
        convert_all(view_over(pixels, 2, 2, 16, gltfx_pixel_format::rgba8_srgb_straight));
    GLINTFX_CHECK(result.has_value());
    const std::vector<gltfx_rgba8> &texels = result.value();
    GLINTFX_CHECK(texels.size() == 4);
    GLINTFX_CHECK(within_one_level(texels[0].red, 187.845));
    GLINTFX_CHECK(texels[0].alpha == 128);
    GLINTFX_CHECK(within_one_level(texels[1].red, 128.0));
    GLINTFX_CHECK(texels[1].alpha == 255);
    GLINTFX_CHECK(
        within_one_level(texels[2].red, 188.0)); // opaque sRGB: the round trip is the identity
    GLINTFX_CHECK(texels[2].alpha == 255);
    GLINTFX_CHECK(texels[3].red == 0 && texels[3].alpha == 0);
}

GLINTFX_TEST(texel_storage_of_the_wrong_size_is_refused_by_name) {
    const std::vector<std::byte> pixels = to_bytes({255, 0, 0, 255, 255, 0, 0, 255}); // 2 pixels
    const gltfx_image_view view =
        view_over(pixels, 2, 1, 8, gltfx_pixel_format::rgba8_srgb_straight);
    std::vector<gltfx_rgba8> too_small(1);
    const gltfx_rslt<void> result = to_stored_texels(view, too_small);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.err().rejected_value() == "texels");
}

// The two EXACT cases (T1-fix1). The header promises that a stored channel is ROUNDED, and the
// one-level tolerance of the table above cannot see the difference between rounding and truncating
// (nor a decode threshold moved inside the linear toe). Both expected bytes come from the formula
// in double precision, never from the code under test: (1,0,0,255) lies on the linear toe of the
// curve and stores 1.000; (255,0,0,128) stores 187.845, which is 0.345 above the rounding boundary
// of 187.5, far beyond any float error, so the exact byte is the same on every compiler.
GLINTFX_TEST(linear_toe_of_the_curve_stores_the_exact_byte) {
    const std::vector<std::byte> pixels = to_bytes({1, 0, 0, 255});
    const auto result =
        convert_all(view_over(pixels, 1, 1, 4, gltfx_pixel_format::rgba8_srgb_straight));
    GLINTFX_CHECK(result.has_value());
    const gltfx_rgba8 texel = result.value().front();
    GLINTFX_CHECK(texel.red == 1);
    GLINTFX_CHECK(texel.green == 0 && texel.blue == 0 && texel.alpha == 255);
}

GLINTFX_TEST(stored_channel_is_rounded_to_the_nearest_byte) {
    const std::vector<std::byte> pixels = to_bytes({255, 0, 0, 128});
    const auto result =
        convert_all(view_over(pixels, 1, 1, 4, gltfx_pixel_format::rgba8_srgb_straight));
    GLINTFX_CHECK(result.has_value());
    const gltfx_rgba8 texel = result.value().front();
    GLINTFX_CHECK(texel.red == 188);
    GLINTFX_CHECK(texel.green == 0 && texel.blue == 0 && texel.alpha == 128);
}
