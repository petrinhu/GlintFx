// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/arc_parameters.hpp"
#include "draw2d/ellipse_check.hpp"
#include "draw2d/ellipse_pixels.hpp"
#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_impl_ellipses.cpp - R2D-SHAPES, fatia S5-C1 (D-W8-565, D-W8-567): the ellipse methods
// of the frame machine, in a file of their own, as the textured methods have theirs (the line and
// the rectangle outline stay in renderer_2d_impl.cpp). A call is checked by the pure check of
// ellipse_check.hpp, turned into pixel triangles by ellipse_pixels.hpp on the screen of this frame
// (the batch transform in force, the surface read at begin_frame() and the curve distance given to
// create()), and kept as one piece through the head and the tail every shape call shares. This file
// only asks and keeps.
namespace glintfx {

draw2d::ellipse_screen renderer_2d_impl::ellipse_screen_now() const noexcept {
    return draw2d::ellipse_screen{
        affine,
        draw2d::pixel_box{0.0, 0.0, static_cast<double>(surface_width),
                          static_cast<double>(surface_height)},
        curve_max_error_pixels,
    };
}

void renderer_2d_impl::fill_ellipse(const gltfx_ellipse_world &ellipse, gltfx_rgba color,
                                    gltfx_draw_layer layer) noexcept {
    if (!shape_call_goes_on(draw2d::refusal_of_fill_ellipse(ellipse, color))) {
        return; // the whole ellipse, once
    }
    const bool built = ellipse_triangles.build_fill(ellipse, color, ellipse_screen_now()) ==
                       draw2d::ellipse_pixels_outcome::built;
    keep_shape_triangles(built, ellipse_triangles.vertices(), ellipse_triangles.indices(), layer);
}

void renderer_2d_impl::stroke_ellipse(const gltfx_ellipse_world &ellipse,
                                      const gltfx_stroke &stroke, gltfx_rgba color,
                                      gltfx_draw_layer layer) noexcept {
    if (!shape_call_goes_on(draw2d::refusal_of_stroke_ellipse(ellipse, stroke, color))) {
        return; // the whole outline, once
    }
    const bool built =
        ellipse_triangles.build_stroke(ellipse, stroke, color, ellipse_screen_now()) ==
        draw2d::ellipse_pixels_outcome::built;
    keep_shape_triangles(built, ellipse_triangles.vertices(), ellipse_triangles.indices(), layer);
}

} // namespace glintfx
