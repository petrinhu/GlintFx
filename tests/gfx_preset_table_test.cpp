// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <span>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_preset_table.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_preset_table_test.cpp - GFX-PRESET, fatia P1 (docs/plano-w6b-fatias-5.md
// sec. 5.1/5.2, D-W6b-34, /var/tmp/cto-w7d/PLANO-errata.md D-P1-2): the closed
// enumeration of the preset table
// - every row of every preset, value by value, every number outside the
// vocabulary, and the ABSENCE the row accessor answers for anything out of
// range (std::nullopt). The public degradation of E1 ({suggested_preset,
// manual}) is the public function's own policy and is proved in P3, where the
// public names exist.
//
// THE CELLS, ALL OF THEM, NO SAMPLE (GODS_LAWS.md L-40, project L-20): 3
// concrete presets x 2 rows (6), the five numbers with no rows (5), the size of
// one preset (1) = 12, plus the accessors the public
// gltfx_gfx_preset_row_count()/gltfx_gfx_preset_row_at() build on (P3): the
// count of every preset in the vocabulary and outside it (2), the in-range row
// read (2) and the absence, one cell for each of the two ways of going out of
// range (2).
//
// RED, SEEN: before gfx_preset_table.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::gltfx_gfx_option;
using glintfx::platform::gfx_preset_row;
using glintfx::platform::gfx_preset_row_at;
using glintfx::platform::gfx_preset_row_count;
using glintfx::platform::gfx_preset_rows;

namespace {
// THE LITERAL NUMBERS DECIDED (D-W6b-44, the comments of
// gfx_option_registry.hpp:79-98; D-P1-5): asserted here as numbers, NEVER
// through the constants the code itself uses, so that a renumbering of the
// internal header - or the swap to the public k_gltfx_* constants in P3 - is
// caught.
constexpr std::int64_t k_preset_manual = 0;
constexpr std::int64_t k_preset_power_saving = 1;
constexpr std::int64_t k_preset_balanced = 2;
constexpr std::int64_t k_preset_performance = 3;
constexpr std::int64_t k_preset_automatic = 4;
constexpr std::int64_t k_vsync_on = 1;

// One cell: preset `preset`, row `row_index`, expecting `{id, value}` exactly.
[[nodiscard]] bool row_is(std::span<const gfx_preset_row> rows, std::size_t row_index,
                          std::int64_t preset, gltfx_gfx_option id, std::int64_t value) {
    return row_index < rows.size() && rows[row_index].preset == preset &&
           rows[row_index].id == id && rows[row_index].value == value;
}
} // namespace

GLINTFX_TEST(gfx_preset_table_every_row_of_every_preset) {
    int analyzed = 0;

    // power_saving = {vsync = on, frame_rate_cap = 30} (D-W6b-34; the 30 Hz has
    // its source in docs/plano-w6b-fatias-5.md sec. 1).
    {
        const auto rows = gfx_preset_rows(k_preset_power_saving);
        GLINTFX_CHECK(row_is(rows, 0, k_preset_power_saving, gltfx_gfx_option::vsync, k_vsync_on));
        ++analyzed;
        GLINTFX_CHECK(row_is(rows, 1, k_preset_power_saving, gltfx_gfx_option::frame_rate_cap, 30));
        ++analyzed;
    }
    // balanced = {vsync = on, frame_rate_cap = 0} and performance = the SAME two
    // rows today (they differ when the library grows options that can be spent on
    // quality).
    {
        const auto rows = gfx_preset_rows(k_preset_balanced);
        GLINTFX_CHECK(row_is(rows, 0, k_preset_balanced, gltfx_gfx_option::vsync, k_vsync_on));
        ++analyzed;
        GLINTFX_CHECK(row_is(rows, 1, k_preset_balanced, gltfx_gfx_option::frame_rate_cap, 0));
        ++analyzed;
    }
    {
        const auto rows = gfx_preset_rows(k_preset_performance);
        GLINTFX_CHECK(row_is(rows, 0, k_preset_performance, gltfx_gfx_option::vsync, k_vsync_on));
        ++analyzed;
        GLINTFX_CHECK(row_is(rows, 1, k_preset_performance, gltfx_gfx_option::frame_rate_cap, 0));
        ++analyzed;
    }

    // The five numbers with NO rows: manual, automatic (it resolves to a concrete
    // preset first) and three outside the vocabulary (below, just above and far
    // above it).
    GLINTFX_CHECK(gfx_preset_rows(k_preset_manual).empty());
    ++analyzed;
    GLINTFX_CHECK(gfx_preset_rows(k_preset_automatic).empty());
    ++analyzed;
    GLINTFX_CHECK(gfx_preset_rows(-1).empty());
    ++analyzed;
    GLINTFX_CHECK(gfx_preset_rows(5).empty());
    ++analyzed;
    GLINTFX_CHECK(gfx_preset_rows(99).empty());
    ++analyzed;

    // The size of one preset.
    GLINTFX_CHECK_EQ(gfx_preset_rows(k_preset_power_saving).size(), std::size_t{2});
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 12);
    std::println("gfx_preset_table_test: {} celula(s) conferida(s) (tabela)", analyzed);
}

GLINTFX_TEST(gfx_preset_table_row_accessors_and_the_out_of_range_absence) {
    int analyzed = 0;

    // The count: 2 for each concrete preset, 0 for manual, automatic and every
    // number outside.
    GLINTFX_CHECK_EQ(gfx_preset_row_count(k_preset_power_saving), std::size_t{2});
    GLINTFX_CHECK_EQ(gfx_preset_row_count(k_preset_balanced), std::size_t{2});
    GLINTFX_CHECK_EQ(gfx_preset_row_count(k_preset_performance), std::size_t{2});
    ++analyzed;
    GLINTFX_CHECK_EQ(gfx_preset_row_count(k_preset_manual), std::size_t{0});
    GLINTFX_CHECK_EQ(gfx_preset_row_count(k_preset_automatic), std::size_t{0});
    GLINTFX_CHECK_EQ(gfx_preset_row_count(std::numeric_limits<std::int64_t>::min()),
                     std::size_t{0});
    GLINTFX_CHECK_EQ(gfx_preset_row_count(std::numeric_limits<std::int64_t>::max()),
                     std::size_t{0});
    ++analyzed;

    // The row at an in-range index: which option it sets, and to what.
    {
        const auto entry = gfx_preset_row_at(k_preset_power_saving, 0);
        GLINTFX_CHECK(entry.has_value());
        GLINTFX_CHECK(entry->id == gltfx_gfx_option::vsync && entry->value == k_vsync_on);
        ++analyzed;
    }
    {
        const auto entry = gfx_preset_row_at(k_preset_power_saving, 1);
        GLINTFX_CHECK(entry.has_value());
        GLINTFX_CHECK(entry->id == gltfx_gfx_option::frame_rate_cap && entry->value == 30);
        ++analyzed;
    }

    // Out of range, in either of the two ways, is ABSENCE - never a made-up entry
    // (the policy of what the public function hands back is P3's, D-P1-2).
    GLINTFX_CHECK(!gfx_preset_row_at(99, 0).has_value()); // the preset is outside the vocabulary
    ++analyzed;
    GLINTFX_CHECK(!gfx_preset_row_at(k_preset_balanced, 2).has_value()); // the row is past the end
    GLINTFX_CHECK(!gfx_preset_row_at(k_preset_manual, 0).has_value());   // manual has no rows
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 6);
    std::println("gfx_preset_table_test: {} celula(s) conferida(s) (acessores e ausencia)",
                 analyzed);
}
