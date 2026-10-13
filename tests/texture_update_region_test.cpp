// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>

#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_update_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_update_region_test.cpp - R2D-TEXTURE, fatia T3: the pure check of one update_texture()
// call (draw2d/texture_update_check.hpp) as a CLOSED table of named cells over a texture of 8 x 4
// texels: the rectangle at the edges (accepted) and one texel past them (refused), a corner whose
// sum wraps in 32 bits, a texture that is not open, is of another renderer or was not made
// updatable, the image checks, an image with no width or no height (accepted, with nothing to
// write, once the checks before it pass), and the order between them. The verdict of each cell is
// written from the rule of the header, never read from the code's output; an accepted cell also
// says the rectangle it must answer.

using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::texture_2d_impl;
using glintfx::draw2d::check_texture_update;
using glintfx::draw2d::texel_rect;
using glintfx::draw2d::texture_update_request;

namespace {

int this_renderer = 0;
int other_renderer = 0;

// Enough bytes for every view below: 8 x 4 texels of 4 bytes.
const std::array<std::byte, 128> k_bytes{};

const texture_2d_impl k_texture{
    .access = glintfx::draw2d::texture_gl_access{nullptr, nullptr, nullptr, &this_renderer},
    .gl_name = 9U,
    .width = 8U,
    .height = 4U,
    .filter = glintfx::gltfx_texture_filter::nearest,
    .reduced_levels = 0U,
    .updatable = true,
};

// The same texture, made without gltfx_texture_2d_desc::updatable.
const texture_2d_impl k_fixed{
    .access = glintfx::draw2d::texture_gl_access{nullptr, nullptr, nullptr, &this_renderer},
    .gl_name = 10U,
    .width = 8U,
    .height = 4U,
    .filter = glintfx::gltfx_texture_filter::nearest,
    .reduced_levels = 0U,
    .updatable = false,
};

[[nodiscard]] gltfx_image_view view_of(std::uint32_t width, std::uint32_t height) {
    return gltfx_image_view{.pixels = std::span<const std::byte>(k_bytes.data(), k_bytes.size()),
                            .width = width,
                            .height = height,
                            .bytes_per_row = static_cast<std::size_t>(width) * 4U,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

[[nodiscard]] gltfx_image_view with_format(gltfx_image_view view, gltfx_pixel_format format) {
    return gltfx_image_view{view.pixels, view.width, view.height, view.bytes_per_row, format};
}

[[nodiscard]] gltfx_image_view with_row(gltfx_image_view view, std::size_t bytes_per_row) {
    return gltfx_image_view{view.pixels, view.width, view.height, bytes_per_row, view.pixel_format};
}

[[nodiscard]] gltfx_image_view with_bytes(gltfx_image_view view, std::size_t count) {
    return gltfx_image_view{view.pixels.first(count), view.width, view.height, view.bytes_per_row,
                            view.pixel_format};
}

struct cell {
    const char *name = nullptr;
    const texture_2d_impl *texture = nullptr;
    const void *renderer = nullptr;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    gltfx_image_view image{};
    std::string_view refusal; // empty: accepted
    bool empty = false;       // when accepted: nothing to write
    texel_rect rect{};        // when accepted and not empty
};

[[nodiscard]] bool answers(const cell &row) {
    const auto got = check_texture_update(
        texture_update_request{row.texture, row.renderer, row.x, row.y, row.image});
    if (row.refusal.empty() && row.empty) {
        return got.has_value() && !got.value().has_value();
    }
    if (row.refusal.empty()) {
        return got.has_value() && got.value().has_value() &&
               got.value()->columns.first == row.rect.columns.first &&
               got.value()->columns.last == row.rect.columns.last &&
               got.value()->rows.first == row.rect.rows.first &&
               got.value()->rows.last == row.rect.rows.last;
    }
    return got.has_error() && got.err().code() == gltfx_err_code::invalid_argument &&
           got.err().rejected_value() == row.refusal;
}

} // namespace

GLINTFX_TEST(texture_update_region_closed_table_of_the_cells) {
    const void *mine = &this_renderer;
    const void *other = &other_renderer;
    const gltfx_pixel_format unspecified = gltfx_pixel_format::unspecified;
    const std::array<cell, 21> cells{{
        {"1 a textura inteira",
         &k_texture,
         mine,
         0U,
         0U,
         view_of(8U, 4U),
         "",
         false,
         {{0U, 7U}, {0U, 3U}}},
        {"2 encostado no canto de baixo a direita",
         &k_texture,
         mine,
         6U,
         3U,
         view_of(2U, 1U),
         "",
         false,
         {{6U, 7U}, {3U, 3U}}},
        {"3 um texel alem da direita",
         &k_texture,
         mine,
         7U,
         0U,
         view_of(2U, 1U),
         "rect",
         false,
         {}},
        {"4 um texel alem de baixo", &k_texture, mine, 0U, 4U, view_of(1U, 1U), "rect", false, {}},
        {"5 x que da a volta em 32 bits",
         &k_texture,
         mine,
         0xFFFFFFFFU,
         0U,
         view_of(2U, 1U),
         "rect",
         false,
         {}},
        {"6 alca sem textura", nullptr, mine, 0U, 0U, view_of(1U, 1U), "texture", false, {}},
        {"7 textura de outro desenhador",
         &k_texture,
         other,
         0U,
         0U,
         view_of(1U, 1U),
         "texture",
         false,
         {}},
        {"8 textura feita sem updatable",
         &k_fixed,
         mine,
         0U,
         0U,
         view_of(1U, 1U),
         "updatable",
         false,
         {}},
        {"9 largura zero: aceita, nada a escrever", &k_texture, mine, 0U, 0U, view_of(0U, 1U), "",
         true},
        {"10 altura zero: aceita, nada a escrever", &k_texture, mine, 0U, 0U, view_of(1U, 0U), "",
         true},
        {"11 largura zero na borda direita (x igual a largura)",
         &k_texture,
         mine,
         8U,
         0U,
         view_of(0U, 1U),
         "",
         true,
         {}},
        {"12 largura zero um texel alem da borda",
         &k_texture,
         mine,
         9U,
         0U,
         view_of(0U, 1U),
         "rect",
         false,
         {}},
        {"13 altura zero com x que da a volta em 32 bits",
         &k_texture,
         mine,
         0xFFFFFFFFU,
         0U,
         view_of(1U, 0U),
         "rect",
         false,
         {}},
        {"14 formato nao dito",
         &k_texture,
         mine,
         0U,
         0U,
         with_format(view_of(1U, 1U), unspecified),
         "pixel_format",
         false,
         {}},
        {"15 linha mais curta que a largura",
         &k_texture,
         mine,
         0U,
         0U,
         with_row(view_of(2U, 1U), 4U),
         "bytes_per_row",
         false,
         {}},
        {"16 bytes que nao cabem a imagem",
         &k_texture,
         mine,
         0U,
         0U,
         with_bytes(view_of(2U, 2U), 15U),
         "pixels",
         false,
         {}},
        {"17 outro desenhador antes de updatable",
         &k_fixed,
         other,
         0U,
         0U,
         view_of(1U, 1U),
         "texture",
         false,
         {}},
        {"18 updatable antes da vista",
         &k_fixed,
         mine,
         0U,
         0U,
         with_format(view_of(1U, 1U), unspecified),
         "updatable",
         false,
         {}},
        {"19 a vista antes do retangulo",
         &k_texture,
         mine,
         9U,
         0U,
         with_format(view_of(1U, 1U), unspecified),
         "pixel_format",
         false,
         {}},
        {"20 largura zero nao pula a vista",
         &k_texture,
         mine,
         0U,
         0U,
         with_format(view_of(0U, 1U), unspecified),
         "pixel_format",
         false,
         {}},
        {"21 altura zero nao pula updatable",
         &k_fixed,
         mine,
         0U,
         0U,
         view_of(1U, 0U),
         "updatable",
         false,
         {}},
    }};
    std::size_t failed = 0;
    for (const cell &row : cells) {
        if (!answers(row)) {
            ++failed;
            std::println("texture_update_region_test: FALHOU a celula {}", row.name);
        }
    }
    std::println("texture_update_region_test: celulas={} falhas={}", cells.size(), failed);
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}
