// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>

#include "draw2d/triangle_batch.hpp"
#include "gl_abi.hpp"
#include "gl_functions.hpp"

// draw2d/vertex_stream.hpp - R2D-BATCH, fatia B3c (docs/plano-w7d.md sec. 4.3, R-B6, D-W7D-18): the
// GL objects that carry a triangle_batch to the graphics card - one vertex array, one vertex
// buffer, one index buffer - and the three things done with them: create, upload, draw. Over an
// already loaded gl_function_table, no context and no operating system, so it compiles on all five
// systems and is exercised against a table of fakes (tests/vertex_stream_test.cpp).
//
// THE LAYOUT of the vertex array is the one of batch_vertex (triangle_batch.hpp), 36 bytes:
// attribute 0 = position float2 at offset 0, attribute 1 = texture coordinate float2 at offset 8,
// attribute 2 = color float4 at offset 16, attribute 3 = the homogeneous coordinate q float at
// offset 32 (T4a, D-W8-406), all float, not normalized, stride 36 - the locations
// embedded_program.hpp names. Indices are GL_UNSIGNED_INT.
//
// THE TWO TECHNIQUES of sending a batch (R-B6: the one the layer uses is chosen by MEASUREMENT on
// the targets, never promised as a number, so both exist and the live cell prints the cost of
// each):
//   - orphan_and_sub_data: glBufferData(nullptr) to orphan the old storage (the driver need not
//   wait
//     for the card), then glBufferSubData with the bytes;
//   - map_range_invalidate: glMapBufferRange over the bytes with WRITE and INVALIDATE_BUFFER, copy,
//     glUnmapBuffer (a false answer means the storage was corrupted and is a failure).
// Either grows the storage first when the batch no longer fits (doubling, never shrinking).
//
// GL ERRORS (D-B3c-2, errata sec. 12; the SDL does the same pair, GL_ClearErrors and
// GL_CheckAllErrors): glGetError answers the FIRST error of a queue that also holds the CONSUMER's
// own, so on ENTRY the layer DRAINS the queue (create_vertex_stream and a non-empty upload_batch;
// at most k_max_gl_error_reads reads, the GL has few flags), and each error drained is SAID, never
// swallowed and never blamed on us: an event `draw2d_prior_gl_error` (category "draw2d", severity
// warn) with the unsigned field `gl_error`. On EXIT it reads ALL the errors of the operation (with
// the same limit) and maps by the code: GL_OUT_OF_MEMORY (0x0505) is `out_of_memory`, anything else
// `platform_failure`; either way rejected_value() is the TOKEN of the step (vertex_array_create,
// vertex_upload, index_upload) and os_error_code() the GL code. A glGen* that answers 0 with
// nothing in the queue (a lost or not-current context) is `platform_failure` with the token of the
// step - glGen* allocates no storage, so it is never out_of_memory; a mapping that came back null
// or corrupted is `platform_failure` with the token of its buffer and no code. Nothing is thrown,
// and a failed create leaves no GL object alive.
namespace glintfx::draw2d {

enum class vertex_upload_technique : std::uint8_t {
    orphan_and_sub_data,
    map_range_invalidate,
};

inline constexpr std::string_view k_reject_vertex_array_create = "vertex_array_create";
inline constexpr std::string_view k_reject_vertex_upload = "vertex_upload";
inline constexpr std::string_view k_reject_index_upload = "index_upload";
inline constexpr std::string_view k_reject_draw = "draw";

inline constexpr std::string_view k_prior_gl_error_event = "draw2d_prior_gl_error";
inline constexpr std::string_view k_gl_error_field = "gl_error";
// The most queue reads one drain or one check makes: a GL that never empties its queue cannot hold
// us.
inline constexpr int k_max_gl_error_reads = 16;

struct vertex_stream {
    std::uint32_t vertex_array = 0;
    std::uint32_t vertex_buffer = 0;
    std::uint32_t index_buffer = 0;
    std::size_t vertex_capacity_bytes = 0;
    std::size_t index_capacity_bytes = 0;
};

// Creates the three objects and describes the layout. Leaves the array, the buffers bound to
// nothing (all zero), as the closed list of gl_state_contract.hpp says.
[[nodiscard]] glintfx::gltfx_rslt<vertex_stream>
create_vertex_stream(const render::gl_function_table &gl) noexcept;

// Deletes the three objects (harmless on an empty stream) and empties it.
void destroy_vertex_stream(const render::gl_function_table &gl, vertex_stream &stream) noexcept;

// Sends the vertices and indices of `batch` to the buffers. An empty batch sends nothing and calls
// no GL function. Leaves the stream's array bound (the caller sets the state next; see the
// contract).
[[nodiscard]] glintfx::gltfx_rslt<void> upload_batch(const render::gl_function_table &gl,
                                                     vertex_stream &stream,
                                                     vertex_upload_technique technique,
                                                     const triangle_batch &batch) noexcept;

// Where draw_batch() finds the program's two integer uniforms it sets (embedded_program.hpp):
// u_textured (T4b-B1) and u_stencil_discard (R2D-SPRITE-SHADE H3b). Grouped so the call stays
// within four parameters.
struct batch_uniforms {
    std::int32_t textured_location = -1;
    std::int32_t stencil_discard_location = -1;
};

// What one draw_batch() did (T4b-B1, D-W8-471): the draw calls it issued and how many times it
// changed the bound texture, counted where the change is made, so the report says what GL was
// told.
struct batch_draw_counts {
    std::uint64_t calls = 0;
    std::uint64_t texture_switches = 0;
    // One batch is one segment (shadow_group.hpp): 1 when a shadow run was drawn in it, else 0.
    std::uint64_t shadow_segments = 0;
};

// Issues the draw calls of every run of `batch`, in order, and says how many were issued. The
// array, the buffers and the program must be bound and the stencil test off with every color
// channel on (set_gl_state_for_drawing does it), unit 0 active, and the program's `u_textured` and
// `u_stencil_discard` (at the locations in `uniforms`) at 0, which the caller sets every flush. It
// starts as if no texture were bound: whatever the consumer left on unit 0 is never read, because
// a run with no texture samples nothing (`u_textured` 0) and a run with one binds its own. Before
// the draw call of a run whose texture differs from the last one it bound (0 is no texture), it
// binds the run's texture to unit 0 and sets `u_textured` to 1 for a texture, 2 for a shadow with a
// texture (a silhouette), 0 for none; that is one texture switch (renderer_2d.hpp: a change to or
// from no texture counts). A run of the same texture that asks for another value only sets the
// value, and it is not a switch.
// THE STENCIL (R2D-SPRITE-SHADE H3b): one batch is one SEGMENT of shadow_group.hpp, so each call
// starts with no shadow come. Each run is drawn with the passes passes_of_run() gives it, in
// order: the stencil cleared first when it asks (clear_stencil(), after the scissor is off), and
// for each pass the state of set_stencil_pass() and the `u_stencil_discard` of
// stencil_discard_of(), set only when the pass differs from the one GL is already set for, then one
// draw call. A batch with no shadow therefore calls no stencil function and sets no uniform for it.
// An ordinary run after the first shadow is two draw calls (paint, then release), and both count.
// The GL errors the calls left are read (at most k_max_gl_error_reads, D-B3c-2) and mapped:
// GL_OUT_OF_MEMORY is `out_of_memory`, anything else `platform_failure` with rejected_value()
// "draw" and the GL code in os_error_code(). An empty batch issues nothing and reads nothing.
[[nodiscard]] glintfx::gltfx_rslt<batch_draw_counts>
draw_batch(const render::gl_function_table &gl, const triangle_batch &batch,
           const batch_uniforms &uniforms) noexcept;

} // namespace glintfx::draw2d
