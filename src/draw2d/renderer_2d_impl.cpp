// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/renderer_2d_impl.hpp"

#include <new>

#include <glintfx/core/err_code.hpp>

#include "draw2d/gl_state_contract.hpp"
#include "draw2d/srgb_encoding.hpp"

namespace glintfx {

namespace {

using render::GLenum;

constexpr GLenum k_gl_color_buffer_bit = 0x00004000;

// The room `reserve_pieces = 0` ("let the renderer choose") takes: a thousand pieces is 128 KiB of
// vertices, enough that an ordinary frame never grows storage, small enough to cost nothing to a
// program that draws little.
constexpr std::size_t k_default_reserve_pieces = 1024;

// The token of the step that failed to make the context current is the context's own error (passed
// on).

[[nodiscard]] draw2d::quad_corners_world corners_of(const gltfx_rect_world &rect) noexcept {
    const double left = rect.corner.x;
    const double top = rect.corner.y;
    const double right = rect.corner.x + rect.size.x;
    const double bottom = rect.corner.y + rect.size.y;
    return draw2d::quad_corners_world{gltfx_vec2_world{left, top}, gltfx_vec2_world{right, top},
                                      gltfx_vec2_world{right, bottom},
                                      gltfx_vec2_world{left, bottom}};
}

[[nodiscard]] draw2d::quad_corners_world corners_of(const gltfx_quad_world &quad) noexcept {
    return draw2d::quad_corners_world{quad.top_left, quad.top_right, quad.bottom_right,
                                      quad.bottom_left};
}

} // namespace

renderer_2d_impl::renderer_2d_impl(const renderer_2d_host &host_in,
                                   const render::gl_function_table &gl_in, bool srgb,
                                   draw2d::batch_allocator allocator) noexcept
    : host(host_in), gl(gl_in), batch(allocator), pieces(allocator), srgb_framebuffer(srgb) {}

gltfx_rslt<renderer_2d_impl *>
renderer_2d_impl::create(const renderer_2d_host &host, const render::gl_function_table &gl,
                         bool srgb_framebuffer, std::size_t reserve_pieces,
                         draw2d::batch_allocator allocator) noexcept {
    auto *impl = new (std::nothrow) renderer_2d_impl(host, gl, srgb_framebuffer, allocator);
    if (impl == nullptr) {
        return gltfx_rslt<renderer_2d_impl *>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    auto program = draw2d::create_embedded_program(impl->gl);
    if (program.has_error()) {
        delete impl;
        return gltfx_rslt<renderer_2d_impl *>::err(program.err());
    }
    impl->program = program.value();
    auto stream = draw2d::create_vertex_stream(impl->gl);
    if (stream.has_error()) {
        draw2d::destroy_embedded_program(impl->gl, impl->program);
        delete impl;
        return gltfx_rslt<renderer_2d_impl *>::err(stream.err());
    }
    impl->stream = stream.value();
    const std::size_t room = reserve_pieces == 0 ? k_default_reserve_pieces : reserve_pieces;
    if (!impl->batch.reserve(room) || !impl->pieces.reserve(room)) {
        draw2d::destroy_vertex_stream(impl->gl, impl->stream);
        draw2d::destroy_embedded_program(impl->gl, impl->program);
        delete impl;
        return gltfx_rslt<renderer_2d_impl *>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    return gltfx_rslt<renderer_2d_impl *>::ok(impl);
}

void renderer_2d_impl::destroy(renderer_2d_impl *impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    // "Releases the renderer's GPU objects in ITS context": make it current first; if that fails
    // there is nothing more to do about it (never refused), the objects go with the context.
    if (impl->host.make_current(impl->host.context).has_value()) {
        draw2d::destroy_vertex_stream(impl->gl, impl->stream);
        draw2d::destroy_embedded_program(impl->gl, impl->program);
    }
    delete impl;
}

bool renderer_2d_impl::context_current() noexcept {
    const gltfx_rslt<void> current = host.make_current(host.context);
    if (current.has_error()) {
        remember_error(current.err());
        return false;
    }
    return true;
}

void renderer_2d_impl::remember_error(const gltfx_err &error) noexcept {
    // The FIRST error of the frame is the one finish_frame() returns.
    if (!frame_error.has_value()) {
        frame_error = error;
    }
}

void renderer_2d_impl::begin_frame(const gltfx_frame_2d_desc &desc) noexcept {
    // A frame already open is thrown away, undrawn (frame_tally counts it as abandoned).
    batch.clear();
    pieces.clear();
    frame_error.reset();
    tally.begin_frame();
    frame_open = true;
    affine = draw2d::pixel_affine{}; // the pixel-direct batch every frame starts with

    gl_ready = context_current();
    if (!gl_ready) {
        return;
    }
    const auto size = host.surface_size(host.context);
    surface_width = size.first;
    surface_height = size.second;
    if (desc.clear_color.has_value()) {
        draw2d::set_gl_state_for_drawing(
            gl, draw2d::draw_state_request{program.program, stream.vertex_array,
                                           stream.vertex_buffer, stream.index_buffer, surface_width,
                                           surface_height, srgb_framebuffer});
        // With the surface encoding for us the clear color is written as it is (linear); with the
        // option off the surface holds ENCODED values and the color is encoded here, as the program
        // does for pieces.
        const gltfx_rgba color =
            srgb_framebuffer ? *desc.clear_color : draw2d::srgb_encode(*desc.clear_color);
        gl.glClearColor(color.red, color.green, color.blue, color.alpha);
        gl.glClear(k_gl_color_buffer_bit);
        draw2d::leave_gl_state_after_drawing(gl, surface_width, surface_height, srgb_framebuffer);
    }
}

void renderer_2d_impl::begin_batch() noexcept {
    affine = draw2d::pixel_affine{};
    tally.batch_began();
}

void renderer_2d_impl::begin_batch(const gltfx_transform &world_to_pixel) noexcept {
    affine = draw2d::pixel_affine_from_transform(world_to_pixel);
    tally.batch_began();
}

void renderer_2d_impl::submit(const draw2d::quad_corners_world &corners, gltfx_rgba color,
                              gltfx_draw_layer layer, gltfx_draw_2d_refusal refusal) noexcept {
    if (!frame_open) {
        tally.piece_outside_frame();
        return;
    }
    if (refusal != gltfx_draw_2d_refusal::none) {
        tally.piece_refused(refusal);
        return;
    }
    // Turned NOW, by the batch transform in force, in double precision: changing batch later never
    // moves a piece already submitted, and it costs no draw call.
    const draw2d::quad_corners_pixel pixel_corners = draw2d::quad_vertices(corners, affine);
    if (!pieces.add(pixel_corners, color, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
}

void renderer_2d_impl::fill_rect(const gltfx_rect_world &rect, gltfx_rgba color,
                                 gltfx_draw_layer layer) noexcept {
    submit(corners_of(rect), color, layer, draw2d::refusal_of_rect(rect, color));
}

void renderer_2d_impl::fill_quad(const gltfx_quad_world &corners, gltfx_rgba color,
                                 gltfx_draw_layer layer) noexcept {
    const draw2d::quad_corners_world world = corners_of(corners);
    submit(world, color, layer, draw2d::refusal_of_quad(world, color));
}

// The phases of a flush, each with a name of its own (D-B7-1, Extract Function): send_pending()
// below only sequences them.

void renderer_2d_impl::drop_pending_without_context() noexcept {
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        tally.piece_dropped_graphics_failure();
    }
    batch.clear();
    pieces.clear();
}

std::size_t renderer_2d_impl::batch_pending_in_paint_order() noexcept {
    // Paint order: (layer, submission). In place, allocates nothing, cannot fail (D-B4-2).
    pieces.sort();
    std::size_t added = 0;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        const draw2d::pending_piece &piece = pieces.painted(i);
        if (batch.add_quad(piece.corners, piece.color,
                           draw2d::batch_state{program.program, 0, 0})) {
            ++added;
        } else {
            tally.piece_dropped_out_of_memory();
        }
    }
    return added;
}

bool renderer_2d_impl::upload_and_draw_batch() noexcept {
    const gltfx_rslt<void> uploaded = draw2d::upload_batch(
        gl, stream, draw2d::vertex_upload_technique::orphan_and_sub_data, batch);
    if (uploaded.has_error()) {
        remember_error(uploaded.err());
        return false;
    }
    draw2d::set_gl_state_for_drawing(
        gl, draw2d::draw_state_request{program.program, stream.vertex_array, stream.vertex_buffer,
                                       stream.index_buffer, surface_width, surface_height,
                                       srgb_framebuffer});
    gl.glUniform2f(program.viewport_location, static_cast<float>(surface_width),
                   static_cast<float>(surface_height));
    gl.glUniform1i(program.encode_srgb_location, srgb_framebuffer ? 0 : 1);
    const gltfx_rslt<std::uint64_t> calls = draw2d::draw_batch(gl, batch);
    if (calls.has_error()) {
        remember_error(calls.err());
        return false;
    }
    tally.draw_calls_issued(calls.value());
    return true;
}

void renderer_2d_impl::settle_piece_counts(std::size_t added, bool drawn) noexcept {
    for (std::size_t i = 0; i < added; ++i) {
        if (drawn) {
            tally.piece_drawn();
        } else {
            tally.piece_dropped_graphics_failure();
        }
    }
}

void renderer_2d_impl::send_pending(bool explicit_barrier) noexcept {
    if (explicit_barrier) {
        tally.flushed();
    }
    if (!gl_ready) {
        drop_pending_without_context();
        return;
    }
    const std::size_t added = batch_pending_in_paint_order();
    const bool drawn = added > 0 && upload_and_draw_batch();
    settle_piece_counts(added, drawn);
    // The closed list of what is left, after every flush and at the end of the frame, even an empty
    // one.
    draw2d::leave_gl_state_after_drawing(gl, surface_width, surface_height, srgb_framebuffer);
    batch.clear();
    pieces.clear();
}

void renderer_2d_impl::flush() noexcept {
    if (!frame_open) {
        return;
    }
    gl_ready = context_current() && gl_ready;
    send_pending(true);
}

gltfx_rslt<gltfx_frame_2d_report> renderer_2d_impl::finish_frame() noexcept {
    if (!frame_open) {
        return gltfx_rslt<gltfx_frame_2d_report>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("frame"));
    }
    gl_ready = context_current() && gl_ready;
    send_pending(false);
    const draw2d::finish_status status = tally.finish();
    frame_open = false;
    const gltfx_frame_2d_report report = tally.last_report();
    // Precedence: the context's or the card's own error first, then the pieces dropped for lack of
    // memory.
    if (frame_error.has_value()) {
        return gltfx_rslt<gltfx_frame_2d_report>::err(*frame_error);
    }
    if (status == draw2d::finish_status::out_of_memory) {
        return gltfx_rslt<gltfx_frame_2d_report>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    return gltfx_rslt<gltfx_frame_2d_report>::ok(report);
}

gltfx_frame_2d_report renderer_2d_impl::last_frame_report() const noexcept {
    return tally.last_report();
}

} // namespace glintfx
