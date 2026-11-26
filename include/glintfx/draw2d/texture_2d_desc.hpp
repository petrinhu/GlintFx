// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/texture_2d_desc.hpp - R2D-TEXTURE. What the
// texture is sampled with, decided when the texture is created.
//
// APPEND-ONLY (GODS_LAWS.md L-26, the same growth rule as
// gltfx_gfx_option): a value is never renumbered and never reused, and
// this enum GROWS in later fatias (the pixel-art mode is a new value in
// R2D-SCALE-PIXEL-ART, W9-B). Do not write a `switch` over it that has no
// default: a value this build does not know is refused by name, never
// read as undefined behaviour (docs/api-conventions.md R4).
//
// Proved by: texture_desc_validation_test (a value outside the enum is
// refused with rejected_value() "filter").

namespace glintfx {

enum class gltfx_texture_filter : std::uint8_t {
    // Each screen pixel takes the colour of ONE texel, never a blend. Drawn
    // at its size or larger, or smaller without reduced levels: the nearest
    // texel of the full-size level; drawn smaller with reduced levels: the
    // nearest texel of the nearest level. Sharp edges: the right choice for
    // pixel art drawn at its own size.
    nearest = 0,
    // Each screen pixel blends texels. Drawn at its size or larger: the four
    // nearest texels of the full-size level. Drawn smaller: the same without
    // reduced levels; with them, the four nearest texels of each of the two
    // nearest levels, and then those two results.
    linear = 1,
};

// FILTERING AND COLOUR: a texture keeps sRGB-encoded colour that is already
// multiplied by alpha (core/image_view.hpp). With the linear filter, the
// graphics card turns each texel into linear light BEFORE it blends the
// neighbours, so an edge between two colours does not come out darker
// than it should. OpenGL lets a driver do the conversion after the blend
// instead; this library treats that as a driver defect, and the test below
// checks the order on every system it is tested on, failing when a driver
// converts after the blend.
// Proved by: draw2d_texture_parity_test

struct gltfx_texture_2d_desc {
    gltfx_texture_filter filter = gltfx_texture_filter::linear;
    // Number of REDUCED levels (mipmaps) below the full-size image, the
    // same count the atlas uses. Zero: no mipmap. k: levels 1 to k, each
    // half the size of the one above it, rounded down and never below one
    // texel. A texture of width w and height h allows at most
    // floor(log2(max(w, h))) of them (a 256 x 1 texture allows 8, a 1 x 1
    // texture none). gltfx_all_mipmap_levels (below) asks for all of them;
    // ANY OTHER count larger than that is refused by name
    // (invalid_argument, rejected_value() "mipmap_levels").
    // The reduced levels are computed by the library, not by the graphics
    // driver: each texel is the area average of the texels it covers in
    // the level above, in linear light, with the colour multiplied by
    // alpha.
    // Proved by: texture_desc_validation_test (the limit, at the boundary).
    // Proved by: mipmap_reduce_test (the average in linear light, and on
    // a side of odd length).
    std::uint32_t mipmap_levels = 0;
    // Whether update_texture() may change this texture. False, the
    // default: the texture is fixed, and update_texture() refuses it by
    // name (invalid_argument, rejected_value() "updatable"). True: the
    // texture can be updated and, with reduced levels, keeps the texels of
    // every level in the library's memory (on the CPU, besides whatever the
    // graphics driver holds), from its creation to its destruction, so
    // that an update recomputes the reduced texels it touches
    // (renderer_2d.hpp, update_texture()). That is 4 bytes for each texel
    // of levels 0 to k, k the reduced levels the texture has, level i
    // having max(1, width >> i) x max(1, height >> i) texels: at least
    // width x height x 4 bytes, and always less than twice that. It is
    // about 4/3 of it for a square texture with every level (1024 x 1024:
    // 1.333), nearly twice for a texture one texel high (256 x 1 with 8
    // levels: 511 texels, 2044 bytes, 1.996), and in between otherwise
    // (4096 x 2 with 12 levels: 1.5). A texture without reduced levels
    // keeps nothing either way.
    // Proved by: texture_update_test (the texels kept only by an updatable
    // texture with reduced levels, and how many a row and a square keep).
    // Proved by: texture_update_region_test (a texture made without it,
    // refused by name).
    bool updatable = false;
};

// The value that asks for every reduced level the texture size allows. It
// is the largest uint32, which is never a real count.
inline constexpr std::uint32_t gltfx_all_mipmap_levels = 0xFFFFFFFFU;

} // namespace glintfx
