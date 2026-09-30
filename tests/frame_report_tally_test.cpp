// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>
#include <cstdint>
#include <limits>
#include <print>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/frame_report_tally.hpp"
#include "draw2d/quad_vertices.hpp"

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

using glintfx::gltfx_rect_world;
using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_world;
using glintfx::draw2d::finish_status;
using glintfx::draw2d::frame_tally;
using glintfx::draw2d::piece_refusal;
using glintfx::draw2d::piece_refusal_token;
using glintfx::draw2d::quad_corners_world;
using glintfx::draw2d::refusal_of_quad;
using glintfx::draw2d::refusal_of_rect;

namespace {
constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};

[[nodiscard]] quad_corners_world quad(double x0, double y0, double x1, double y1) {
    return quad_corners_world{gltfx_vec2_world{x0, y0}, gltfx_vec2_world{x1, y0},
                              gltfx_vec2_world{x1, y1}, gltfx_vec2_world{x0, y1}};
}

[[nodiscard]] int number_of(piece_refusal reason) { return static_cast<int>(reason); }
} // namespace

GLINTFX_TEST(frame_report_tally_refusal_cells) {
    int analyzed = 0;

    // The literal numbers and tokens of the vocabulary (frozen in B0), append-only.
    GLINTFX_CHECK_EQ(number_of(piece_refusal::none), 0);
    GLINTFX_CHECK_EQ(number_of(piece_refusal::not_a_number), 1);
    GLINTFX_CHECK_EQ(number_of(piece_refusal::infinite), 2);
    GLINTFX_CHECK_EQ(number_of(piece_refusal::negative_size), 3);
    GLINTFX_CHECK(piece_refusal_token(piece_refusal::none) == "none");
    GLINTFX_CHECK(piece_refusal_token(piece_refusal::not_a_number) == "not_a_number");
    GLINTFX_CHECK(piece_refusal_token(piece_refusal::infinite) == "infinite");
    GLINTFX_CHECK(piece_refusal_token(piece_refusal::negative_size) == "negative_size");
    // A value this build does not know reads "unknown" (R4), never undefined behavior.
    // Simulates a value a NEWER glintfx produced that this build's vocabulary has never heard of
    // (R4).
    //
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    GLINTFX_CHECK(piece_refusal_token(static_cast<piece_refusal>(200)) == "unknown");
    ++analyzed;

    // A finite quad is not refused - and a quad with ZERO area is drawn, it just covers no pixel.
    GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 10, 10), k_white) == piece_refusal::none);
    GLINTFX_CHECK(refusal_of_quad(quad(5, 5, 5, 5), k_white) == piece_refusal::none);
    ++analyzed;
    // A NaN in ANY of the eight coordinates is refused, with the token not_a_number.
    for (int slot = 0; slot < 8; ++slot) {
        quad_corners_world q = quad(0, 0, 10, 10);
        double *const coordinates[8] = {&q[0].x, &q[0].y, &q[1].x, &q[1].y,
                                        &q[2].x, &q[2].y, &q[3].x, &q[3].y};
        *coordinates[slot] = k_nan;
        GLINTFX_CHECK(refusal_of_quad(q, k_white) == piece_refusal::not_a_number);
    }
    ++analyzed;
    // An infinity in a coordinate (either sign) is refused as `infinite`.
    {
        quad_corners_world plus = quad(0, 0, 10, 10);
        plus[2].x = k_inf;
        quad_corners_world minus = quad(0, 0, 10, 10);
        minus[3].y = -k_inf;
        GLINTFX_CHECK(refusal_of_quad(plus, k_white) == piece_refusal::infinite);
        GLINTFX_CHECK(refusal_of_quad(minus, k_white) == piece_refusal::infinite);
        ++analyzed;
    }
    // The color is a value too: a channel that is not a finite number refuses the piece.
    {
        GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1),
                                      gltfx_rgba{1.0F, static_cast<float>(k_nan), 0.0F, 1.0F}) ==
                      piece_refusal::not_a_number);
        GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1),
                                      gltfx_rgba{static_cast<float>(k_inf), 0.0F, 0.0F, 1.0F}) ==
                      piece_refusal::infinite);
        ++analyzed;
    }
    // EVERY color channel is checked, alpha included (a quad or a rectangle whose ONLY problem is
    // one channel): NaN in any of the four is not_a_number, an infinity in any is infinite.
    {
        for (int channel = 0; channel < 4; ++channel) {
            float nan_channels[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            float inf_channels[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            nan_channels[channel] = static_cast<float>(k_nan);
            inf_channels[channel] = static_cast<float>(k_inf);
            const gltfx_rgba with_nan{nan_channels[0], nan_channels[1], nan_channels[2],
                                      nan_channels[3]};
            const gltfx_rgba with_inf{inf_channels[0], inf_channels[1], inf_channels[2],
                                      inf_channels[3]};
            const gltfx_rect_world fine{{0.0, 0.0}, {5.0, 5.0}};
            GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1), with_nan) ==
                          piece_refusal::not_a_number);
            GLINTFX_CHECK(refusal_of_quad(quad(0, 0, 1, 1), with_inf) == piece_refusal::infinite);
            GLINTFX_CHECK(refusal_of_rect(fine, with_nan) == piece_refusal::not_a_number);
            GLINTFX_CHECK(refusal_of_rect(fine, with_inf) == piece_refusal::infinite);
        }
        ++analyzed;
    }
    // A rectangle with a NEGATIVE width or height is refused as negative_size; zero size is drawn.
    {
        const gltfx_rect_world negative_width{{0.0, 0.0}, {-1.0, 5.0}};
        const gltfx_rect_world negative_height{{0.0, 0.0}, {5.0, -0.5}};
        const gltfx_rect_world zero{{0.0, 0.0}, {0.0, 0.0}};
        const gltfx_rect_world fine{{-3.0, -4.0}, {5.0, 5.0}};
        GLINTFX_CHECK(refusal_of_rect(negative_width, k_white) == piece_refusal::negative_size);
        GLINTFX_CHECK(refusal_of_rect(negative_height, k_white) == piece_refusal::negative_size);
        GLINTFX_CHECK(refusal_of_rect(zero, k_white) == piece_refusal::none);
        GLINTFX_CHECK(refusal_of_rect(fine, k_white) == piece_refusal::none);
        ++analyzed;
    }
    // A piece with several problems is refused for ONE reason, in the order of the vocabulary:
    // not_a_number first, then infinite, then negative_size.
    {
        const gltfx_rect_world nan_and_negative{{k_nan, 0.0}, {-1.0, 5.0}};
        const gltfx_rect_world inf_and_negative{{0.0, k_inf}, {-1.0, 5.0}};
        const gltfx_rect_world nan_and_inf{{k_nan, k_inf}, {1.0, 1.0}};
        GLINTFX_CHECK(refusal_of_rect(nan_and_negative, k_white) == piece_refusal::not_a_number);
        GLINTFX_CHECK(refusal_of_rect(inf_and_negative, k_white) == piece_refusal::infinite);
        GLINTFX_CHECK(refusal_of_rect(nan_and_inf, k_white) == piece_refusal::not_a_number);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("frame_report_tally_test: {} celula(s) conferida(s) (recusa)", analyzed);
}

