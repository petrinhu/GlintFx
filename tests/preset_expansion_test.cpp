// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <span>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_preset_table.hpp"
#include "platform/gl/preset_expansion.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// preset_expansion_test.cpp - GFX-PRESET, fatia P1 (docs/plano-w6b-fatias-5.md
// sec. 5.1/5.2, D-W6b-35 rule 5): the rows of a preset, minus the ones the
// consumer's own list already set, plus that list, in the order given - the
// EXPLICIT entry wins.
//
// The six cells of the plan (each one below, in the same order) plus the six of
// the fixed-capacity, cut-immune shape (D-P1-3, D-P1-4): every preset fits (3),
// and a repeated option gives one entry with the FIRST value - in an oversize
// list, in a small one, and next to the table's rows (3).
//
// RED, SEEN: before preset_expansion.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_entry;
using glintfx::platform::expand_preset;
using glintfx::platform::gfx_preset_row_at;
using glintfx::platform::gfx_preset_row_count;
using glintfx::platform::k_preset_expansion_capacity;
using glintfx::platform::preset_expansion;

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
constexpr std::int64_t k_vsync_off = 0;
constexpr std::int64_t k_vsync_on = 1;
constexpr std::int64_t k_auto_reason_none = 0;
constexpr std::int64_t k_auto_reason_on_battery = 1;
constexpr std::int64_t k_auto_reason_software_renderer = 2;
constexpr std::int64_t k_auto_reason_shared_gpu = 3;
constexpr std::int64_t k_auto_reason_dedicated_gpu = 4;
constexpr std::int64_t k_auto_reason_unknown_gpu = 5;

constexpr gltfx_gfx_option vsync = gltfx_gfx_option::vsync;
constexpr gltfx_gfx_option cap = gltfx_gfx_option::frame_rate_cap;

[[nodiscard]] bool same(const gltfx_gfx_option_entry &entry, gltfx_gfx_option id,
                        std::int64_t value) {
    return entry.id == id && entry.value == value;
}
} // namespace

GLINTFX_TEST(preset_expansion_every_cell) {
    int analyzed = 0;

    // power_saving with no explicit entry: the two rows of the table.
    {
        const preset_expansion r = expand_preset(k_preset_power_saving, {});
        GLINTFX_CHECK_EQ(r.count, std::size_t{2});
        GLINTFX_CHECK(same(r.entries[0], vsync, k_vsync_on));
        GLINTFX_CHECK(same(r.entries[1], cap, 30));
        ++analyzed;
    }
    // performance with frame_rate_cap = 120 explicit: vsync from the table, then
    // the 120 (the explicit entry wins over the table's 0).
    {
        const std::array<gltfx_gfx_option_entry, 1> given{{{cap, 120}}};
        const preset_expansion r = expand_preset(k_preset_performance, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{2});
        GLINTFX_CHECK(same(r.entries[0], vsync, k_vsync_on));
        GLINTFX_CHECK(same(r.entries[1], cap, 120));
        ++analyzed;
    }
    // manual with vsync = 0 explicit: only the explicit one (manual has no rows).
    {
        const std::array<gltfx_gfx_option_entry, 1> given{{{vsync, k_vsync_off}}};
        const preset_expansion r = expand_preset(k_preset_manual, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{1});
        GLINTFX_CHECK(same(r.entries[0], vsync, k_vsync_off));
        ++analyzed;
    }
    // automatic: only the explicit ones (the caller resolves it to a concrete
    // preset first).
    {
        const std::array<gltfx_gfx_option_entry, 1> given{{{cap, 60}}};
        const preset_expansion r = expand_preset(k_preset_automatic, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{1});
        GLINTFX_CHECK(same(r.entries[0], cap, 60));
        ++analyzed;
    }
    // balanced with BOTH rows given explicitly: only the explicit ones, in the
    // order given.
    {
        const std::array<gltfx_gfx_option_entry, 2> given{{{cap, 45}, {vsync, k_vsync_off}}};
        const preset_expansion r = expand_preset(k_preset_balanced, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{2});
        GLINTFX_CHECK(same(r.entries[0], cap, 45));
        GLINTFX_CHECK(same(r.entries[1], vsync, k_vsync_off));
        ++analyzed;
    }
    // A number outside the vocabulary: only the explicit ones.
    {
        const std::array<gltfx_gfx_option_entry, 1> given{{{vsync, k_vsync_on}}};
        const preset_expansion r = expand_preset(7, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{1});
        GLINTFX_CHECK(same(r.entries[0], vsync, k_vsync_on));
        ++analyzed;
    }
    // EVERY preset fits, with no explicit entry: the count is the preset's own
    // row count, and every entry is the table's row, value by value (the capacity
    // is never the limit).
    for (const std::int64_t preset :
         {k_preset_power_saving, k_preset_balanced, k_preset_performance}) {
        const preset_expansion r = expand_preset(preset, {});
        GLINTFX_CHECK_EQ(r.count, gfx_preset_row_count(preset));
        GLINTFX_CHECK(r.count <= r.entries.size());
        for (std::size_t i = 0; i < r.count; ++i) {
            const auto row = gfx_preset_row_at(preset, i);
            GLINTFX_CHECK(row.has_value() && same(r.entries[i], row->id, row->value));
        }
        ++analyzed;
    }
    // D-P1-4: the atom is IMMUNE to the cut by construction - at most ONE entry
    // per option, so the capacity (one per option of the registry) can never
    // overflow, and it does not depend on anyone validating the list first. A
    // repeated option keeps the FIRST occurrence (the same rule as
    // gl_context_desc_validation.cpp). A list with the same option more times
    // than the capacity allows gives ONE entry, with the value of the first.
    {
        std::array<gltfx_gfx_option_entry, k_preset_expansion_capacity + 3> many{};
        for (std::size_t i = 0; i < many.size(); ++i) {
            many[i] = gltfx_gfx_option_entry{cap, static_cast<std::int64_t>(7 + i)};
        }
        const preset_expansion r = expand_preset(k_preset_manual, many);
        GLINTFX_CHECK_EQ(r.count, std::size_t{1});
        GLINTFX_CHECK(same(r.entries[0], cap, 7));
        ++analyzed;
    }
    // Repeated options of a legal-sized list: each option once, the first value
    // wins, in the order of first appearance.
    {
        const std::array<gltfx_gfx_option_entry, 4> given{
            {{cap, 5}, {vsync, k_vsync_off}, {cap, 9}, {vsync, k_vsync_on}}};
        const preset_expansion r = expand_preset(k_preset_manual, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{2});
        GLINTFX_CHECK(same(r.entries[0], cap, 5));
        GLINTFX_CHECK(same(r.entries[1], vsync, k_vsync_off));
        ++analyzed;
    }
    // The table's rows and a repeated explicit option together: the table's vsync
    // stays (nobody named it), the cap comes once, with the first explicit value.
    {
        const std::array<gltfx_gfx_option_entry, 2> given{{{cap, 120}, {cap, 60}}};
        const preset_expansion r = expand_preset(k_preset_performance, given);
        GLINTFX_CHECK_EQ(r.count, std::size_t{2});
        GLINTFX_CHECK(same(r.entries[0], vsync, k_vsync_on));
        GLINTFX_CHECK(same(r.entries[1], cap, 120));
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 12);
    std::println("preset_expansion_test: {} celula(s) conferida(s)", analyzed);
}
