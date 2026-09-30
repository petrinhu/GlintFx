// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/draw2d/frame_2d_report.hpp>

// draw2d/frame_report_tally.hpp - R2D-BATCH, fatia B2d (docs/plano-w7d.md sec. 4.3, D-W7D-13 as
// AMENDED by docs/auditoria-api-draw2d.md B0-C2): the PURE atom that counts what happened to one
// frame and produces THE report - the ONE place a frame
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
// Which piece is refused, and why, is decided by piece_refusal.hpp.
namespace glintfx::draw2d {

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
    // A piece that was accepted and never reached the card because the flush that carried it failed
    // (the context could not be made current, or the card refused the data): submitted, and no
    // other end.
    void piece_dropped_graphics_failure() noexcept;
    void piece_refused(gltfx_draw_2d_refusal reason) noexcept;
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
    // finish(), which rewrites the member a reference would point at.
    [[nodiscard]] gltfx_frame_2d_report last_report() const noexcept;

  private:
    gltfx_frame_2d_report current;
    gltfx_frame_2d_report last;
    bool frame_open = false;
    std::uint64_t abandoned_since_report = 0;
    std::uint64_t outside_since_report = 0;
};

} // namespace glintfx::draw2d