GLINTFX_TEST(frame_report_tally_frame_cells) {
    int analyzed = 0;

    // A frame with one refused piece: the piece is submitted and REFUSED, with the token of the
    // FIRST reason; a second refused piece for ANOTHER reason does not change it. A refusal is NOT
    // an error of the frame.
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_refused(piece_refusal::not_a_number);
        tally.piece_refused(piece_refusal::infinite);
        tally.piece_drawn();
        const finish_status status = tally.finish();
        GLINTFX_CHECK(status == finish_status::ok);
        const auto r = tally.last_report();
        GLINTFX_CHECK_EQ(r.pieces_submitted, std::uint64_t{3});
        GLINTFX_CHECK_EQ(r.pieces_drawn, std::uint64_t{1});
        GLINTFX_CHECK_EQ(r.pieces_refused, std::uint64_t{2});
        GLINTFX_CHECK_EQ(number_of(r.first_refusal), 1);
        GLINTFX_CHECK(piece_refusal_token(r.first_refusal) == "not_a_number");
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
    // Running out of memory becomes a COUNT and ONE error at the end: the status says
    // out_of_memory, and the counts are still readable AFTER the error (B0-C2).
    {
        frame_tally tally;
        tally.begin_frame();
        tally.piece_drawn();
        tally.piece_dropped_out_of_memory();
        tally.piece_dropped_out_of_memory();
        tally.piece_refused(piece_refusal::negative_size);
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
                tally.piece_refused(piece_refusal::infinite);
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
        tally.piece_refused(piece_refusal::infinite);
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
        tally.piece_refused(piece_refusal::infinite);
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

    GLINTFX_CHECK_EQ(analyzed, 11);
    std::println("frame_report_tally_test: {} celula(s) conferida(s) (quadro)", analyzed);
}
