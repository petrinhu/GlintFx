// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/renderer_2d_impl.hpp"

#include <new>

#include <glintfx/core/err_code.hpp>

#include "draw2d/gl_state_contract.hpp"
#include "draw2d/piece_refusal.hpp"
#include "draw2d/srgb_encoding.hpp"
#include "draw2d/stroke_line_check.hpp"
#include "draw2d/stroke_rect_check.hpp"
#include "draw2d/texture_update.hpp"
#include "draw2d/texture_update_check.hpp"
#include "draw2d/triangle_list_check.hpp"

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
    : host(host_in), gl(gl_in), batch(allocator), pieces(allocator), srgb_framebuffer(srgb),
      triangle_pixels(allocator) {}

gltfx_rslt<renderer_2d_impl *>
renderer_2d_impl::create(const renderer_2d_host &host, const render::gl_function_table &gl,
                         const renderer_2d_options &options) noexcept {
    auto *impl =
        new (std::nothrow) renderer_2d_impl(host, gl, options.srgb_framebuffer, options.allocator);
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
    impl->curve_max_error_pixels = options.curve_max_error_pixels;
    const std::size_t room =
        options.reserve_pieces == 0 ? k_default_reserve_pieces : options.reserve_pieces;
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
    impl->held_textures.release_held(); // D-W8-288: before the renderer goes
    // "Releases the renderer's GPU objects in ITS context": make it current first; if that fails
    // there is nothing more to do about it (never refused), the objects go with the context.
    if (impl->host.make_current(impl->host.context).has_value()) {
        draw2d::destroy_vertex_stream(impl->gl, impl->stream);
        draw2d::destroy_embedded_program(impl->gl, impl->program);
    }
    delete impl;
}

draw2d::texture_gl_access renderer_2d_impl::texture_access() noexcept {
    return draw2d::texture_gl_access{
        host.context, host.make_current, &gl, this, &renderer_2d_impl::hold_texture_release, this};
}

bool renderer_2d_impl::hold_texture_release(void *renderer, texture_2d_impl *texture) noexcept {
    auto *self = static_cast<renderer_2d_impl *>(renderer);
    if (!self->frame_open) {
        return false;
    }
    self->held_textures.hold(texture);
    return true;
}

gltfx_rslt<void> renderer_2d_impl::update_texture(texture_2d_impl *texture, std::uint32_t x,
                                                  std::uint32_t y,
                                                  const gltfx_image_view &image) noexcept {
    const gltfx_rslt<std::optional<draw2d::texel_rect>> rect =
        draw2d::check_texture_update(draw2d::texture_update_request{texture, this, x, y, image});
    if (rect.has_error()) {
        return gltfx_rslt<void>::err(rect.err());
    }
    if (!rect.value().has_value()) {
        return gltfx_rslt<void>::ok(); // T3: an update with no width or no height writes nothing
    }
    return draw2d::update_texture_object(*texture, *rect.value(), image,
                                         draw2d::default_batch_allocator());
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
    held_textures.release_held(); // D-W8-288: the textures of an abandoned frame
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
    if (!draw2d::quad_in_pixel_range(corners, affine)) {
        tally.piece_refused(gltfx_draw_2d_refusal::pixel_position_out_of_range); // S2b-fix1, I-7
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

void renderer_2d_impl::fill_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                      gltfx_draw_layer layer) noexcept {
    submit_triangles(vertices, {}, false, layer);
}

void renderer_2d_impl::fill_indexed_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                              std::span<const std::uint32_t> indices,
                                              gltfx_draw_layer layer) noexcept {
    submit_triangles(vertices, indices, true, layer);
}

void renderer_2d_impl::stroke_line(const gltfx_segment_world &segment,
                                   const gltfx_line_style &style, gltfx_rgba color,
                                   gltfx_draw_layer layer) noexcept {
    if (!shape_call_goes_on(draw2d::refusal_of_stroke_line(segment, style, color))) {
        return; // the whole line, once
    }
    const bool built =
        stroke_pixels.build(segment, style, color, affine) == draw2d::stroke_pixels_outcome::built;
    keep_shape_triangles(built, stroke_pixels.vertices(), stroke_pixels.indices(), layer);
}

void renderer_2d_impl::stroke_rect(const gltfx_rect_world &rect,
                                   const gltfx_rect_outline_style &style, gltfx_rgba color,
                                   gltfx_draw_layer layer) noexcept {
    if (!shape_call_goes_on(draw2d::refusal_of_stroke_rect(rect, style, color))) {
        return; // the whole outline, once
    }
    const bool built =
        rect_pixels.build(rect, style, color, affine) == draw2d::stroke_rect_pixels_outcome::built;
    keep_shape_triangles(built, rect_pixels.vertices(), rect_pixels.indices(), layer);
}

