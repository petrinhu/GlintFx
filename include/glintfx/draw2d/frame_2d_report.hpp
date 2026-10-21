// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// draw2d/frame_2d_report.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what happened to one frame, counted. It is the ONE place a frame
// reports anything (the leader's decision of 19/09/2026: drawing calls
// return nothing; a problem is reported once, at the end of the frame).
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header). New counters are appended
// at the end, never inserted, never reordered.
//
// WHAT A "PIECE" IS: one call of a drawing method (fill_rect(), fill_quad(),
// fill_triangles(), fill_indexed_triangles(), stroke_line(), stroke_rect(),
// fill_ellipse(), stroke_ellipse(), draw_texture() or draw_texture_quad()),
// whatever the
// method: a triangle call with a hundred triangles is one piece, and a
// refusal refuses the whole call, once. A call that does nothing (no
// vertices, or no indices for the indexed one) is not a piece at all. Every
// piece submitted while a frame is open ends in exactly one of four places:
//
//   pieces_submitted == pieces_drawn + pieces_refused
//                       + pieces_dropped_out_of_memory
//                       + pieces_dropped_graphics_failure
//
// Proved by: frame_report_tally_test (a piece holding a non-number is
// refused with the token of the FIRST reason; an empty frame reports
// zero; running out of memory becomes a count and one error at the
// end, never a thrown exception).
// A piece submitted while NO frame is open (before begin_frame(), or
// after finish_frame()) is not part of any frame: it is dropped and
// counted in pieces_dropped_outside_frame of the NEXT report, never in
// pieces_submitted.

