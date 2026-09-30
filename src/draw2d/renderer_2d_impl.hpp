// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>

#include "draw2d/embedded_program.hpp"
#include "draw2d/frame_report_tally.hpp"
#include "draw2d/piece_list.hpp"
#include "draw2d/pod_buffer.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"
#include "draw2d/vertex_stream.hpp"
#include "gl_functions.hpp"

// draw2d/renderer_2d_impl.hpp - R2D-BATCH, fatia B4 (docs/auditoria-api-draw2d.md B0): the interior
// of the public handle gltfx_renderer_2d (renderer_2d.hpp declares it, opaque) - the frame machine
// that turns fill_rect()/fill_quad() into pieces, holds them until a flush, puts them in paint
// order, and sends them to the graphics card through the atoms of B2 and B3 (piece_list,
// triangle_batch, vertex_stream, gl_state_contract, embedded_program, frame_tally).
//
// It asks of its context only what renderer_2d_host says (make it current, how big is the surface),
// and it takes the GL functions as an already loaded table, so all of it - the whole frame machine
// - runs on all five systems against a fake host and a fake table (tests/renderer_2d_impl_test.cpp)
// with no window and no GL. The public facade (renderer_2d_facade.cpp) is the thin piece that
// builds the real host from a gltfx_gl_context.
//
// Nothing here includes a header of src/platform/ or of the operating system (GODS_LAWS.md L-19).
namespace glintfx {

// What the renderer asks of its context (D-API-07: it makes ITS OWN context current before it
// touches GL).
struct renderer_2d_host {
    void *context = nullptr;
    gltfx_rslt<void> (*make_current)(void *context) noexcept = nullptr;
    // The size of the surface in physical pixels {width, height}, read at begin_frame().
    std::pair<std::uint32_t, std::uint32_t> (*surface_size)(void *context) noexcept = nullptr;
};

// What create() is told besides the host and the GL functions (D-B7-1, Introduce Parameter Object):
// the surface encoding the context was opened with, how many pieces to make room for (0 lets the
// renderer choose), and the allocator of the piece and batch storage (a test arms its own).
struct renderer_2d_options {
    bool srgb_framebuffer = false;
    std::size_t reserve_pieces = 0;
    draw2d::batch_allocator allocator = draw2d::default_batch_allocator();
};

struct renderer_2d_impl {
    // The context is current (the caller made it so) and `gl` is loaded. Builds the program and the
    // vertex stream and reserves `reserve_pieces` pieces (0 lets the renderer choose). Refusals as
    // the header of renderer_2d.hpp lists them for open(); a failed create leaves no GL object
    // alive.
    [[nodiscard]] static gltfx_rslt<renderer_2d_impl *>
    create(const renderer_2d_host &host, const render::gl_function_table &gl,
           const renderer_2d_options &options) noexcept;

    // Makes the context current, deletes the GL objects, deletes `impl` (null is harmless). Never
    // refused.
    static void destroy(renderer_2d_impl *impl) noexcept;

    void begin_frame(const gltfx_frame_2d_desc &desc) noexcept;
    void begin_batch() noexcept;
    void begin_batch(const gltfx_transform &world_to_pixel) noexcept;
    void fill_rect(const gltfx_rect_world &rect, gltfx_rgba color, gltfx_draw_layer layer) noexcept;
    void fill_quad(const gltfx_quad_world &corners, gltfx_rgba color,
                   gltfx_draw_layer layer) noexcept;
    void flush() noexcept;
    [[nodiscard]] gltfx_rslt<gltfx_frame_2d_report> finish_frame() noexcept;
    [[nodiscard]] gltfx_frame_2d_report last_frame_report() const noexcept;

    renderer_2d_impl(const renderer_2d_impl &) = delete;
    renderer_2d_impl &operator=(const renderer_2d_impl &) = delete;

  private:
    renderer_2d_impl(const renderer_2d_host &host_in, const render::gl_function_table &gl_in,
                     bool srgb, draw2d::batch_allocator allocator) noexcept;
    ~renderer_2d_impl() = default;

    void submit(const draw2d::quad_corners_world &corners, gltfx_rgba color, gltfx_draw_layer layer,
                gltfx_draw_2d_refusal refusal) noexcept;
    // Sends the pending pieces to the card, in paint order, and leaves GL as the closed list says.
    // `explicit_barrier` is true for flush(), false for the implicit one of finish_frame().
    void send_pending(bool explicit_barrier) noexcept;
    // The phases send_pending() sequences (D-B7-1): the pieces of a frame with no current context
    // are dropped and counted; the rest go into the batch in paint order (how many entered is
    // returned), are uploaded and drawn (whether they were drawn is returned, and the first error
    // of the frame is kept), and every piece that entered is counted as drawn or as dropped by the
    // graphics side.
    void drop_pending_without_context() noexcept;
    [[nodiscard]] std::size_t batch_pending_in_paint_order() noexcept;
    [[nodiscard]] bool upload_and_draw_batch() noexcept;
    void settle_piece_counts(std::size_t added, bool drawn) noexcept;
    void remember_error(const gltfx_err &error) noexcept;
    [[nodiscard]] bool context_current() noexcept;

    renderer_2d_host host;
    render::gl_function_table gl;
    draw2d::embedded_program program;
    draw2d::vertex_stream stream;
    draw2d::triangle_batch batch;
    draw2d::piece_list pieces;
    draw2d::frame_tally tally;
    draw2d::pixel_affine affine;
    std::uint32_t surface_width = 0;
    std::uint32_t surface_height = 0;
    bool srgb_framebuffer = false;
    bool frame_open = false;
    bool gl_ready = false; // the context was made current for this frame
    std::optional<gltfx_err> frame_error;
};

} // namespace glintfx
