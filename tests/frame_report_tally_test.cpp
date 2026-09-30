// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>

#include "draw2d/frame_report_tally.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// frame_report_tally_test.cpp - R2D-BATCH, fatia B2d (docs/plano-w7d.md sec. 4.3, D-W7D-13 as
// AMENDED by docs/auditoria-api-draw2d.md B0-C2 and PLANO-errata.md sec. 3): the pure atom that
// counts what happened to one frame, decides which piece is refused and why, and produces THE
// report - the ONE place a frame reports anything (drawing calls return nothing).
//
// THE RULE, frozen in the public header of the report (B4):
//   pieces_submitted == pieces_drawn + pieces_refused + pieces_dropped_out_of_memory
// so every piece submitted while a frame is open ends in exactly one of three places. A piece
// submitted while NO frame is open is not part of any frame: it is counted in
// pieces_dropped_outside_frame of the NEXT report, never in pieces_submitted. A frame begun while
// another is open throws the open one away and is counted in frames_abandoned of the next report;
// the pieces of an abandoned frame are counted nowhere else.
//
// B0-C2: a gltfx_rslt holds a value OR an error, so an error could not carry the counts. The
// report therefore ALWAYS exists: finish() returns a status (ok, out_of_memory, no_frame_open) and
// last_report() reads the report of the last finish - also when the status was an error.
//
// THE LITERAL NUMBERS AND TOKENS asserted below are the ones frozen in the header of B0
// (gltfx_draw_2d_refusal: none = 0, not_a_number = 1, infinite = 2, negative_size = 3; tokens
// "none", "not_a_number", "infinite", "negative_size", "unknown"): never through the internal
// constants the code uses, so that the swap to the public names in B4 is caught (D-P1-5's rule).
//
// RED, SEEN: before frame_report_tally.{hpp,cpp} existed, this file's own #include line failed to
// compile; then, against a body that counted nothing, the cells below failed.

using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_draw_2d_refusal_name;
using glintfx::draw2d::finish_status;
using glintfx::draw2d::frame_tally;

namespace {
[[nodiscard]] int number_of(gltfx_draw_2d_refusal reason) { return static_cast<int>(reason); }
} // namespace

