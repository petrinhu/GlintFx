// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>
#include <vector>

#include <glintfx/core/image_view.hpp>

#include "draw2d/atlas_extrude.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// atlas_extrude_test.cpp - R2D-TEXTURE, fatia T6-B1 (draw2d/atlas_extrude.hpp; GODS_LAWS.md L-17,
// L-20). The texels of one cell of the texture atlas, alone: atlas_extrude.cpp is compiled a second
// time into this test (tests/CMakeLists.txt). The space is small and closed (GODS_LAWS.md L-17
// global), so it is enumerated WHOLE: every image side from 1 to 3, every margin of 1, 2 and 4, and
// every rounding of the cell side to 1, 2 and 4. The oracle is the formula of texture_atlas.hpp,
// written here, never the program's output: cell texel (x, y) is image texel
// (clamp(x - margin, 0, w - 1), clamp(y - margin, 0, h - 1)). Every image texel carries its own
// coordinates in its bytes, and every row is padded with bytes 0xEE that must never reach the cell.

namespace {

using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::draw2d::atlas_cell_bytes;
using glintfx::draw2d::extrude_atlas_cell;

constexpr std::byte k_padding{0xEE};

// An image whose texel (x, y) is the bytes x, y, x + y, 0x80 + x, rows padded with one texel of
// k_padding.
struct coded_image {
    std::vector<std::byte> bytes;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t row_bytes = 0;
};

[[nodiscard]] coded_image coded(std::uint32_t width, std::uint32_t height) {
    const std::size_t row_bytes = (std::size_t{width} + 1U) * 4U;
    std::vector<std::byte> bytes(row_bytes * height, k_padding);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::byte *texel = bytes.data() + y * row_bytes + std::size_t{x} * 4U;
            texel[0] = static_cast<std::byte>(x);
            texel[1] = static_cast<std::byte>(y);
            texel[2] = static_cast<std::byte>(x + y);
            texel[3] = static_cast<std::byte>(0x80U + x);
        }
    }
    return coded_image{std::move(bytes), width, height, row_bytes};
}

[[nodiscard]] gltfx_image_view view_of(const coded_image &image, gltfx_pixel_format format) {
    return gltfx_image_view{.pixels = std::span<const std::byte>(image.bytes),
                            .width = image.width,
                            .height = image.height,
                            .bytes_per_row = image.row_bytes,
                            .pixel_format = format};
}

[[nodiscard]] std::uint32_t clamped(std::uint32_t position, std::uint32_t margin,
                                    std::uint32_t side) {
    const long inside = static_cast<long>(position) - static_cast<long>(margin);
    if (inside < 0) {
        return 0;
    }
    return inside >= static_cast<long>(side) ? side - 1U : static_cast<std::uint32_t>(inside);
}

[[nodiscard]] std::uint32_t rounded(std::uint32_t side, std::uint32_t multiple) {
    return (side + multiple - 1U) / multiple * multiple;
}

struct shape {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t margin = 0;
    std::uint32_t multiple = 1;
};

// How many texels of the cell of `s` differ from the formula (the cell is filled with 0x11 first,
// so a texel never written differs too).
[[nodiscard]] int wrong_texels(const shape &s) {
    const coded_image image = coded(s.width, s.height);
    const std::uint32_t cell_width = rounded(s.width + 2U * s.margin, s.multiple);
    const std::uint32_t cell_height = rounded(s.height + 2U * s.margin, s.multiple);
    std::vector<std::byte> cell(std::size_t{cell_width} * cell_height * 4U, std::byte{0x11});
    extrude_atlas_cell(view_of(image, gltfx_pixel_format::rgba8_srgb_straight), s.margin,
                       atlas_cell_bytes{cell, cell_width, cell_height});
    int wrong = 0;
    for (std::uint32_t y = 0; y < cell_height; ++y) {
        for (std::uint32_t x = 0; x < cell_width; ++x) {
            const std::uint32_t from_x = clamped(x, s.margin, s.width);
            const std::uint32_t from_y = clamped(y, s.margin, s.height);
            const std::byte *got = cell.data() + (std::size_t{y} * cell_width + x) * 4U;
            const std::byte *want =
                image.bytes.data() + from_y * image.row_bytes + std::size_t{from_x} * 4U;
            wrong +=
                (got[0] != want[0] || got[1] != want[1] || got[2] != want[2] || got[3] != want[3])
                    ? 1
                    : 0;
        }
    }
    return wrong;
}

} // namespace

GLINTFX_TEST(atlas_extrude_every_texel_of_the_cell_is_the_nearest_image_texel) {
    // The 81 shapes, one index each: width, height, margin and multiple, three values apiece.
    constexpr std::array<std::uint32_t, 3> sides{1U, 2U, 3U};
    constexpr std::array<std::uint32_t, 3> powers{1U, 2U, 4U};
    int shapes = 0;
    int wrong = 0;
    for (std::size_t i = 0; i < 81U; ++i) {
        wrong += wrong_texels(
            shape{sides[i % 3U], sides[i / 3U % 3U], powers[i / 9U % 3U], powers[i / 27U % 3U]});
        ++shapes;
    }
    std::printf("atlas_extrude_test: %d formas conferidas, %d texels errados\n", shapes, wrong);
    GLINTFX_CHECK(shapes == 81);
    GLINTFX_CHECK(wrong == 0);
}

GLINTFX_TEST(atlas_extrude_copies_the_bytes_as_they_are_in_the_image_format) {
    // A premultiplied linear image whose alpha is below its colour (not a valid premultiplied
    // texel): the cell must hold the very bytes, never a conversion of them.
    const coded_image image = coded(2, 1);
    std::vector<std::byte> cell(std::size_t{4} * 3U * 4U, std::byte{0x11});
    extrude_atlas_cell(view_of(image, gltfx_pixel_format::rgba8_linear_premultiplied), 1U,
                       atlas_cell_bytes{cell, 4U, 3U});
    // Row 1, texels 1 and 2 are the image's texels (0, 0) and (1, 0).
    const std::byte *first = cell.data() + (std::size_t{1} * 4U + 1U) * 4U;
    GLINTFX_CHECK(first[0] == std::byte{0} && first[1] == std::byte{0} &&
                  first[2] == std::byte{0} && first[3] == std::byte{0x80});
    const std::byte *second = first + 4U;
    GLINTFX_CHECK(second[0] == std::byte{1} && second[1] == std::byte{0} &&
                  second[2] == std::byte{1} && second[3] == std::byte{0x81});
}
