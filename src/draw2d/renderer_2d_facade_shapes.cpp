// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>
#include <cstdint>
#include <span>

#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_facade_shapes.cpp - R2D-SHAPES, fatia S2b (D-W8-18): the public methods of the SHAPE
// family, in their own file, opened the first time the family gained a method (the facade of the
// rectangles and quadrilaterals stays in renderer_2d_facade.cpp). Each method is a forward to the
// frame machine (renderer_2d_impl.hpp), with the same moved-from precondition as the rest of the
// facade.
namespace glintfx {

void gltfx_renderer_2d::fill_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                       gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr &&
           "gltfx_renderer_2d::fill_triangles() called on a moved-from renderer - "
           "the object no longer owns its drawing state");
    impl->fill_triangles(vertices, layer);
}

void gltfx_renderer_2d::fill_indexed_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                               std::span<const std::uint32_t> indices,
                                               gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::fill_indexed_triangles() called on a moved-from "
                              "renderer - the object no longer owns its drawing state");
    impl->fill_indexed_triangles(vertices, indices, layer);
}

} // namespace glintfx
