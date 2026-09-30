// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <print>
#include <string_view>
#include <vector>

#include "draw2d/draw_order.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// draw_order_test.cpp - R2D-BATCH, fatia B2b (docs/plano-w7d.md sec. 4.3 D-W7D-10, sec. 4.4 B2,
// sec. 4.5): the TOTAL ORDER a frame's pieces are painted in - by (layer, order of submission) -
// proved on the pure atom, before any GL exists.
//
// THE ORDER, frozen in the public header of the layer (draw2d/draw_layer.hpp, B4): inside one
// frame, between two flush() barriers, pieces are painted by (layer, order of submission). A lower
// layer is painted first, so a higher layer lands on top; two pieces on the SAME layer keep the
// order they were submitted in, every frame, on every system; a piece drawn without a layer is on
// layer 0.
//
// THE CLOSED SET OF EIGHT CELLS: how the layer of A relates to the layer of B - {no layer at all,
// equal, lower, higher} - times how they were submitted - {A then B, B then A}. Every cell, no
// sample, the count printed, zero fails (GODS_LAWS.md L-40, project L-20).
//
// RED, SEEN: before draw_order.{hpp,cpp} existed, this file's own #include line failed to compile;
// then, against a body that left the keys where they were, the cells that need a real order failed.

using glintfx::draw2d::draw_key;
using glintfx::draw2d::draw_key_before;
using glintfx::draw2d::k_default_draw_layer;
using glintfx::draw2d::sort_draw_keys;

namespace {
// How the layer of A relates to the layer of B.
enum class relation : std::uint8_t { none, equal, lower, higher };

struct layers {
    std::int32_t a;
    std::int32_t b;
};

// The two layers for one relation. `none` means neither piece named a layer: both are layer 0.
[[nodiscard]] layers layers_of(relation r) {
    switch (r) {
    case relation::none:
        return {k_default_draw_layer, k_default_draw_layer};
    case relation::equal:
        return {3, 3};
    case relation::lower:
        return {1, 2};
    case relation::higher:
        return {2, 1};
    }
    return {0, 0};
}

// Submits A and B in the given order (the FIRST submitted gets submission 0), sorts, and returns
// the names in painting order: "AB" or "BA".
[[nodiscard]] std::string_view painted(relation r, bool a_first) {
    const layers l = layers_of(r);
    // submission index: the first submitted is 0, the second 1.
    std::array<draw_key, 2> keys{};
    if (a_first) {
        keys[0] = draw_key{l.a, 0}; // A
        keys[1] = draw_key{l.b, 1}; // B
    } else {
        keys[0] = draw_key{l.b, 0}; // B is submitted first
        keys[1] = draw_key{l.a, 1}; // A second
    }
    sort_draw_keys(keys);
    // Which piece is the first painted? Its submission tells which one it was.
    const bool first_painted_is_a = a_first ? keys[0].submission == 0 : keys[0].submission == 1;
    return first_painted_is_a ? "AB" : "BA";
}

// What the order MUST be: the lower layer first; on the same layer, the submission.
[[nodiscard]] std::string_view expected(relation r, bool a_first) {
    switch (r) {
    case relation::lower:
        return "AB";
    case relation::higher:
        return "BA";
    case relation::none:
    case relation::equal:
        return a_first ? "AB" : "BA";
    }
    return "";
}
} // namespace

