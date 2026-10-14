// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/quad_vertices.hpp"

// draw2d/triangle_batch.hpp - R2D-BATCH, fatia B2c (docs/plano-w7d.md sec. 4.3, D-W7D-18): the PURE
// atom that ASSEMBLES the batch of a frame as INDEXED TRIANGLES - the way SDL3
// (SDL_RenderGeometry), RmlUi 6 and raylib do - so that one mechanism serves the quad of today and
// the shapes and images that come after: the vertex format is fixed once, here. It knows nothing of
// GL or of the operating system, so it compiles and runs on all five systems; it includes nothing
// but core/ and the pure atoms of draw2d/ (GODS_LAWS.md L-19: src/draw2d/ never reaches
// src/platform/).
//
// THE VERTEX (36 bytes, R2D-TEXTURE T4a, D-W8-23 and D-W8-406): position float2 in PIXELS, texture
// coordinate float2, color float4 LINEAR and PREMULTIPLIED (the straight color of core/color.hpp
// times its alpha, alpha kept), and LAST the homogeneous coordinate q, so the texture coordinate
// stored is (u q, v q) and the program divides by q (the projective drawing of T4b; 1 until then).
// The premultiplication happens HERE, once per piece, so a half-transparent edge never grows a
// dark fringe; the shader does not repeat it. The texture coordinate is by ROLE, as core/quad.hpp
// names the corners: top_left (0, 0), top_right (1, 0), bottom_right (1, 1), bottom_left (0, 1);
// with no image yet the values are inert, and the textured drawing of T4b reads them.
//
// THE INDEX (32 bits): a batch is not capped at 16 384 pieces. A quad is 4 vertices and 6 indices,
// the two triangles of the frozen diagonal (docs/auditoria-api-draw2d.md B0-I4):
// (top_left, top_right, bottom_right) and (top_left, bottom_right, bottom_left).
//
// RUNS: pieces are added in painting order; CONSECUTIVE pieces with the same STATE KEY (program,
// texture, blend) are ONE run - one draw call - and a change in ANY of the three starts another.
// Reordering to merge more (sokol_gp) is a later unit, after textures exist.
//
// MEMORY, WITHOUT std::vector AND WITHOUT A FAILURE THAT THROWS (GODS_LAWS.md L-22, R3): the three
// buffers grow through a batch_allocator (realloc-shaped: null on failure, the old block
// untouched), so nothing here can throw. A piece is added WHOLE or not at all: every buffer it
// needs is grown BEFORE anything is written, so a failed allocation leaves the batch exactly as it
// was, and the piece is COUNTED as dropped (pieces_dropped_out_of_memory), which the frame report
// later carries.
namespace glintfx::draw2d {

struct batch_vertex {
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 0.0F;
    float q = 1.0F;
};
static_assert(sizeof(batch_vertex) == 36, "the vertex format is 36 bytes: 2 + 2 + 4 + 1 floats");

// What has to be equal for two pieces to share one draw call.
struct batch_state {
    std::uint32_t program = 0;
    std::uint32_t texture = 0;
    std::uint32_t blend = 0;
};

[[nodiscard]] constexpr bool operator==(const batch_state &first,
                                        const batch_state &second) noexcept {
    return first.program == second.program && first.texture == second.texture &&
           first.blend == second.blend;
}

// One vertex of a TRIANGLE piece (S2a): its position in PIXELS and its color, LINEAR and straight
// (add_triangles() premultiplies it, the same as add_quad()). The input of add_triangles() and of
// the triangle pieces of piece_list.
struct colored_pixel_vertex {
    gltfx_vec2_screen position{};
    glintfx::gltfx_rgba color{};
};

// One draw call: `index_count` indices starting at `first_index`, all with `state`.
struct draw_run {
    batch_state state;
    std::size_t first_index = 0;
    std::size_t index_count = 0;
};

class triangle_batch {
  public:
    explicit triangle_batch(batch_allocator allocator = default_batch_allocator()) noexcept;
    ~triangle_batch();
    triangle_batch(const triangle_batch &) = delete;
    triangle_batch &operator=(const triangle_batch &) = delete;
    triangle_batch(triangle_batch &&) = delete;
    triangle_batch &operator=(triangle_batch &&) = delete;

    // Adds one quad (its four corners in pixels, by role) with a straight LINEAR color, which is
    // stored premultiplied. True when the piece was added; false when memory ran out - the batch is
    // then unchanged and pieces_dropped_out_of_memory() has grown by one.
    [[nodiscard]] bool add_quad(const quad_corners_pixel &corners, glintfx::gltfx_rgba color,
                                batch_state state) noexcept;

    // Adds one TRIANGLE piece (S2a): `vertices` in pixels with a color each, and `local_indices`
    // into them (0..n-1). NO indices means 0..n-1 in order. The same atomicity as add_quad(): every
    // buffer the piece needs grows BEFORE anything is written. True when added; false when memory
    // ran out, with the batch unchanged and the piece counted as dropped.
    [[nodiscard]] bool add_triangles(std::span<const colored_pixel_vertex> vertices,
                                     std::span<const std::uint32_t> local_indices,
                                     batch_state state) noexcept;

    // Makes room, in one go, for `pieces` MORE pieces (4 vertices and 6 indices each, plus a
    // run each, the worst case), so the fill that follows allocates nothing: what the public
    // `reserve_pieces` hint asks for at open(). True when the room is there; false when memory ran
    // out or the size overflows - the batch's contents are then unchanged, and stores that did grow
    // stay (harmlessly). reserve(0) is a no-op that succeeds.
    [[nodiscard]] bool reserve(std::size_t pieces) noexcept;

    [[nodiscard]] std::span<const batch_vertex> vertices() const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;
    [[nodiscard]] std::span<const draw_run> runs() const noexcept;
    [[nodiscard]] std::size_t pieces_dropped_out_of_memory() const noexcept;

    // Empties the batch and KEEPS its capacity (the next fill allocates nothing). The dropped
    // count is kept too: it belongs to the frame, not to one fill.
    void clear() noexcept;

  private:
    // add_quad() in two steps (D-B7-1): the vertices and indices of one piece, and its run.
    void write_quad(const quad_corners_pixel &corners, glintfx::gltfx_rgba premultiplied) noexcept;
    // The triangles of one piece (S2a): the vertices and the indices, shifted by the base.
    void write_triangles(std::span<const colored_pixel_vertex> vertices,
                         std::span<const std::uint32_t> local_indices) noexcept;
    [[nodiscard]] bool make_room_for_triangles(std::size_t vertex_count, std::size_t index_count,
                                               bool starts_a_run) noexcept;
    void record_run(batch_state state, std::size_t first_index, std::size_t index_count,
                    bool starts_a_run) noexcept;

    batch_allocator allocator;
    pod_buffer<batch_vertex> vertex_store;
    pod_buffer<std::uint32_t> index_store;
    pod_buffer<draw_run> run_store;
    std::size_t dropped = 0;
};

} // namespace glintfx::draw2d