bool renderer_2d_impl::shape_call_goes_on(gltfx_draw_2d_refusal refusal) noexcept {
    if (!frame_open) {
        tally.piece_outside_frame();
        return false;
    }
    if (refusal != gltfx_draw_2d_refusal::none) {
        tally.piece_refused(refusal);
        return false;
    }
    return true;
}

void renderer_2d_impl::keep_shape_triangles(bool built,
                                            std::span<const draw2d::colored_pixel_vertex> vertices,
                                            std::span<const std::uint32_t> indices,
                                            gltfx_draw_layer layer) noexcept {
    if (!built) {
        tally.piece_refused(gltfx_draw_2d_refusal::pixel_position_out_of_range); // the outline
        return;
    }
    if (!pieces.add_triangles(vertices, indices, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
}

void renderer_2d_impl::submit_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                        std::span<const std::uint32_t> indices, bool indexed,
                                        gltfx_draw_layer layer) noexcept {
    const draw2d::triangle_call_counts counts{vertices.size(),
                                              indexed ? indices.size() : std::size_t{0}, indexed};
    if (draw2d::is_empty_triangle_call(counts)) {
        return; // nothing to draw, and not a piece: it is counted nowhere (D-W8-287)
    }
    if (!frame_open) {
        tally.piece_outside_frame();
        return;
    }
    const gltfx_draw_2d_refusal refusal =
        draw2d::refusal_of_triangle_call(vertices, indices, indexed);
    if (refusal != gltfx_draw_2d_refusal::none) {
        tally.piece_refused(refusal); // the whole call, once
        return;
    }
    keep_triangle_call(vertices, indexed ? indices : std::span<const std::uint32_t>{}, layer);
}

void renderer_2d_impl::keep_triangle_call(std::span<const gltfx_vertex_2d_world> vertices,
                                          std::span<const std::uint32_t> indices,
                                          gltfx_draw_layer layer) noexcept {
    // Turned NOW, by the batch transform in force, like a quad (submit()).
    switch (triangle_pixels.convert(vertices, affine)) {
    case draw2d::triangle_pixels_outcome::pixel_position_out_of_range:
        tally.piece_refused(gltfx_draw_2d_refusal::pixel_position_out_of_range); // S2b-fix1, I-7
        return;
    case draw2d::triangle_pixels_outcome::out_of_memory:
        tally.piece_dropped_out_of_memory();
        return;
    case draw2d::triangle_pixels_outcome::converted:
        break;
    }
    if (!pieces.add_triangles(triangle_pixels.converted(), indices, layer.value)) {
        tally.piece_dropped_out_of_memory();
    }
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

bool renderer_2d_impl::add_piece_to_batch(const draw2d::pending_piece &piece) noexcept {
    if (piece.kind == draw2d::piece_kind::triangles) {
        // A triangle piece carries its texture (0 for none) and one read per vertex (T5): a change
        // of texture starts another run, in paint order, as for a quad.
        return batch.add_textured_triangles(
            draw2d::textured_triangles{pieces.triangle_vertices(piece),
                                       pieces.triangle_reads(piece),
                                       pieces.triangle_indices(piece)},
            draw2d::batch_state{program.program, piece.texture, 0});
    }
    // A quad carries its texture (0 for none) and where its corners read it (T4b-B1, D-W8-470):
    // a change of texture between two pieces starts another run, in paint order, never reordered.
    return batch.add_textured_quad(piece.corners, piece.texture_corners, piece.color,
                                   draw2d::batch_state{program.program, piece.texture, 0});
}

std::size_t renderer_2d_impl::batch_pending_in_paint_order() noexcept {
    // Paint order: (layer, submission). In place, allocates nothing, cannot fail (D-B4-2).
    pieces.sort();
    std::size_t added = 0;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (add_piece_to_batch(pieces.painted(i))) {
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
    // What draw_batch() starts from (T4b-B1, D-W8-473): the sampler reads unit 0, and no run is
    // textured until one binds its texture; a uniform keeps its value in the program between
    // flushes, so it is set every time.
    gl.glUniform1i(program.texture_location, 0);
    gl.glUniform1i(program.textured_location, 0);
    const gltfx_rslt<draw2d::batch_draw_counts> counts =
        draw2d::draw_batch(gl, batch, program.textured_location);
    if (counts.has_error()) {
        remember_error(counts.err());
        return false;
    }
    tally.draw_calls_issued(counts.value().calls);
    tally.texture_switched(counts.value().texture_switches);
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
    held_textures.release_held(); // D-W8-288: after the last draw of the frame
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
