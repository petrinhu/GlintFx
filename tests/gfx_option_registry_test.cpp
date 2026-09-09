// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_option_registry_test.cpp - C-OPT (docs/plano-w6b-placa-e-laco.md
// fatia 2a, D-W6b-16/17, GODS_LAWS.md L-17/L-20/L-26/L-40): the TDD
// red/green witness for glintfx::gltfx_gfx_option and its four public
// discovery functions (include/glintfx/platform/gl/gfx_option.hpp).
//
// RED, SEEN (GODS_LAWS.md L-20): before gfx_option.hpp and gfx_option_
// registry.{hpp,cpp} existed, this file's own #include line above
// failed to compile - every symbol this file names was undeclared.
// That failure IS this fatia's own red, the same "compile failure
// counts as a legitimate red for a foundational enumeration" precedent
// gfss_property_registry_test.cpp already set for this project's other
// append-only registry. Green is this file compiling and every case
// below passing against the real, ratified table (docs/plano-w6b-
// placa-e-laco.md sec. 11.2).
//
// THIS FILE NEVER INCLUDES gfx_option_registry.hpp (the internal
// table): k_expected_table below is its OWN, independently-typed set
// of eight rows (tests/gfx_option_registry_test_table.inc), cross-
// checked field-by-field against sec. 11.2's own table during this
// fatia's own delivery - reading the internal table back would only
// ever agree with itself (the same reasoning gfss_property_registry_
// test.cpp's own header comment already gives for its own k_expected_
// table). This test needs no "${PROJECT_SOURCE_DIR}/src" include dir
// for exactly that reason - only the PUBLIC header is exercised.

namespace {

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_at;
using glintfx::gltfx_gfx_option_by_name;
using glintfx::gltfx_gfx_option_count;
using glintfx::gltfx_gfx_option_describe;
using glintfx::gltfx_gfx_option_info;
using glintfx::gltfx_gfx_option_kind;
using glintfx::gltfx_gfx_option_when;

struct expected_option_row {
    gltfx_gfx_option id = gltfx_gfx_option::vsync;
    std::uint16_t expected_numeric_id = 0;
    std::string_view expected_name;
    gltfx_gfx_option_kind expected_kind = gltfx_gfx_option_kind::toggle;
    gltfx_gfx_option_when expected_when = gltfx_gfx_option_when::read_only;
    std::int64_t expected_min = 0;
    std::int64_t expected_max = 0;
    std::int64_t expected_default = 0;
};

// THE STABILITY/CORRECTNESS TABLE - see this file's own header comment
// for why it is its own, independently-typed set of rows rather than a
// read of the internal table.
constexpr std::array<expected_option_row, 8> k_expected_table{{
#include "gfx_option_registry_test_table.inc"
}};

} // namespace

GLINTFX_TEST(gltfx_gfx_option_registry_has_the_ratified_count_of_8_options) {
    GLINTFX_CHECK_EQ(gltfx_gfx_option_count(), static_cast<std::size_t>(8));
    GLINTFX_CHECK_EQ(k_expected_table.size(), gltfx_gfx_option_count());
    std::println("gltfx_gfx_option_registry_has_the_ratified_count_of_8_options: {} option(s)",
                 gltfx_gfx_option_count());
}

// The enumeration this whole file enumerates CLOSED (GODS_LAWS.md
// L-40): every row of k_expected_table is checked against gltfx_gfx_
// option_at()/gltfx_gfx_option_describe() - the same value, reached
// two different ways, has to agree both times.
GLINTFX_TEST(gltfx_gfx_option_id_is_append_only_and_matches_at_and_describe_for_every_option) {
    std::size_t swept = 0;
    for (std::size_t index = 0; index < k_expected_table.size(); ++index) {
        const expected_option_row &row = k_expected_table[index];
        GLINTFX_CHECK_EQ(static_cast<std::uint16_t>(row.id), row.expected_numeric_id);

        const gltfx_gfx_option_info by_index = gltfx_gfx_option_at(index);
        const gltfx_gfx_option_info by_id = gltfx_gfx_option_describe(row.id);

        GLINTFX_CHECK(by_index.id == row.id);
        GLINTFX_CHECK(by_index.name == row.expected_name);
        GLINTFX_CHECK(by_index.kind == row.expected_kind);
        GLINTFX_CHECK(by_index.when == row.expected_when);
        GLINTFX_CHECK_EQ(by_index.min_value, row.expected_min);
        GLINTFX_CHECK_EQ(by_index.max_value, row.expected_max);
        GLINTFX_CHECK_EQ(by_index.default_value, row.expected_default);

        GLINTFX_CHECK(by_id.id == by_index.id);
        GLINTFX_CHECK(by_id.name == by_index.name);
        GLINTFX_CHECK(by_id.kind == by_index.kind);
        GLINTFX_CHECK(by_id.when == by_index.when);
        GLINTFX_CHECK_EQ(by_id.min_value, by_index.min_value);
        GLINTFX_CHECK_EQ(by_id.max_value, by_index.max_value);
        GLINTFX_CHECK_EQ(by_id.default_value, by_index.default_value);

        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfx_option_count());
    std::println("gltfx_gfx_option_id_is_append_only_and_matches_at_and_describe_for_every_"
                 "option: {} id(s) checked",
                 swept);
}

