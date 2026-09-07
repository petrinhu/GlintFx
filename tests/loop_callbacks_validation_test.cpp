// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <print>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/loop/loop_callbacks_validation.hpp"

// loop_callbacks_validation_test.cpp - LOOP-RUN fatia 6a (docs/plano-
// w6b-fatias-6-8.md D-W6b-43, GODS_LAWS.md L-19/L-20/L-40): the closed
// {empty, filled}^3 matrix over on_frame/on_render/on_event - eight
// cells, one GLINTFX_TEST per combination this file's own top comment
// enumerates, plus the closed-enumeration test at the bottom that
// re-runs all eight through one loop and prints the count (GODS_LAWS.md
// L-40: a matrix silently missing a cell would still look green
// without that).

using glintfx::gltfx_input_event;
using glintfx::gltfx_loop_callbacks;
using glintfx::platform::validate_loop_callbacks;

namespace {

// Trivial, non-empty std::function bodies - only their EMPTINESS
// matters to validate_loop_callbacks(), never what they do when called
// (this file never calls one).
bool filled_on_frame(const glintfx::gltfx_frame_tick &) noexcept { return true; }
void filled_on_render(const glintfx::gltfx_frame_tick &) noexcept {}
void filled_on_event(const gltfx_input_event &) noexcept {}

} // namespace

GLINTFX_TEST(all_three_empty_is_refused_naming_on_frame_first) {
    const gltfx_loop_callbacks callbacks{};

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_frame"));
}

GLINTFX_TEST(on_frame_empty_is_refused_even_when_on_render_is_filled) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_render = &filled_on_render;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_frame"));
}

GLINTFX_TEST(on_frame_empty_is_refused_even_when_on_event_is_filled) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_event = &filled_on_event;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_frame"));
}

GLINTFX_TEST(on_frame_empty_is_refused_even_when_render_and_event_are_both_filled) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_render = &filled_on_render;
    callbacks.on_event = &filled_on_event;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_frame"));
}

GLINTFX_TEST(on_render_empty_is_refused_once_on_frame_is_filled) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_frame = &filled_on_frame;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_render"));
}

GLINTFX_TEST(on_render_empty_is_refused_even_when_on_event_is_filled_too) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_frame = &filled_on_frame;
    callbacks.on_event = &filled_on_event;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_render"));
}

GLINTFX_TEST(on_frame_and_on_render_filled_with_on_event_empty_is_accepted) {
    gltfx_loop_callbacks callbacks{};
    callbacks.on_frame = &filled_on_frame;
    callbacks.on_render = &filled_on_render;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(!result.has_error());
}

GLINTFX_TEST(on_event_filled_is_refused_once_on_frame_and_on_render_are_both_filled) {
    // on_event is RESERVED for INPUT-EVENTS (W7, platform/loop/loop.hpp's
    // own header comment on gltfx_loop_callbacks::on_event) - a caller
    // that already filled it in is refused, never silently ignored.
    gltfx_loop_callbacks callbacks{};
    callbacks.on_frame = &filled_on_frame;
    callbacks.on_render = &filled_on_render;
    callbacks.on_event = &filled_on_event;

    const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view("on_event"));
}

namespace {

struct cell_t {
    bool on_frame_filled;
    bool on_render_filled;
    bool on_event_filled;
    std::string_view expected_rejected_value; // empty means "accepted"
};

// {empty, filled}^3, all eight cells, with the SAME expectation this
// file's own eight named cases above already assert one at a time -
// this is the same set, re-run through one loop so the count is
// printed (GODS_LAWS.md L-40).
constexpr std::array<cell_t, 8> k_cells{{
    {false, false, false, "on_frame"},
    {false, false, true, "on_frame"},
    {false, true, false, "on_frame"},
    {false, true, true, "on_frame"},
    {true, false, false, "on_render"},
    {true, false, true, "on_render"},
    {true, true, false, ""},
    {true, true, true, "on_event"},
}};

} // namespace

GLINTFX_TEST(the_empty_by_filled_cubed_matrix_is_enumerated_in_full) {
    std::size_t cells_checked = 0;

    for (const cell_t &cell : k_cells) {
        gltfx_loop_callbacks callbacks{};
        if (cell.on_frame_filled) {
            callbacks.on_frame = &filled_on_frame;
        }
        if (cell.on_render_filled) {
            callbacks.on_render = &filled_on_render;
        }
        if (cell.on_event_filled) {
            callbacks.on_event = &filled_on_event;
        }

        const glintfx::gltfx_rslt<void> result = validate_loop_callbacks(callbacks);

        if (cell.expected_rejected_value.empty()) {
            GLINTFX_CHECK(!result.has_error());
        } else {
            GLINTFX_CHECK(result.has_error());
            GLINTFX_CHECK(result.error().rejected_value() == cell.expected_rejected_value);
        }
        ++cells_checked;
    }

    GLINTFX_CHECK_EQ(cells_checked, k_cells.size());
    std::println("the_empty_by_filled_cubed_matrix_is_enumerated_in_full: {} of {} cell(s) checked",
                 cells_checked, k_cells.size());
}
