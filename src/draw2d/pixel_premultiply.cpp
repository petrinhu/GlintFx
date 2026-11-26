// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/pixel_premultiply.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>

#include "core/srgb_transfer.hpp"
#include "draw2d/srgb_encoding.hpp"

namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rgba8;
using glintfx::gltfx_rslt;

constexpr std::size_t k_bytes_per_pixel = 4;
constexpr float k_byte_max = 255.0F;

[[nodiscard]] bool is_known_format(gltfx_pixel_format format) noexcept {
    switch (format) {
    case gltfx_pixel_format::rgba8_srgb_straight:
    case gltfx_pixel_format::rgba8_srgb_premultiplied:
    case gltfx_pixel_format::rgba8_linear_straight:
    case gltfx_pixel_format::rgba8_linear_premultiplied:
        return true;
    case gltfx_pixel_format::unspecified:
        return false;
    }
    return false;
}

[[nodiscard]] bool is_srgb(gltfx_pixel_format format) noexcept {
    return format == gltfx_pixel_format::rgba8_srgb_straight ||
           format == gltfx_pixel_format::rgba8_srgb_premultiplied;
}

[[nodiscard]] bool is_premultiplied(gltfx_pixel_format format) noexcept {
    return format == gltfx_pixel_format::rgba8_srgb_premultiplied ||
           format == gltfx_pixel_format::rgba8_linear_premultiplied;
}

[[nodiscard]] gltfx_rslt<void> refused(std::string_view field) noexcept {
    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field));
}

[[nodiscard]] gltfx_rslt<void> check_format(gltfx_pixel_format format) noexcept {
    if (!is_known_format(format)) {
        return refused("pixel_format");
    }
    return gltfx_rslt<void>::ok();
}

[[nodiscard]] gltfx_rslt<void> check_stride(const gltfx_image_view &view) noexcept {
    const std::uint64_t row_bytes = static_cast<std::uint64_t>(view.width) * k_bytes_per_pixel;
    if (static_cast<std::uint64_t>(view.bytes_per_row) < row_bytes) {
        return refused("bytes_per_row");
    }
    return gltfx_rslt<void>::ok();
}

// Written as a division so that height * bytes_per_row cannot overflow: bytes_per_row fits in
// size / height exactly when height * bytes_per_row fits in size.
[[nodiscard]] gltfx_rslt<void> check_extent(const gltfx_image_view &view) noexcept {
    if (view.height != 0 && view.bytes_per_row > view.pixels.size() / view.height) {
        return refused("pixels");
    }
    return gltfx_rslt<void>::ok();
}

[[nodiscard]] gltfx_rslt<void> check_texel_count(const gltfx_image_view &view,
                                                 std::size_t texel_count) noexcept {
    const std::uint64_t needed = static_cast<std::uint64_t>(view.width) * view.height;
    if (static_cast<std::uint64_t>(texel_count) != needed) {
        return refused("texels");
    }
    return gltfx_rslt<void>::ok();
}

// The straight value of one channel in the encoding the caller wrote it in, in [0, 1].
// Premultiplied input is undone here, in that same encoding: channel / alpha, limited to 1.
[[nodiscard]] float straight_in_input_encoding(std::uint8_t channel, std::uint8_t alpha,
                                               bool premultiplied) noexcept {
    const float value = static_cast<float>(channel) / k_byte_max;
    if (!premultiplied) {
        return value;
    }
    if (alpha == 0) {
        return 0.0F;
    }
    return std::min(1.0F, value / (static_cast<float>(alpha) / k_byte_max));
}

// The clamp sits INSIDE the rounding call's argument, the shape GATE-LLROUND-ORDER accepts: the
// rounding never sees a value out of the byte range. Clamping first gives the same byte as rounding
// first, since both bounds are whole numbers.
[[nodiscard]] std::uint8_t to_byte(float scaled) noexcept {
    return static_cast<std::uint8_t>(std::round(std::clamp(scaled, 0.0F, k_byte_max)));
}

// One stored channel: the straight value taken to linear light, multiplied by alpha in linear
// light, then encoded and scaled to 8 bits.
[[nodiscard]] std::uint8_t stored_channel(std::uint8_t channel, std::uint8_t alpha,
                                          gltfx_pixel_format format) noexcept {
    const float straight = straight_in_input_encoding(channel, alpha, is_premultiplied(format));
    const float linear = is_srgb(format) ? glintfx::srgb_channel_to_linear(straight) : straight;
    const float premultiplied_linear = linear * (static_cast<float>(alpha) / k_byte_max);
    return stored_channel_byte(premultiplied_linear);
}

[[nodiscard]] gltfx_rgba8 stored_texel(const std::byte *pixel, gltfx_pixel_format format) noexcept {
    const auto byte_at = [pixel](std::size_t offset) {
        return std::to_integer<std::uint8_t>(pixel[offset]);
    };
    const std::uint8_t alpha = byte_at(3);
    return gltfx_rgba8{
        .red = stored_channel(byte_at(0), alpha, format),
        .green = stored_channel(byte_at(1), alpha, format),
        .blue = stored_channel(byte_at(2), alpha, format),
        .alpha = alpha,
    };
}

// Row by row, left to right. The stride and the span are already validated, so every pixel read
// lies inside the span, and a zero-width image never reads a byte.
void convert_rows(const gltfx_image_view &view, std::span<gltfx_rgba8> texels) noexcept {
    for (std::uint32_t row = 0; row < view.height; ++row) {
        for (std::uint32_t column = 0; column < view.width; ++column) {
            const std::byte *pixel = view.pixels.data() +
                                     static_cast<std::size_t>(row) * view.bytes_per_row +
                                     static_cast<std::size_t>(column) * k_bytes_per_pixel;
            texels[static_cast<std::size_t>(row) * view.width + column] =
                stored_texel(pixel, view.pixel_format);
        }
    }
}

} // namespace

gltfx_rslt<void> to_stored_texels(const gltfx_image_view &view,
                                  std::span<gltfx_rgba8> texels) noexcept {
    gltfx_rslt<void> checked = check_image_view(view);
    if (checked.has_error()) {
        return checked;
    }
    gltfx_rslt<void> counted = check_texel_count(view, texels.size());
    if (counted.has_error()) {
        return counted;
    }
    convert_rows(view, texels);
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> check_image_view(const gltfx_image_view &view) noexcept {
    gltfx_rslt<void> format = check_format(view.pixel_format);
    if (format.has_error()) {
        return format;
    }
    gltfx_rslt<void> stride = check_stride(view);
    if (stride.has_error()) {
        return stride;
    }
    return check_extent(view);
}

std::uint8_t stored_channel_byte(float premultiplied_linear) noexcept {
    return to_byte(srgb_encode(premultiplied_linear) * k_byte_max);
}

} // namespace glintfx::draw2d
