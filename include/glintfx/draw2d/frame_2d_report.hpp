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
// WHAT A "PIECE" IS: one call of fill_rect() or fill_quad(). Every piece
// submitted while a frame is open ends in exactly one of four places:
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
};

// The token for `reason`: "none", "not_a_number", "infinite",
// "negative_size". A value this build does not know (a newer glintfx
// crossing the library boundary) reads "unknown" (docs/api-
// conventions.md R4). The returned text lives as long as the program.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal reason) noexcept;

struct gltfx_frame_2d_report {
    // Pieces submitted while this frame was open.
    std::uint64_t pieces_submitted = 0;
    // Pieces that reached the graphics card. A piece whose area is zero
    // is drawn: it simply covers no pixel.
    std::uint64_t pieces_drawn = 0;
    // Pieces refused because a value was not a finite number, or a
    // rectangle had a negative width or height. The frame goes on
    // without them; this is not an error of the frame.
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
    // How many draw calls reached the graphics card. Printed for you to
    // measure, never promised as a number.
    std::uint64_t draw_calls = 0;
};

} // namespace glintfx
