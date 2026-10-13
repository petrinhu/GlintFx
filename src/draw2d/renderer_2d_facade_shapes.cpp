// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>
#include <cstdint>
#include <span>

#include <glintfx/core/segment.hpp>
#include <glintfx/draw2d/stroke.hpp>

#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_facade_shapes.cpp - R2D-SHAPES, fatias S2b to S4 (D-W8-18): the public methods of
// the SHAPE family, in their own file, opened the first time the family gained a method (the facade
// of the rectangles and quadrilaterals stays in renderer_2d_facade.cpp). Each method is a forward
// to the frame machine (renderer_2d_impl.hpp), with the same moved-from precondition as the rest of
// the facade.
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

// cppcheck-suppress-begin passedByValue
// reason: gltfx_segment_world and gltfx_line_style are VALUE types, and stroke_line() takes them by
// value as docs/auditoria-api-shapes.md fixes (D-W8-354): the copy is a few doubles, and a const
// reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::stroke_line(gltfx_segment_world segment, gltfx_line_style style,
                                    gltfx_rgba color, gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::stroke_line() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->stroke_line(segment, style, color, layer);
}
// cppcheck-suppress-end passedByValue

// cppcheck-suppress-begin passedByValue
// reason: gltfx_rect_world and gltfx_rect_outline_style are VALUE types, and stroke_rect() takes
// them by value as docs/auditoria-api-shapes.md fixes (D-W8-354): the copy is a few doubles, and a
// const reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::stroke_rect(gltfx_rect_world rect, gltfx_rect_outline_style style,
                                    gltfx_rgba color, gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::stroke_rect() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->stroke_rect(rect, style, color, layer);
}
// cppcheck-suppress-end passedByValue

} // namespace glintfx
