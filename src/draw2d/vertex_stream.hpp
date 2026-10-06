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
// THE LAYOUT of the vertex array is the one of batch_vertex (triangle_batch.hpp), 32 bytes:
// attribute 0 = position float2 at offset 0, attribute 1 = texture coordinate float2 at offset 8,
// attribute 2 = color float4 at offset 16, all float, not normalized, stride 32 - the locations
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

// Issues one draw call per run of `batch`, in order, and returns how many were issued. The array
// and the buffers must be bound (set_gl_state_for_drawing does it). The GL errors the calls left
// are read (at most k_max_gl_error_reads, D-B3c-2) and mapped: GL_OUT_OF_MEMORY is `out_of_memory`,
// anything else `platform_failure` with rejected_value() "draw" and the GL code in os_error_code().
// An empty batch issues nothing and reads nothing.
[[nodiscard]] glintfx::gltfx_rslt<std::uint64_t> draw_batch(const render::gl_function_table &gl,
                                                            const triangle_batch &batch) noexcept;

} // namespace glintfx::draw2d
