// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/piece_refusal.hpp"
#include "draw2d/renderer_2d_impl.hpp"

// draw2d/renderer_2d_impl_shadows.cpp - R2D-SPRITE-SHADE H3b (docs/auditoria-api-sprite-shade.md,
// sec. 2 and 4.2): the shadow methods of the interior. A shadow is a quad of one color whose run
// takes the stencil passes of shadow_group.hpp, so the shadows of one segment darken a pixel at
// most once; what is checked and how it is kept is the one path of every quad call
// (renderer_2d_impl::quad_call_goes_on).
namespace glintfx {

void renderer_2d_impl::fill_shadow(const gltfx_quad_world &shadow, gltfx_rgba tint,
                                   gltfx_draw_layer layer) noexcept {
    const draw2d::quad_corners_world world{shadow.top_left, shadow.top_right, shadow.bottom_right,
                                           shadow.bottom_left};
    draw2d::quad_corners_pixel pixel_corners{};
    if (!quad_call_goes_on(world, draw2d::refusal_of_quad(world, tint), pixel_corners)) {
        return;
    }
    if (!pieces.add_shadow(pixel_corners, tint, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
}

} // namespace glintfx