GLINTFX_TEST(draw_order_closed_eight_cell_enumeration) {
    int analyzed = 0;

    for (const relation r : {relation::none, relation::equal, relation::lower, relation::higher}) {
        for (const bool a_first : {true, false}) {
            GLINTFX_CHECK(painted(r, a_first) == expected(r, a_first));
            ++analyzed;
        }
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("draw_order_test: {} celula(s) conferida(s) (as 8 de ordem)", analyzed);
}

GLINTFX_TEST(draw_order_ties_and_reproducibility) {
    int analyzed = 0;

    // A thousand pieces on ONE layer come out in the order of submission (the tie-break).
    {
        std::vector<draw_key> keys;
        keys.reserve(1000);
        for (std::uint32_t i = 0; i < 1000; ++i) {
            keys.push_back(draw_key{7, i});
        }
        // Hand them to the sort in a scrambled order, so that "already in order" proves nothing.
        std::reverse(keys.begin(), keys.end());
        std::rotate(keys.begin(), keys.begin() + 313, keys.end());
        sort_draw_keys(keys);
        bool in_submission_order = true;
        std::uint32_t first_out_of_order = 0;
        for (std::uint32_t i = 0; i < keys.size(); ++i) {
            if (keys[i].submission != i) {
                in_submission_order = false;
                first_out_of_order = i;
                break;
            }
        }
        GLINTFX_CHECK_EQ(first_out_of_order, std::uint32_t{0});
        GLINTFX_CHECK(in_submission_order);
        ++analyzed;
    }
    // Many layers, submitted in a scrambled order: the result is the total order, layer first, then
    // submission - and the same however the input was arranged (reproducible).
    {
        std::vector<draw_key> base;
        base.reserve(300);
        for (std::uint32_t i = 0; i < 300; ++i) {
            base.push_back(draw_key{static_cast<std::int32_t>(i % 7) - 3, i});
        }
        std::vector<draw_key> forward = base;
        std::vector<draw_key> backward = base;
        std::reverse(backward.begin(), backward.end());
        sort_draw_keys(forward);
        sort_draw_keys(backward);
        bool same = forward.size() == backward.size();
        for (std::size_t i = 0; same && i < forward.size(); ++i) {
            same = forward[i].layer == backward[i].layer &&
                   forward[i].submission == backward[i].submission;
        }
        GLINTFX_CHECK(same);
        // and it IS the total order: every neighbour pair is ordered by (layer, submission).
        bool ordered = true;
        for (std::size_t i = 1; ordered && i < forward.size(); ++i) {
            ordered = draw_key_before(forward[i - 1], forward[i]);
        }
        GLINTFX_CHECK(ordered);
        ++analyzed;
    }
    // The extremes of the layer are ordinary layers: the lowest int32 first, the highest last,
    // negative before zero before positive.
    {
        std::array<draw_key, 5> keys{{{0, 0},
                                      {std::numeric_limits<std::int32_t>::max(), 1},
                                      {-1, 2},
                                      {std::numeric_limits<std::int32_t>::min(), 3},
                                      {1, 4}}};
        sort_draw_keys(keys);
        GLINTFX_CHECK(keys[0].layer == std::numeric_limits<std::int32_t>::min());
        GLINTFX_CHECK(keys[1].layer == -1 && keys[2].layer == 0 && keys[3].layer == 1);
        GLINTFX_CHECK(keys[4].layer == std::numeric_limits<std::int32_t>::max());
        ++analyzed;
    }
    // The order relation itself: strict, on (layer, submission), never equal for two different
    // submissions; equal keys are not "before" each other (a strict weak order for the sort).
    {
        GLINTFX_CHECK(draw_key_before(draw_key{1, 9}, draw_key{2, 0})); // the layer decides
        GLINTFX_CHECK(!draw_key_before(draw_key{2, 0}, draw_key{1, 9}));
        GLINTFX_CHECK(draw_key_before(draw_key{4, 1}, draw_key{4, 2})); // the submission decides
        GLINTFX_CHECK(!draw_key_before(draw_key{4, 2}, draw_key{4, 1}));
        GLINTFX_CHECK(!draw_key_before(draw_key{4, 2}, draw_key{4, 2})); // strict
        ++analyzed;
    }
    // Empty and single-element lists are untouched and do not fail.
    {
        std::vector<draw_key> none;
        sort_draw_keys(none);
        GLINTFX_CHECK(none.empty());
        std::array<draw_key, 1> one{{{5, 42}}};
        sort_draw_keys(one);
        GLINTFX_CHECK(one[0].layer == 5 && one[0].submission == 42);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 5);
    std::println("draw_order_test: {} celula(s) conferida(s) (empate, reprodutibilidade, extremos)",
                 analyzed);
}