namespace glintfx {

// Why the FIRST refused piece of a frame was refused. The token of each
// value (gltfx_draw_2d_refusal_name()) is an identifier, never a
// sentence (docs/api-conventions.md R7). APPEND-ONLY: a value is never
// renumbered and never reused.
enum class gltfx_draw_2d_refusal : std::uint8_t {
    none = 0,
    not_a_number = 1,
    infinite = 2,
    negative_size = 3,
    // Appended in W8 (D-W8-30, D-W8-285, D-W8-287): the triangle refusals
    // and the option refusal share this one enum, so first_refusal names
    // any of them.
    index_out_of_range = 4,
    count_not_multiple_of_three = 5,
    unknown_option = 6,
    too_many_vertices = 7,
    // Appended by S2b-fix1: every value given was a finite number, but
    // the batch transform made of a position a pixel coordinate larger
    // in magnitude than the library's vertex program can scale without
    // overflowing (about 1.7e38: half the largest 32-bit float, less 3
    // units in the last place), or the transform itself holds a value
    // that is not a finite number. A position outside the surface is NOT
    // this: it is drawn, and simply covers no pixel.
    pixel_position_out_of_range = 8,
    // Appended by R2D-TEXTURE T4b-A (D-W8-290, the one list of the texture
    // reasons): the texture of a drawing call is released or was never
    // opened; it belongs to another renderer; the source region reaches
    // outside the texture; or the four corners of a textured quadrilateral
    // define no projective mapping (three on one line, edges that cross, or
    // a corner pointing inward; decided exactly), or define one with a
    // projective weight of 2^63 or more (draw_texture_quad() in
    // renderer_2d.hpp).
    texture_not_open = 9,
    texture_other_renderer = 10,
    region_out_of_range = 11,
    degenerate_quad = 12,
};

// The token for `reason`: "none", "not_a_number", "infinite",
// "negative_size", "index_out_of_range", "count_not_multiple_of_three",
// "unknown_option", "too_many_vertices", "pixel_position_out_of_range",
// "texture_not_open", "texture_other_renderer", "region_out_of_range" or
// "degenerate_quad". A value this build does not
// know (a newer glintfx crossing the library boundary) reads "unknown"
// (docs/api-conventions.md R4). The returned text lives as long as the
// program.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal reason) noexcept;

struct gltfx_frame_2d_report {
    // Pieces submitted while this frame was open.
    std::uint64_t pieces_submitted = 0;
    // Pieces that reached the graphics card. A piece whose area is zero
    // is drawn: it simply covers no pixel.
    std::uint64_t pieces_drawn = 0;
    // Pieces refused, one count per refused call: a value that is not a
    // finite number, a rectangle with a negative width or height, a line with
    // a negative thickness, a rectangle outline with a negative thickness, an
    // ellipse with a negative radius or thickness, an
    // index out of range, a count that is not a
    // multiple of three, an unknown option, too many vertices, a position
    // whose pixel coordinate is out of the range the vertex program can
    // scale, a texture that is not open or belongs to another renderer, a
    // source region outside its texture, or a textured quadrilateral with no
    // projective mapping or with a projective weight of 2^63 or more.
    // The frame goes on without them; this is not an error of the
    // frame. The counters at the end of this struct, one per reason, add up
    // to this one.
    std::uint64_t pieces_refused = 0;
    // Why the first of those was refused; `none` when pieces_refused is
    // zero.
    gltfx_draw_2d_refusal first_refusal = gltfx_draw_2d_refusal::none;
    // Pieces dropped because the renderer could not get memory of its own
    // to store them. When this is not zero, finish_frame() returns an
    // error.
    std::uint64_t pieces_dropped_out_of_memory = 0;
    // Pieces accepted while the frame was open that never reached the
    // screen because the graphics side failed: the context could not be
    // made current, or the graphics card refused the frame's data (the
    // card running out of ITS memory counts here, not in
    // pieces_dropped_out_of_memory). When this is not zero,
    // finish_frame() returns an error, and the error says why.
    std::uint64_t pieces_dropped_graphics_failure = 0;
    // Pieces submitted with no frame open, since the previous report.
    std::uint64_t pieces_dropped_outside_frame = 0;
    // Frames that were begun and never finished (begin_frame() called
    // while a frame was open throws the open one away, undrawn), since
    // the previous report. The pieces of an abandoned frame are not
    // counted in any other field of any report.
    std::uint64_t frames_abandoned = 0;
    // How many times the transform changed (begin_batch()), counting
    // the pixel-direct batch every frame starts with.
    std::uint64_t batches = 0;
    // How many flush() barriers this frame had.
    std::uint64_t flushes = 0;
    // How many draw calls reached the graphics card. A flush whose data or
    // drawing the card refuses counts none of its draw calls: its pieces
    // count in pieces_dropped_graphics_failure, and finish_frame() returns
    // an error. Printed for you to measure, never promised as a number.
    std::uint64_t draw_calls = 0;
    // Appended in W8 (D-W8-30): one counter per refusal reason. Their sum is
    // pieces_refused, always.
    std::uint64_t pieces_refused_not_a_number = 0;
    std::uint64_t pieces_refused_infinite = 0;
    std::uint64_t pieces_refused_negative_size = 0;
    std::uint64_t pieces_refused_index_out_of_range = 0;
    std::uint64_t pieces_refused_count_not_multiple_of_three = 0;
    std::uint64_t pieces_refused_unknown_option = 0;
    std::uint64_t pieces_refused_too_many_vertices = 0;
    std::uint64_t pieces_refused_pixel_position_out_of_range = 0;
    // Appended by R2D-TEXTURE T4b-A, one per texture reason.
    std::uint64_t pieces_refused_texture_not_open = 0;
    std::uint64_t pieces_refused_texture_other_renderer = 0;
    std::uint64_t pieces_refused_region_out_of_range = 0;
    std::uint64_t pieces_refused_degenerate_quad = 0;
    // Appended by R2D-TEXTURE T4b-B1 (D-W8-471): how many times the bound
    // texture changed between two draw calls, a change to or from no
    // texture included; the first draw call of every flush counts when it
    // binds a texture. A flush the card refuses counts none of its
    // switches, as for draw_calls. A cost, printed for you to measure, never
    // promised as a number: a textured piece between two untextured ones
    // costs two.
    std::uint64_t texture_switches = 0;
};

} // namespace glintfx
