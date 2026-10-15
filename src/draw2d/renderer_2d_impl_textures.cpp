// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <optional>

#include "draw2d/projective_quad.hpp"
#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_region_check.hpp"
#include "draw2d/texture_region_uv.hpp"

// renderer_2d_impl_textures.cpp - R2D-TEXTURE, fatia T4b-B1 (D-W8-472, D-W8-474): the textured
// methods of the frame machine, in a file of their own as the renderer's texture family grew (the
// rest of the machine is renderer_2d_impl.cpp). A textured call is checked, then turned into pixel
// corners by the batch transform in force, like fill_quad(), and kept as ONE textured quad: its
// texture's GL name and where each corner reads it, stored times the projective weight q
// (triangle_batch.hpp). The checks are the pure ones of texture_region_check.hpp, asked in its
// order; the renderer adds the two only it can answer (is the texture open, is it ours).
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

// The reasons only the renderer can give, after the values passed: a texture that is not open, a
// texture of another renderer, and a source region outside the texture (its size is the texture's).
[[nodiscard]] gltfx_draw_2d_refusal
refusal_of_texture(const texture_2d_impl *texture, const void *renderer,
                   const gltfx_texture_region &region) noexcept {
    if (texture == nullptr) {
        return gltfx_draw_2d_refusal::texture_not_open;
    }
    if (texture->access.owner != renderer) {
        return gltfx_draw_2d_refusal::texture_other_renderer;
    }
    return draw2d::refusal_of_region_range(region.source,
                                           draw2d::texture_extent{texture->width, texture->height});
}

// Where each corner reads the texture, times its weight (1 at every corner of a rectangle).
[[nodiscard]] draw2d::quad_texture_corners
weighted_corners(const std::array<draw2d::texture_coordinate, 4> &reads,
                 const std::array<double, 4> &weights) noexcept {
    draw2d::quad_texture_corners corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        corners[corner] = draw2d::quad_texture_corner{static_cast<float>(reads[corner].u),
                                                      static_cast<float>(reads[corner].v),
                                                      static_cast<float>(weights[corner])};
    }
    return corners;
}

// Where each corner of a submission that passed its values reads its texture, weighted: the
// rectangle at q = 1, the quadrilateral at its projective weights. No value when the
// quadrilateral has no projective mapping (degenerate_quad). The region was checked against this
// texture before, so it always has coordinates (they are missing only for a mirroring this build
// does not know or a texture of no size, both refused before this point).
[[nodiscard]] std::optional<draw2d::quad_texture_corners>
texture_corners_of(const draw2d::textured_submission &submission) noexcept {
    const std::optional<std::array<double, 4>> weights =
        submission.projective ? draw2d::projective_weights(submission.corners)
                              : std::optional<std::array<double, 4>>{{1.0, 1.0, 1.0, 1.0}};
    const std::optional<std::array<draw2d::texture_coordinate, 4>> reads =
        draw2d::region_corner_coordinates(
            submission.paint->region,
            draw2d::texture_extent{submission.texture->width, submission.texture->height});
    if (!weights.has_value() || !reads.has_value()) {
        return std::nullopt;
    }
    return weighted_corners(*reads, *weights);
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
    const std::optional<draw2d::quad_texture_corners> reads = texture_corners_of(submission);
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

} // namespace glintfx
