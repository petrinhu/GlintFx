// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>

#include "draw2d/frame_report_values.hpp"
#include "draw2d/quad_vertices.hpp"

// draw2d/frame_report_tally.hpp - R2D-BATCH, fatia B2d (docs/plano-w7d.md sec. 4.3, D-W7D-13 as
// AMENDED by docs/auditoria-api-draw2d.md B0-C2): the PURE atom that counts what happened to one
// frame, decides which piece is refused and why, and produces THE report - the ONE place a frame
// reports anything (the leader's decision of 19/09/2026: drawing calls return nothing). It knows
// nothing of GL or of the operating system, allocates nothing and can throw nothing, so it compiles
// and runs on all five systems (GODS_LAWS.md L-19/L-22: src/draw2d/ never reaches src/platform/).
//
// THE REPORT ALWAYS EXISTS (B0-C2): a gltfx_rslt holds a value OR an error, so an error could not
// carry the counts - exactly on the frame that failed, when they matter most. finish() therefore
// returns only a STATUS (ok, out_of_memory, no_frame_open), and last_report() reads the report of
// the last finish() - the same value on success, and still readable after an error. Before the
// first frame every counter reads zero. The renderer (B4) turns the status into a gltfx_rslt.
//
// WHAT COUNTS AS AN ERROR OF THE FRAME: only out_of_memory (a piece was dropped for lack of
// memory). A refused piece is NOT an error: the frame is drawn without it and the report counts it.
//
// REFUSAL BY VALUE: a piece is refused when a value in it is not a finite number, or a rectangle
// has a negative width or height. ONE reason per piece, in the order of the vocabulary:
// not_a_number, then infinite, then negative_size. A quad with zero area is DRAWN (it covers no
// pixel) - a shape, not a bad value. The report keeps the reason of the FIRST refused piece only.
namespace glintfx::draw2d {

// Why a quad in world position, with this color, is refused (`none` when it is not).
[[nodiscard]] piece_refusal refusal_of_quad(const quad_corners_world &corners,
                                            glintfx::gltfx_rgba color) noexcept;

// The same for a rectangle in world position, which can also have a negative size.
[[nodiscard]] piece_refusal refusal_of_rect(const glintfx::gltfx_rect_world &rect,
                                            glintfx::gltfx_rgba color) noexcept;

enum class finish_status : std::uint8_t {
    ok,
    out_of_memory,
    no_frame_open,
};

class frame_tally {
  public:
    // Opens a frame. With another frame open, that one is thrown away (undrawn) and counted in
    // frames_abandoned of the NEXT report; its pieces are counted nowhere else. Every frame starts
    // with one batch (the pixel-direct one).
    void begin_frame() noexcept;

    // The three ends of a piece submitted while a frame is open (each also counts as submitted).
    void piece_drawn() noexcept;
    void piece_refused(piece_refusal reason) noexcept;
    void piece_dropped_out_of_memory() noexcept;

    // A piece submitted with NO frame open: counted in pieces_dropped_outside_frame of the next
    // report, never as submitted.
    void piece_outside_frame() noexcept;

    void batch_began() noexcept;
    void flushed() noexcept;
    void draw_calls_issued(std::uint64_t count) noexcept;

    // Closes the frame and makes its report the last one. no_frame_open (and no new report) when
    // none is open; out_of_memory when any piece was dropped for lack of memory; otherwise ok.
    [[nodiscard]] finish_status finish() noexcept;

    // The report of the last finish() that had a frame to close; all zero before the first.
    // cppcheck-suppress returnByReference ; reason: the report is a plain struct of counters
    // returned BY VALUE on purpose (B0-C2, B0-I10): the caller's copy stays valid after the next
    // begin_frame(), which rewrites the member a reference would point at.
    [[nodiscard]] frame_report last_report() const noexcept;

  private:
    frame_report current;
    frame_report last;
    bool frame_open = false;
    std::uint64_t abandoned_since_report = 0;
    std::uint64_t outside_since_report = 0;
};

} // namespace glintfx::draw2d
