// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/shadow_group.hpp"
#include "draw2d/triangle_batch.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// shadow_group_test.cpp - R2D-SPRITE-SHADE, slice H3a (docs/auditoria-api-sprite-shade.md sec. 4.2,
// D-W8-319): the stencil sequence of a batch, on pure data (no GL, no window). The expected passes
// are written here by hand from the rule of the audit, never read from the code:
//   - the closed space of one run, {ordinary, shadow} x {a shadow came before it, none came}: all
//     four cells, counted and printed;
//   - one segment walked run by run (ordinary, shadow, ordinary, shadow, ordinary) through the
//     atom, each run compared with what the rule gives;
//   - a segment with no shadow never touches the stencil.
// NOT proved here: that GL draws these passes (the pixel cells of H3b), and that the renderer
// starts every segment from "no shadow came" (H3b, with the barrier).

using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_screen;
using glintfx::draw2d::batch_state;
using glintfx::draw2d::draw_run;
using glintfx::draw2d::passes_of_run;
using glintfx::draw2d::quad_corners_pixel;
using glintfx::draw2d::run_passes;
using glintfx::draw2d::shadow_came_after;
using glintfx::draw2d::stencil_pass;
using glintfx::draw2d::stencil_role;
using glintfx::draw2d::triangle_batch;

namespace {

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr batch_state k_ordinary{1, 1, 0, stencil_role::ordinary};
constexpr batch_state k_shadow{1, 1, 0, stencil_role::shadow};

// One run as the rule says it: whether the stencil is cleared first, and its passes in order.
struct expected_run {
    bool clear_stencil_first = false;
    std::size_t count = 0;
    std::array<stencil_pass, 2> passes{};
};

constexpr expected_run k_only_paints{false, 1, {stencil_pass::plain, stencil_pass::plain}};
constexpr expected_run k_paints_then_releases{
    false, 2, {stencil_pass::plain, stencil_pass::release_marks}};
constexpr expected_run k_clears_then_marks{
    true, 1, {stencil_pass::mark_shadow, stencil_pass::plain}};
constexpr expected_run k_marks{false, 1, {stencil_pass::mark_shadow, stencil_pass::plain}};

[[nodiscard]] bool same_run(const run_passes &got, const expected_run &want) {
    if (got.clear_stencil_first != want.clear_stencil_first || got.count != want.count) {
        return false;
    }
    for (std::size_t i = 0; i < want.count; ++i) {
        if (got.passes[i] != want.passes[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] quad_corners_pixel square_at(float x) {
    return quad_corners_pixel{gltfx_vec2_screen{x, 0.0F}, gltfx_vec2_screen{x + 1.0F, 0.0F},
                              gltfx_vec2_screen{x + 1.0F, 1.0F}, gltfx_vec2_screen{x, 1.0F}};
}

// Adds one quad per state, in order, at x = 0, 1, 2...; true when every one was added.
[[nodiscard]] bool add_in_order(triangle_batch &batch, std::span<const batch_state> states) {
    bool added = true;
    for (std::size_t i = 0; i < states.size(); ++i) {
        added = batch.add_quad(square_at(static_cast<float>(i)), k_white, states[i]) && added;
    }
    return added;
}

// Walks the runs of one segment from "no shadow came", as the renderer will: how many runs get
// exactly the passes `want` gives them, and whether a shadow came by the end.
struct walk_result {
    std::size_t as_ruled = 0;
    bool shadow_came = false;
};

[[nodiscard]] walk_result walk_segment(std::span<const draw_run> runs,
                                       std::span<const expected_run> want) {
    walk_result result;
    for (std::size_t i = 0; i < runs.size() && i < want.size(); ++i) {
        if (same_run(passes_of_run(runs[i].state.stencil, result.shadow_came), want[i])) {
            ++result.as_ruled;
        }
        result.shadow_came = shadow_came_after(runs[i].state.stencil, result.shadow_came);
    }
    return result;
}

// One cell of the closed space of a run: its role, whether a shadow came before it, the passes the
// rule gives it, and whether a shadow has come once it is drawn.
struct cell {
    stencil_role role = stencil_role::ordinary;
    bool shadow_came = false;
    expected_run want;
    bool came_after = false;
};

[[nodiscard]] bool cell_holds(const cell &one) {
    return same_run(passes_of_run(one.role, one.shadow_came), one.want) &&
           shadow_came_after(one.role, one.shadow_came) == one.came_after;
}

} // namespace

// The closed space of one run, every cell: an ordinary run before any shadow only paints; after a
// shadow it paints and then releases; the first shadow clears the stencil and marks; a later
// shadow marks and keeps the marks. A shadow has come once a shadow run is drawn, and stays come.
GLINTFX_TEST(shadow_group_every_run_in_the_closed_space_of_role_and_history) {
    const std::array<cell, 4> cells{{
        {stencil_role::ordinary, false, k_only_paints, false},
        {stencil_role::ordinary, true, k_paints_then_releases, true},
        {stencil_role::shadow, false, k_clears_then_marks, true},
        {stencil_role::shadow, true, k_marks, true},
    }};
    std::size_t holding = 0;
    for (const cell &one : cells) {
        holding += cell_holds(one) ? 1U : 0U;
    }
    std::println("shadow_group_test: celulas encontradas={} conferidas={}", cells.size(), holding);
    GLINTFX_CHECK_EQ(holding, std::size_t{4});
}

// One segment of five runs, with the same program and texture: the stencil role alone splits
// them. The first ordinary run only paints; the first shadow clears and marks; the ordinary run
// after it paints and releases; the second shadow marks without clearing; the last ordinary run
// paints and releases.
GLINTFX_TEST(shadow_group_the_stencil_sequence_of_a_batch) {
    triangle_batch batch;
    const std::array<batch_state, 5> order{k_ordinary, k_shadow, k_ordinary, k_shadow, k_ordinary};
    GLINTFX_CHECK(add_in_order(batch, order));
    const std::array<expected_run, 5> want{k_only_paints, k_clears_then_marks,
                                           k_paints_then_releases, k_marks, k_paints_then_releases};
    GLINTFX_CHECK_EQ(batch.runs().size(), want.size());
    const walk_result walked = walk_segment(batch.runs(), want);
    GLINTFX_CHECK_EQ(walked.as_ruled, want.size());
    GLINTFX_CHECK(walked.shadow_came);
}

// A segment with no shadow: three runs (three textures), and every one only paints, so the
// stencil is never cleared nor tested.
GLINTFX_TEST(shadow_group_a_segment_with_no_shadow_never_touches_the_stencil) {
    triangle_batch batch;
    const std::array<batch_state, 3> order{batch_state{1, 1, 0, stencil_role::ordinary},
                                           batch_state{1, 2, 0, stencil_role::ordinary},
                                           batch_state{1, 3, 0, stencil_role::ordinary}};
    GLINTFX_CHECK(add_in_order(batch, order));
    const std::array<expected_run, 3> want{k_only_paints, k_only_paints, k_only_paints};
    GLINTFX_CHECK_EQ(batch.runs().size(), want.size());
    const walk_result walked = walk_segment(batch.runs(), want);
    GLINTFX_CHECK_EQ(walked.as_ruled, want.size());
    GLINTFX_CHECK(!walked.shadow_came);
}
