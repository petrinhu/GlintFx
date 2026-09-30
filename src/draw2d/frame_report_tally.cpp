// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/frame_report_tally.hpp"

#include <cmath>

namespace glintfx::draw2d {

namespace {
// The reason a set of numbers is refused, in the order of the vocabulary: any NaN first, then any
// infinity. `negative_size` is the caller's own test (only a rectangle has a size).
template <typename... Values>
[[nodiscard]] gltfx_draw_2d_refusal refusal_of_values(Values... values) noexcept {
    if ((std::isnan(values) || ...)) {
        return gltfx_draw_2d_refusal::not_a_number;
    }
    if ((std::isinf(values) || ...)) {
        return gltfx_draw_2d_refusal::infinite;
    }
    return gltfx_draw_2d_refusal::none;
}
} // namespace

gltfx_draw_2d_refusal refusal_of_quad(const quad_corners_world &corners,
                                      glintfx::gltfx_rgba color) noexcept {
    return refusal_of_values(corners[0].x, corners[0].y, corners[1].x, corners[1].y, corners[2].x,
                             corners[2].y, corners[3].x, corners[3].y, color.red, color.green,
                             color.blue, color.alpha);
}

gltfx_draw_2d_refusal refusal_of_rect(const glintfx::gltfx_rect_world &rect,
                                      glintfx::gltfx_rgba color) noexcept {
    const gltfx_draw_2d_refusal by_value =
        refusal_of_values(rect.corner.x, rect.corner.y, rect.size.x, rect.size.y, color.red,
                          color.green, color.blue, color.alpha);
    if (by_value != gltfx_draw_2d_refusal::none) {
        return by_value;
    }
    return (rect.size.x < 0.0 || rect.size.y < 0.0) ? gltfx_draw_2d_refusal::negative_size
                                                    : gltfx_draw_2d_refusal::none;
}

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

void frame_tally::piece_refused(gltfx_draw_2d_refusal reason) noexcept {
    ++current.pieces_submitted;
    if (current.pieces_refused == 0) {
        current.first_refusal = reason; // the FIRST refusal names the reason, the later ones do not
    }
    ++current.pieces_refused;
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
