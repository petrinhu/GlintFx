// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

// draw2d/texture_region.hpp - R2D-TEXTURE. A part of a
// texture (in TEXELS, the texture's own pixels), how it is mirrored, and
// the vertex that carries texel coordinates to the textured triangles.
//
// TEXEL COORDINATES are their own type, not world and not screen
// (GODS_LAWS.md L-19): a world rectangle passed where a texel rectangle
// is expected must not compile. Double precision, as the world is, so a
// source rectangle can start between two texels.
//
// MIRRORING is a named value, never a sign: `flip_left_right` swaps the
// left and right edges of the source, `flip_top_bottom` the top and bottom,
// `flip_left_right_and_top_bottom` both: the name says what the reader sees.
// A value outside this list is refused for the whole call, counted as
// `unknown_option`.
//
// Proved by: texture_region_uv_test (each mirror value puts the corners of
// the source on the corner roles of core/quad.hpp; and, at compile time, a
// world vector or rectangle neither converts to nor builds a texel one,
// nor the other way round).
// Proved by: draw2d_texture_parity_test (each mirror value read back as
// pixels from a real surface: every texel of a 2 x 2 texture in the quarter
// the mirror puts it).

namespace glintfx {

struct gltfx_vec2_texel {
    double x = 0.0;
    double y = 0.0;
};

struct gltfx_rect_texel {
    gltfx_vec2_texel corner{};
    gltfx_vec2_texel size{};
};

enum class gltfx_texture_mirror : std::uint8_t {
    none = 0,
    flip_left_right = 1,
    flip_top_bottom = 2,
    flip_left_right_and_top_bottom = 3,
};

// A region of a texture, as a VALUE: the rectangle in texels and the
// mirroring. It holds NO reference to the texture. The texture comes in each
// drawing call, as in SDL_RenderTexture, so a region can never point to a
// texture that was released. Copying a region copies four numbers (the
// corner and the size) and an enumerator (the mirroring).
//
// A texture that is not open, or that belongs to another renderer, is refused
// by the drawing call (`texture_not_open`, `texture_other_renderer`). A
// region whose origin or size is not a finite number is refused as
// `not_a_number` or `infinite`, one with a negative width or height as
// `negative_size`, and one that reaches outside the texture as
// `region_out_of_range`.
struct gltfx_texture_region {
    gltfx_rect_texel source{};
    gltfx_texture_mirror mirror = gltfx_texture_mirror::none;
};

// What a textured drawing call paints with, grouped (D-W8-354): the region and
// the tint. Grouping keeps each drawing call within four parameters. The
// tint you do not choose is opaque white, which leaves the texture as it is
// (D-W8-464).
// Proved by: draw2d_texture_parity_test (on a real context, in both values
// of srgb_framebuffer: a white texel tinted white with alpha 0.5 over black,
// and an opaque red texel filtered next to a transparent one, read back at
// the value the formula gives, the red with green and blue exactly zero: no
// dark halo).
struct gltfx_texture_paint {
    gltfx_texture_region region{};
    gltfx_rgba tint{1.0F, 1.0F, 1.0F, 1.0F};
};

// The vertex of the textured triangles. Position in WORLD space, like
// every world position; `tint` is linear light with straight alpha, as
// every gltfx_rgba. The library multiplies the premultiplied texel by the
// tint in two parts: the colour by `tint` times `tint.alpha`, and the alpha
// by `tint.alpha`. So a tint of alpha 0.5 over a texel of partial alpha
// gives the right colour and the right alpha, not a dark halo.
// `texel` is in texel units of the texture being drawn. The tint you do not
// choose is opaque white, as in gltfx_texture_paint (D-W8-464).
struct gltfx_textured_vertex_2d {
    gltfx_vec2_world position{};
    gltfx_rgba tint{1.0F, 1.0F, 1.0F, 1.0F};
    gltfx_vec2_texel texel{};
};

} // namespace glintfx
