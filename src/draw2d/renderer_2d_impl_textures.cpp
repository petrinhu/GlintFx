// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_extent.hpp"
#include "draw2d/texture_region_check.hpp"
#include "draw2d/textured_quad_corners.hpp"
#include "draw2d/triangle_list_check.hpp"

// renderer_2d_impl_textures.cpp - R2D-TEXTURE, fatia T4b-B1 (D-W8-472, D-W8-474): the textured
// methods of the frame machine, in a file of their own as the renderer's texture family grew (the
// rest of the machine is renderer_2d_impl.cpp). A textured call is checked, then turned into pixel
// corners by the batch transform in force, like fill_quad(), and kept as ONE textured quad: its
// texture's GL name and where each corner reads it, stored times the projective weight q
// (triangle_batch.hpp). The checks are the pure ones of texture_region_check.hpp, asked in its
// order; the renderer adds the two only it can answer (is the texture open, is it ours). Where the
// corners read the texture is textured_quad_corners.hpp's (T4b-B1-fix1, D-W8-506): this file only
// refuses and keeps. The textured triangles (T5) are kept the same way, as one textured triangle
// piece: the checks are triangle_list_check.hpp's, and where each vertex reads the texture is
// triangle_pixel_vertices.hpp's.
namespace glintfx {

namespace {

[[nodiscard]] gltfx_quad_world quad_of(const gltfx_rect_world &rect) noexcept {
    const double left = rect.corner.x;
    const double top = rect.corner.y;
    const double right = rect.corner.x + rect.size.x;
    const double bottom = rect.corner.y + rect.size.y;
    return gltfx_quad_world{gltfx_vec2_world{left, top}, gltfx_vec2_world{right, top},
                            gltfx_vec2_world{right, bottom}, gltfx_vec2_world{left, bottom}};
}

// The two reasons only the renderer can give about the texture itself: not open, then another
// renderer's.
[[nodiscard]] gltfx_draw_2d_refusal refusal_of_texture_owner(const texture_2d_impl *texture,
                                                             const void *renderer) noexcept {
    if (texture == nullptr) {
        return gltfx_draw_2d_refusal::texture_not_open;
    }
    if (texture->access.owner != renderer) {
        return gltfx_draw_2d_refusal::texture_other_renderer;
    }
    return gltfx_draw_2d_refusal::none;
}

[[nodiscard]] draw2d::texture_extent extent_of(const texture_2d_impl &texture) noexcept {
    return draw2d::texture_extent{texture.width, texture.height};
}

// The reasons only the renderer can give, after the values passed: a texture that is not open, a
// texture of another renderer, and a source region outside the texture (its size is the texture's).
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_texture(const texture_2d_impl *texture, const void *renderer,
                   const gltfx_texture_region &region) noexcept {
    const gltfx_draw_2d_refusal by_owner = refusal_of_texture_owner(texture, renderer);
    if (by_owner != gltfx_draw_2d_refusal::none) {
        return by_owner;
    }
    return draw2d::refusal_of_region_range(region.source, extent_of(*texture));
}

// The reasons of a textured triangle call, in order (T5, D-W8-673): its own (the counts, the
// indices, the values with the texel coordinates), then the texture's, then the texel range.
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_textured_triangles(const draw2d::textured_triangle_call &call,
                              const void *renderer) noexcept {
    gltfx_draw_2d_refusal refusal =
        draw2d::refusal_of_textured_triangle_call(call.vertices, call.indices, call.indexed);
    if (refusal == gltfx_draw_2d_refusal::none) {
        refusal = refusal_of_texture_owner(call.texture, renderer);
    }
    if (refusal == gltfx_draw_2d_refusal::none) {
        refusal = draw2d::refusal_of_texel_range(call.vertices, extent_of(*call.texture));
    }
    return refusal;
}

} // namespace

void renderer_2d_impl::draw_texture(const texture_2d_impl *texture,
                                    const gltfx_texture_paint &paint, const gltfx_rect_world &rect,
                                    gltfx_draw_layer layer) noexcept {
    gltfx_draw_2d_refusal refusal = draw2d::refusal_of_texture_rect(paint, rect);
    if (refusal == gltfx_draw_2d_refusal::none) {
        refusal = refusal_of_texture(texture, this, paint.region);
    }
    submit_textured(draw2d::textured_submission{texture, &paint, quad_of(rect), false}, refusal,
                    layer);
}

void renderer_2d_impl::draw_texture_quad(const texture_2d_impl *texture,
                                         const gltfx_texture_paint &paint,
                                         const gltfx_quad_world &corners,
                                         gltfx_draw_layer layer) noexcept {
    gltfx_draw_2d_refusal refusal = draw2d::refusal_of_texture_quad(paint, corners);
    if (refusal == gltfx_draw_2d_refusal::none) {
        refusal = refusal_of_texture(texture, this, paint.region);
    }
    submit_textured(draw2d::textured_submission{texture, &paint, corners, true}, refusal, layer);
}

void renderer_2d_impl::submit_textured(const draw2d::textured_submission &submission,
                                       gltfx_draw_2d_refusal refusal,
                                       gltfx_draw_layer layer) noexcept {
    if (!frame_open) {
        tally.piece_outside_frame();
        return;
    }
    if (refusal != gltfx_draw_2d_refusal::none) {
        tally.piece_refused(refusal);
        return;
    }
    // The region was checked against this texture before, so it always has coordinates; no
    // corners here means a quadrilateral with no projective mapping, or with a weight of 2^63 or
    // more (textured_quad_corners.hpp).
    const std::optional<draw2d::quad_texture_corners> reads = draw2d::textured_quad_corners(
        submission.paint->region,
        draw2d::texture_extent{submission.texture->width, submission.texture->height},
        submission.corners, submission.projective);
    if (!reads.has_value()) {
        tally.piece_refused(gltfx_draw_2d_refusal::degenerate_quad);
        return;
    }
    const draw2d::quad_corners_world world{
        submission.corners.top_left, submission.corners.top_right, submission.corners.bottom_right,
        submission.corners.bottom_left};
    if (!draw2d::quad_in_pixel_range(world, affine)) {
        tally.piece_refused(gltfx_draw_2d_refusal::pixel_position_out_of_range);
        return;
    }
    // Turned NOW, by the batch transform in force, like fill_quad() (submit()).
    const draw2d::textured_quad_paint paint{submission.texture->gl_name, *reads,
                                            submission.paint->tint};
    if (!pieces.add_textured(draw2d::quad_vertices(world, affine), paint, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
}

void renderer_2d_impl::fill_textured_triangles(const texture_2d_impl *texture,
                                               std::span<const gltfx_textured_vertex_2d> vertices,
                                               gltfx_draw_layer layer) noexcept {
    submit_textured_triangles(draw2d::textured_triangle_call{texture, vertices, {}, false}, layer);
}

void renderer_2d_impl::fill_indexed_textured_triangles(
    const texture_2d_impl *texture, std::span<const gltfx_textured_vertex_2d> vertices,
    std::span<const std::uint32_t> indices, gltfx_draw_layer layer) noexcept {
    submit_textured_triangles(draw2d::textured_triangle_call{texture, vertices, indices, true},
                              layer);
}

void renderer_2d_impl::submit_textured_triangles(const draw2d::textured_triangle_call &call,
                                                 gltfx_draw_layer layer) noexcept {
    const draw2d::triangle_call_counts counts{
        call.vertices.size(), call.indexed ? call.indices.size() : std::size_t{0}, call.indexed};
    if (draw2d::is_empty_triangle_call(counts)) {
        return; // nothing to draw, and not a piece, whatever the texture (D-W8-287)
    }
    if (!frame_open) {
        tally.piece_outside_frame();
        return;
    }
    const gltfx_draw_2d_refusal refusal = refusal_of_textured_triangles(call, this);
    if (refusal != gltfx_draw_2d_refusal::none) {
        tally.piece_refused(refusal); // the whole call, once
        return;
    }
    keep_textured_triangle_call(call, layer);
}

void renderer_2d_impl::keep_textured_triangle_call(const draw2d::textured_triangle_call &call,
                                                   gltfx_draw_layer layer) noexcept {
    // Turned NOW, by the batch transform in force, like every triangle call (keep_triangle_call()).
    switch (triangle_pixels.convert_textured(call.vertices, affine, extent_of(*call.texture))) {
    case draw2d::triangle_pixels_outcome::pixel_position_out_of_range:
        tally.piece_refused(gltfx_draw_2d_refusal::pixel_position_out_of_range);
        return;
    case draw2d::triangle_pixels_outcome::out_of_memory:
        tally.piece_dropped_out_of_memory();
        return;
    case draw2d::triangle_pixels_outcome::converted:
        break;
    }
    const std::span<const std::uint32_t> local =
        call.indexed ? call.indices : std::span<const std::uint32_t>{};
    const draw2d::textured_triangle_piece piece{
        draw2d::textured_triangles{triangle_pixels.converted(), triangle_pixels.reads(), local},
        call.texture->gl_name};
    if (!pieces.add_textured_triangles(piece, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
}

} // namespace glintfx
