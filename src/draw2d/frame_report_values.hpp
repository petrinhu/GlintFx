// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

// draw2d/frame_report_values.hpp - R2D-BATCH, fatia B2d (docs/auditoria-api-draw2d.md B0, D-API-03;
// /var/tmp/cto-w7d/PLANO-errata.md D-P1-5's rule): the vocabulary and the shape of the frame
// report, in ONE internal header - never copied into the atom that uses them.
//
// TEMPORARY BY DECISION: the public names (gltfx_draw_2d_refusal, gltfx_draw_2d_refusal_name,
// gltfx_frame_2d_report) only exist once B4 publishes include/glintfx/draw2d/frame_2d_report.hpp.
// B4 DELETES this file and the atom uses the public names; there is no period of coexistence, so no
// static_assert of equality: afterwards there is ONE source. What guards the swap are the tests of
// B2d, which assert the LITERAL numbers and tokens frozen in B0 and never these names.
//
// APPEND-ONLY, FOREVER: a value is never renumbered and never reused; new counters are appended at
// the end of the report, never inserted, never reordered.
namespace glintfx::draw2d {

// Why the FIRST refused piece of a frame was refused. The token of each value is an identifier,
// never a sentence (docs/api-conventions.md R7).
enum class piece_refusal : std::uint8_t {
    none = 0,
    not_a_number = 1,
    infinite = 2,
    negative_size = 3,
};

// The token of `reason`; a value this build does not know reads "unknown" (R4).
[[nodiscard]] constexpr std::string_view piece_refusal_token(piece_refusal reason) noexcept {
    switch (reason) {
    case piece_refusal::none:
        return "none";
    case piece_refusal::not_a_number:
        return "not_a_number";
    case piece_refusal::infinite:
        return "infinite";
    case piece_refusal::negative_size:
        return "negative_size";
    }
    return "unknown";
}

// What happened to one frame, counted. A "piece" is one call of fill_rect() or fill_quad(). Every
// piece submitted while a frame is open ends in exactly one of three places:
//   pieces_submitted == pieces_drawn + pieces_refused + pieces_dropped_out_of_memory
struct frame_report {
    std::uint64_t pieces_submitted = 0;
    std::uint64_t pieces_drawn = 0;
    std::uint64_t pieces_refused = 0;
    piece_refusal first_refusal = piece_refusal::none;
    std::uint64_t pieces_dropped_out_of_memory = 0;
    std::uint64_t pieces_dropped_outside_frame = 0;
    std::uint64_t frames_abandoned = 0;
    std::uint64_t batches = 0;
    std::uint64_t flushes = 0;
    std::uint64_t draw_calls = 0;
};

} // namespace glintfx::draw2d
