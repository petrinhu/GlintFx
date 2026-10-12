// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/frame_report_tally.hpp"

namespace glintfx::draw2d {

void frame_tally::begin_frame() noexcept {
    if (frame_open) {
        ++abandoned_since_report; // the open frame is thrown away, undrawn; its pieces count
                                  // nowhere
    }
    current = gltfx_frame_2d_report{};
    current.batches = 1; // every frame starts with the pixel-direct batch
    frame_open = true;
}

void frame_tally::piece_drawn() noexcept {
    ++current.pieces_submitted;
    ++current.pieces_drawn;
}

void frame_tally::piece_dropped_graphics_failure() noexcept {
    ++current.pieces_submitted;
    ++current.pieces_dropped_graphics_failure;
}

namespace {

// The counter of ONE refusal reason (D-W8-30): the seven counters add up to pieces_refused, so each
// refused call bumps exactly one of them. Null for `none` and for a reason this build does not
// know.
[[nodiscard]] std::uint64_t *counter_of(gltfx_frame_2d_report &report,
                                        gltfx_draw_2d_refusal reason) noexcept {
    switch (reason) {
    case gltfx_draw_2d_refusal::not_a_number:
        return &report.pieces_refused_not_a_number;
    case gltfx_draw_2d_refusal::infinite:
        return &report.pieces_refused_infinite;
    case gltfx_draw_2d_refusal::negative_size:
        return &report.pieces_refused_negative_size;
    case gltfx_draw_2d_refusal::index_out_of_range:
        return &report.pieces_refused_index_out_of_range;
    case gltfx_draw_2d_refusal::count_not_multiple_of_three:
        return &report.pieces_refused_count_not_multiple_of_three;
    case gltfx_draw_2d_refusal::unknown_option:
        return &report.pieces_refused_unknown_option;
    case gltfx_draw_2d_refusal::too_many_vertices:
        return &report.pieces_refused_too_many_vertices;
    case gltfx_draw_2d_refusal::none:
        return nullptr;
    }
    return nullptr;
}

} // namespace

void frame_tally::piece_refused(gltfx_draw_2d_refusal reason) noexcept {
    ++current.pieces_submitted;
    if (current.pieces_refused == 0) {
        current.first_refusal = reason; // the FIRST refusal names the reason, the later ones do not
    }
    ++current.pieces_refused;
    if (std::uint64_t *counter = counter_of(current, reason); counter != nullptr) {
        ++*counter;
    }
}

void frame_tally::piece_dropped_out_of_memory() noexcept {
    ++current.pieces_submitted;
    ++current.pieces_dropped_out_of_memory;
}

void frame_tally::piece_outside_frame() noexcept { ++outside_since_report; }

void frame_tally::batch_began() noexcept { ++current.batches; }

void frame_tally::flushed() noexcept { ++current.flushes; }

void frame_tally::draw_calls_issued(std::uint64_t count) noexcept { current.draw_calls += count; }

finish_status frame_tally::finish() noexcept {
    if (!frame_open) {
        return finish_status::no_frame_open;
    }
    current.frames_abandoned = abandoned_since_report;
    current.pieces_dropped_outside_frame = outside_since_report;
    abandoned_since_report = 0; // "since the previous report": each is reported once
    outside_since_report = 0;
    last = current;
    frame_open = false;
    return current.pieces_dropped_out_of_memory != 0 ? finish_status::out_of_memory
                                                     : finish_status::ok;
}

gltfx_frame_2d_report frame_tally::last_report() const noexcept { return last; }

} // namespace glintfx::draw2d