// D-W6b-16 (1)'s own "o nome de cada id e contrato de DADO": every
// option's name has to make the round trip through gltfx_gfx_option_
// by_name() back to its own id.
GLINTFX_TEST(gltfx_gfx_option_name_makes_the_round_trip_through_by_name_for_every_option) {
    std::size_t swept = 0;
    for (const expected_option_row &row : k_expected_table) {
        const glintfx::gltfx_rslt<gltfx_gfx_option> found =
            gltfx_gfx_option_by_name(row.expected_name);
        GLINTFX_CHECK(found.has_value());
        if (found.has_value()) {
            GLINTFX_CHECK(found.value() == row.id);
        }
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfx_option_count());
    std::println("gltfx_gfx_option_name_makes_the_round_trip_through_by_name_for_every_option: "
                 "{} name(s) checked",
                 swept);
}

// docs/api-conventions.md R4: a name this build's table does not
// recognize is a genuine failure (not_found), never a fabricated id.
GLINTFX_TEST(gltfx_gfx_option_by_name_reports_not_found_for_an_unknown_name) {
    const glintfx::gltfx_rslt<gltfx_gfx_option> found = gltfx_gfx_option_by_name("does-not-exist");
    GLINTFX_CHECK(found.has_error());
    GLINTFX_CHECK(found.err().code() == glintfx::gltfx_err_code::not_found);
}

// D-W6b-16 (1)'s own shape rule: `default_value` always sits inside
// its own [min_value, max_value] - checked for every option, not
// sampled.
GLINTFX_TEST(gltfx_gfx_option_default_sits_inside_its_own_min_max_for_every_option) {
    std::size_t swept = 0;
    for (const expected_option_row &row : k_expected_table) {
        const gltfx_gfx_option_info info = gltfx_gfx_option_describe(row.id);
        GLINTFX_CHECK(info.min_value <= info.default_value);
        GLINTFX_CHECK(info.default_value <= info.max_value);
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfx_option_count());
    std::println("gltfx_gfx_option_default_sits_inside_its_own_min_max_for_every_option: {} "
                 "option(s) checked",
                 swept);
}

// docs/api-conventions.md R4, applied to gltfx_gfx_option_at()/gltfx_
// gfx_option_describe(): an out-of-range index or an id outside this
// build's own table degrades to a default-constructed info whose own
// `when` is `read_only` - NEVER `open_only`, so a consumer that
// forwards a stale/unknown id into open-time validation never gets
// treated as if the library still expected it fixed on the window
// (gfx_option_validation.cpp's own `already_open` rule only bites
// `open_only`; `read_only` is unconditionally refused either way, the
// stricter, safer default).
GLINTFX_TEST(gltfx_gfx_option_degrades_to_read_only_never_open_only_for_an_unknown_option) {
    const gltfx_gfx_option_info out_of_range_index =
        gltfx_gfx_option_at(gltfx_gfx_option_count() + 1000);
    GLINTFX_CHECK(out_of_range_index.when == gltfx_gfx_option_when::read_only);
    GLINTFX_CHECK(out_of_range_index.name.empty());

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: simulates a value a
    // NEWER glintfx produced that this build's table has never heard of - the same pattern
    // gfss_property_registry_test.cpp's own out-of-range case already uses.
    const auto out_of_range_id =
        static_cast<gltfx_gfx_option>(static_cast<std::uint16_t>(gltfx_gfx_option_count()) + 1000);
    const gltfx_gfx_option_info unknown_id = gltfx_gfx_option_describe(out_of_range_id);
    GLINTFX_CHECK(unknown_id.when == gltfx_gfx_option_when::read_only);
    GLINTFX_CHECK(unknown_id.name.empty());
}