GLINTFX_TEST(frame_report_tally_frame_cells) {
    int analyzed = 0;

    // A frame with one refused piece: the piece is submitted and REFUSED, with the token of the
    // FIRST reason; a second refused piece for ANOTHER reason does not change it. A refusal is NOT
    // an error of the frame.
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_refused(gltfx_draw_2d_refusal::not_a_number);
        tally.piece_refused(gltfx_draw_2d_refusal::infinite);
        tally.piece_drawn();
        const finish_status status = tally.finish();
        GLINTFX_CHECK(status == finish_status::ok);
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{3});
        GLINTFX_CHECK_EQ(r.pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{2});
        GLINTFX_CHECK_EQ(number_of(r.first_refusal), 1);
        GLINTFX_CHECK(gltfx_draw_2d_refusal_name(r.first_refusal) == "not_a_number");
        GLINTFX_CHECK_EQ(r.pieces_dropped_out_of_memory, std::uint64_t{0});
        ++analyzed;
    }
    // An EMPTY frame reports zero pieces of every kind, `none` for the first refusal - and the one
    // pixel-direct batch every frame starts with.
    {
        frame_tally tally;
        tally.begin_frame();
        GLINTFX_CHECK(tally.finish() == finish_status::ok);
        const auto r = tally.last_report();
        GLINTFX_CHECK(r.pieces_submitted == 0 && r.pieces_drawn == 0 && r.pieces_refused == 0);
        GLINTFX_CHECK(r.pieces_dropped_out_of_memory == 0 && r.pieces_dropped_outside_frame == 0);
        GLINTFX_CHECK(r.frames_abandoned == 0 && r.flushes == 0 && r.draw_calls == 0);
        GLINTFX_CHECK_EQ(number_of(r.first_refusal), 0);
        GLINTFX_CHECK_EQ(r.batches, std::uint64_t{1});
        ++analyzed;
    }
    // A piece whose flush failed is submitted and ends in pieces_dropped_graphics_failure: not
    // drawn, not refused, not dropped, and NOT an error of the tally (the error of the card is the
    // renderer's).
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_dropped_graphics_failure();
        tally.piece_dropped_graphics_failure();
        const finish_status status = tally.finish();
        const auto r = tally.last_report();
        GLINTFX_CHECK(status == finish_status::ok);
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{3});
        GLINTFX_CHECK_EQ(r.pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK(r.pieces_refused == 0 && r.pieces_dropped_out_of_memory == 0);
        GLINTFX_CHECK_EQ(r.pieces_dropped_graphics_failure, std::uint64_t{2});
        GLINTFX_CHECK_EQ(r.pieces_submitted, r.pieces_drawn + r.pieces_refused +
                                                 r.pieces_dropped_out_of_memory +
                                                 r.pieces_dropped_graphics_failure);
        ++analyzed;
    }
    // Running out of memory becomes a COUNT and ONE error at the end: the status says
    // out_of_memory, and the counts are still readable AFTER the error (B0-C2).
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_dropped_out_of_memory();
        tally.piece_dropped_out_of_memory();
        tally.piece_refused(gltfx_draw_2d_refusal::negative_size);
        const finish_status status = tally.finish();
        GLINTFX_CHECK(status == finish_status::out_of_memory);
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{4});
        GLINTFX_CHECK_EQ(r.pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_dropped_out_of_memory, std::uint64_t{2});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{1});
        GLINTFX_CHECK_EQ(number_of(r.first_refusal), 3);
        // ... and it stays readable, unchanged, however many times it is read.
        GLINTFX_CHECK_EQ(tally.last_report().pieces_dropped_out_of_memory, std::uint64_t{2});
        ++analyzed;
    }
    // The invariant of the header, over a mix: submitted == drawn + refused +
    // dropped_out_of_memory.
    {
        frame_tally tally;
        tally.begin_frame();
        for (int i = 0; i < 100; ++i) {
            if (i % 5 == 0) {
                tally.piece_refused(gltfx_draw_2d_refusal::infinite);
            } else if (i % 7 == 0) {
                tally.piece_dropped_out_of_memory();
            } else {
                tally.piece_drawn();
            }
        }
        (void)tally.finish();
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{100});
        GLINTFX_CHECK_EQ(r.pieces_submitted,
                         r.pieces_drawn + r.pieces_refused + r.pieces_dropped_out_of_memory);
        ++analyzed;
    }
    // Before the FIRST frame every counter reads zero.
    {
        frame_tally tally;
        const auto r = tally.last_report();
        GLINTFX_CHECK(r.pieces_submitted == 0 && r.batches == 0 && r.frames_abandoned == 0);
        ++analyzed;
    }
    // Batches, flushes and draw calls are counted; the frame starts with ONE batch (pixel-direct).
    {
        frame_tally tally;
        tally.begin_frame();
        tally.batch_began();
        tally.batch_began();
        tally.flushed();
        tally.draw_calls_issued(3);
        tally.draw_calls_issued(2);
        (void)tally.finish();
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.batches, std::uint64_t{3});
        GLINTFX_CHECK_EQ(r.flushes, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.draw_calls, std::uint64_t{5});
        ++analyzed;
    }
    // A frame begun while another is open throws the open one away: frames_abandoned of the NEXT
    // report, and the pieces of the abandoned frame are counted NOWHERE else. The next report after
    // that one starts again at zero (the counter is "since the previous report").
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_refused(gltfx_draw_2d_refusal::infinite);
        tally.begin_frame(); // abandons the first
        tally.piece_drawn();
        (void)tally.finish();
        auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.frames_abandoned, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{0});
        GLINTFX_CHECK_EQ(number_of(r.first_refusal), 0);
        tally.begin_frame();
        (void)tally.finish();
        GLINTFX_CHECK_EQ(tally.last_report().frames_abandoned, std::uint64_t{0});
        ++analyzed;
    }
    // A piece submitted with NO frame open (before begin_frame, after finish) is counted in
    // pieces_dropped_outside_frame of the NEXT report, never in pieces_submitted, and only once.
    {
        frame_tally tally;
        tally.piece_outside_frame(); // before any frame
        tally.piece_outside_frame();
        tally.begin_frame();
        tally.piece_drawn();
        (void)tally.finish();
        auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_dropped_outside_frame, std::uint64_t{2});
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{1});
        tally.piece_outside_frame(); // after finish
        // the report already made is not disturbed by a piece that belongs to no frame
        GLINTFX_CHECK_EQ(tally.last_report().pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(tally.last_report().pieces_dropped_outside_frame, std::uint64_t{2});
        tally.begin_frame();
        (void)tally.finish();
        r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_dropped_outside_frame, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{0});
        ++analyzed;
    }
    // finish() with no frame open is refused by name and does not disturb the last report.
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        (void)tally.finish();
        GLINTFX_CHECK(tally.finish() == finish_status::no_frame_open);
        GLINTFX_CHECK_EQ(tally.last_report().pieces_drawn, std::uint64_t{1});
        ++analyzed;
    }
    // last_report() is the report of the last frame that FINISHED, never the frame in progress
    // (B0-C2): frame A ends with known counts, frame B is opened and drawn in, and the report read
    // meanwhile is still A's; it becomes B's only when B finishes.
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_drawn();
        tally.piece_refused(gltfx_draw_2d_refusal::infinite);
        (void)tally.finish();
        tally.begin_frame();
        for (int i = 0; i < 5; ++i) {
            tally.piece_drawn();
        }
        tally.draw_calls_issued(9);
        auto meanwhile = tally.last_report();
        GLINTFX_CHECK_EQ(meanwhile.pieces_submitted, std::uint64_t{3});
        GLINTFX_CHECK_EQ(meanwhile.pieces_drawn, std::uint64_t{2});
        GLINTFX_CHECK_EQ(meanwhile.draw_calls, std::uint64_t{0});
        GLINTFX_CHECK_EQ(number_of(meanwhile.first_refusal), 2);
        (void)tally.finish();
        GLINTFX_CHECK_EQ(tally.last_report().pieces_submitted, std::uint64_t{5});
        GLINTFX_CHECK_EQ(tally.last_report().draw_calls, std::uint64_t{9});
        ++analyzed;
    }
    // The report of the last finished frame survives a frame that is ABANDONED: frame A finishes
    // with one piece, frame B gets three and is thrown away by a new begin_frame(); last_report()
    // is still A's. (Only finish() rewrites the report, never begin_frame(): B0-C2.)
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        (void)tally.finish();
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_drawn();
        tally.piece_drawn();
        tally.begin_frame(); // abandons B
        GLINTFX_CHECK_EQ(tally.last_report().pieces_submitted, std::uint64_t{1});
        GLINTFX_CHECK_EQ(tally.last_report().pieces_drawn, std::uint64_t{1});
        ++analyzed;
    }
    // A frame open after an error frame is a clean frame: no counts leak from the failed one.
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_dropped_out_of_memory();
        GLINTFX_CHECK(tally.finish() == finish_status::out_of_memory);
        tally.begin_frame();
        tally.piece_drawn();
        GLINTFX_CHECK(tally.finish() == finish_status::ok);
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_dropped_out_of_memory, std::uint64_t{0});
        GLINTFX_CHECK_EQ(r.pieces_drawn, std::uint64_t{1});
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 13);
    std::println("frame_report_tally_test: {} celula(s) conferida(s) (quadro)", analyzed);
}
