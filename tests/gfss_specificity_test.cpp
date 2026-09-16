// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <limits>
#include <print>
#include <string>
#include <string_view>

#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfss/specificity.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_specificity_test.cpp - GFSS-SPECIFICITY (docs/plano-w6-folha-de-
// estilo.md fatia S-1, TODO.md, GODS_LAWS.md L-20/L-27/L-40): the TDD
// red/green witness for specificity.hpp's own three functions - see
// that file's own header comment for the design rationale each check
// below proves.
using glintfx::style::detail::gfss_combinator_count;
using glintfx::style::detail::gfss_combinator_table;
using glintfx::style::detail::gfss_simple_selector;
using glintfx::style::detail::gfss_simple_selector_kind;
using glintfx::style::detail::gfss_specificity;
using glintfx::style::detail::parse_selector_list;
using glintfx::style::detail::saturating_increment;
using glintfx::style::detail::specificity_of_complex;

namespace {

// One case per fixed(-but-parsed) selector text, asserting the WHOLE
// selector's own total specificity - the shape every test below
// shares: parse, then read exactly one complex selector out of the
// resulting list.
[[nodiscard]] gfss_specificity specificity_of_the_one_selector_in(std::string_view text) {
    const auto result = parse_selector_list(text);
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return gfss_specificity{};
    }
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    if (result.value.selectors.size() != 1) {
        return gfss_specificity{};
    }
    return specificity_of_complex(result.value.selectors.front());
}

} // namespace

// FIRST ASSERTION (this fatia's own row in docs/plano-w6-folha-de-
// estilo.md SS4, captured BEFORE specificity.cpp existed - GODS_LAWS.md
// L-20 "veja o teste falhar"): "#a.b c" is one id, one class, one type -
// #a.b is a compound (id + class), c is a second compound (one type)
// joined by a descendant combinator, which contributes to no column of
// its own (specificity.hpp's own header comment).
GLINTFX_TEST(gltfx_gfss_specificity_of_id_class_and_descendant_type_is_one_one_one) {
    const gfss_specificity total = specificity_of_the_one_selector_in("#a.b c");
    GLINTFX_CHECK_EQ(total.ids, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(total.classes, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(total.types, static_cast<std::uint32_t>(1));
}

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado") - the 8 simple-selector
// SHAPES selector_ast.hpp's own gfss_simple_selector_kind enumerates
// (that file's own header comment: NOT X-macro'd, fixed by the
// grammar's own structure), each built DIRECTLY (never through the
// parser - this test is about specificity_of_simple() alone, not about
// parsing) with the ONE column it is supposed to touch.
// specificity_of_simple()'s own switch in specificity.cpp has no
// `default` label - a 9th kind added with no matching row HERE still
// compiles (this table is plain data), but a 9th kind added with no
// matching CASE in specificity.cpp fails the BUILD under -Werror
// (-Wswitch), which is the real enforcement; this static_assert instead
// catches the far more likely slip, a row forgotten in THIS table
// while specificity.cpp's own switch was updated.
namespace {

struct simple_selector_kind_case {
    gfss_simple_selector_kind kind;
    std::string_view name;
    gfss_specificity expected;
};

// `pseudo_function` is represented here by "nth-child", never "not" -
// `:not()` is the ONE exception to "a pseudo-function is an ordinary
// class-column unit" (specificity.hpp's own header comment), and gets
// its own three-case test below rather than a row in this generic
// sweep.
constexpr std::array<simple_selector_kind_case, 8> k_simple_selector_kind_cases{{
    {gfss_simple_selector_kind::universal, "",
     gfss_specificity{.ids = 0, .classes = 0, .types = 0}},
    {gfss_simple_selector_kind::type, "div", gfss_specificity{.ids = 0, .classes = 0, .types = 1}},
    {gfss_simple_selector_kind::class_selector, "primary",
     gfss_specificity{.ids = 0, .classes = 1, .types = 0}},
    {gfss_simple_selector_kind::id_selector, "ok",
     gfss_specificity{.ids = 1, .classes = 0, .types = 0}},
    {gfss_simple_selector_kind::pseudo_class, "hover",
     gfss_specificity{.ids = 0, .classes = 1, .types = 0}},
    {gfss_simple_selector_kind::pseudo_function, "nth-child",
     gfss_specificity{.ids = 0, .classes = 1, .types = 0}},
    {gfss_simple_selector_kind::pseudo_element, "before",
     gfss_specificity{.ids = 0, .classes = 0, .types = 1}},
    {gfss_simple_selector_kind::attribute, "id",
     gfss_specificity{.ids = 0, .classes = 1, .types = 0}},
}};

static_assert(k_simple_selector_kind_cases.size() == 8,
              "GODS_LAWS.md L-40: selector_ast.hpp's gfss_simple_selector_kind changed - update "
              "this table (and specificity.cpp's own switch) to match");

} // namespace

GLINTFX_TEST(gltfx_gfss_specificity_of_simple_touches_exactly_one_column_per_kind) {
    std::size_t swept = 0;
    for (const auto &kind_case : k_simple_selector_kind_cases) {
        gfss_simple_selector simple{};
        simple.kind = kind_case.kind;
        simple.name = kind_case.name;
        const gfss_specificity actual = glintfx::style::detail::specificity_of_simple(simple);
        GLINTFX_CHECK(actual == kind_case.expected);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_simple_selector_kind_cases.size());
    std::println(
        "gltfx_gfss_specificity_of_simple_touches_exactly_one_column_per_kind: {} kind(s) checked",
        swept);
}

// ENUMERATION: the four combinators selector_ast.hpp's own gfss_
// combinator_table names, swept the SAME way gfss_selector_parse_test.
// cpp's own gltfx_gfss_parse_selector_list_recognizes_every_combinator
// already does - "a<delim>b" is always two type selectors (types = 2
// total), whatever the combinator: if a combinator ever contributed to
// a column of its own, this total would drift away from 2 for whichever
// one it was, which is exactly the defect this sweep exists to catch.
GLINTFX_TEST(gltfx_gfss_specificity_every_combinator_contributes_nothing_of_its_own) {
    static_assert(gfss_combinator_count == 4,
                  "GODS_LAWS.md L-40: selector_ast.hpp's combinator table changed - update this "
                  "sweep to match");

    std::size_t swept = 0;
    for (const auto &entry : gfss_combinator_table) {
        const std::string text = std::string("a") + entry.delimiter + "b";
        const gfss_specificity total = specificity_of_the_one_selector_in(text);
        GLINTFX_CHECK_EQ(total.ids, static_cast<std::uint32_t>(0));
        GLINTFX_CHECK_EQ(total.classes, static_cast<std::uint32_t>(0));
        GLINTFX_CHECK_EQ(total.types, static_cast<std::uint32_t>(2));
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gfss_combinator_count);
    std::println(
        "gltfx_gfss_specificity_every_combinator_contributes_nothing_of_its_own: {} combinator(s) "
        "checked",
        swept);
}

// `:not()` CASE 1/3 - ONE argument (dossie SS2.2 / TODO.md:401): it
// contributes the specificity of that ONE argument, never a SUM with an
// extra "+1 class for being a pseudo-class" on top - "div:not(.a.b)" is
// one type (the "div") plus the two classes of ":not()"'s own argument,
// never three classes.
GLINTFX_TEST(gltfx_gfss_specificity_not_with_one_argument_contributes_that_arguments_own_weight) {
    const gfss_specificity total = specificity_of_the_one_selector_in("div:not(.a.b)");
    GLINTFX_CHECK_EQ(total.ids, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(total.classes, static_cast<std::uint32_t>(2));
    GLINTFX_CHECK_EQ(total.types, static_cast<std::uint32_t>(1));
}

// `:not()` CASE 2/3 - a LIST of arguments: it contributes the MOST
// SPECIFIC one, compared LEXICOGRAPHICALLY (never by "most simple
// selectors") - ":not(#a, .b.c.d)" has one argument with one id and one
// with three classes; (1,0,0) beats (0,3,0) because the id column is
// compared FIRST, even though the second argument names more simple
// selectors. Mutating `max` to a sum would report (1,3,0) instead - the
// exact mutation docs/plano-w6-folha-de-estilo.md SS4 names for this
// row.
GLINTFX_TEST(
    gltfx_gfss_specificity_not_with_a_list_contributes_the_most_specific_one_lexicographically) {
    const gfss_specificity total = specificity_of_the_one_selector_in(":not(#a, .b.c.d)");
    GLINTFX_CHECK_EQ(total.ids, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(total.classes, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(total.types, static_cast<std::uint32_t>(0));
}

// `:not()` CASE 3/3 - NESTED `:not()` (bounded by selector_parse.cpp's
// own k_max_not_nesting_depth, proved elsewhere by gfss_selector_parse_
// test.cpp; this case only proves the SPECIFICITY of one level of
// nesting propagates correctly, never that the depth limit itself is
// enforced): ":not(:not(.a))" - the outer `:not()` contributes nothing
// of its own, its ONE argument is the complex selector ":not(.a)",
// whose OWN specificity is, by the SAME rule one level down, the inner
// `:not()`'s own argument weight - one class, from ".a". A mutation that
// added a class for EACH `:not()` level (instead of propagating the
// innermost weight unchanged) would report two classes here, not one.
GLINTFX_TEST(gltfx_gfss_specificity_nested_not_propagates_the_innermost_weight_unchanged) {
    const gfss_specificity total = specificity_of_the_one_selector_in(":not(:not(.a))");
    GLINTFX_CHECK_EQ(total.ids, static_cast<std::uint32_t>(0));
    GLINTFX_CHECK_EQ(total.classes, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(total.types, static_cast<std::uint32_t>(0));
}

// COMPARISON IS LEXICOGRAPHIC: ids, THEN classes, THEN types - 6 pairs,
// each one exercising a DIFFERENT column as the tie-breaker (or as the
// column that alone decides, swamping every column after it). Mutating
// the field declaration order in gfss_specificity (specificity.hpp)
// would silently change WHICH column the defaulted operator<=> compares
// first - these 6 pairs are what would catch that, not a single
// "bigger wins" smoke case.
GLINTFX_TEST(gltfx_gfss_specificity_comparison_is_lexicographic_ids_then_classes_then_types) {
    // Many classes never outweigh a single id (dossie: the classic
    // packed-integer defect D-W6-1 rejects by name).
    GLINTFX_CHECK((gfss_specificity{.ids = 0, .classes = 2, .types = 0} <
                   gfss_specificity{.ids = 1, .classes = 0, .types = 0}));
    // Many types never outweigh a single class.
    GLINTFX_CHECK((gfss_specificity{.ids = 0, .classes = 0, .types = 9} <
                   gfss_specificity{.ids = 0, .classes = 1, .types = 0}));
    // Tied ids: classes is the tie-breaker.
    GLINTFX_CHECK((gfss_specificity{.ids = 1, .classes = 0, .types = 0} <
                   gfss_specificity{.ids = 1, .classes = 1, .types = 0}));
    // Tied ids and classes: types is the last tie-breaker.
    GLINTFX_CHECK((gfss_specificity{.ids = 1, .classes = 5, .types = 0} <
                   gfss_specificity{.ids = 1, .classes = 5, .types = 1}));
    // Zero versus one, in the types column alone.
    GLINTFX_CHECK((gfss_specificity{.ids = 0, .classes = 0, .types = 0} <
                   gfss_specificity{.ids = 0, .classes = 0, .types = 1}));
    // Two ids each, differing ids alone.
    GLINTFX_CHECK((gfss_specificity{.ids = 2, .classes = 0, .types = 0} <
                   gfss_specificity{.ids = 3, .classes = 0, .types = 0}));
}

// SATURATION - the atom itself, at the boundary and one step BEYOND it
// (memoria da casa: um passo alem da borda, nao so a borda exata).
// "#a" repeated 4294967295 times does not fit in a source text this
// suite could hold, so this proves saturating_increment() DIRECTLY,
// never through a real parse - the same reason specificity.hpp's own
// header comment gives for exposing this as its own public atom.
GLINTFX_TEST(gltfx_gfss_specificity_saturating_increment_clamps_at_the_boundary_and_beyond) {
    constexpr std::uint32_t k_max = std::numeric_limits<std::uint32_t>::max();
    // An ordinary increment, nowhere near the boundary - proves this is
    // not a stub that always returns the max.
    GLINTFX_CHECK_EQ(saturating_increment(0), static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(saturating_increment(41), static_cast<std::uint32_t>(42));
    // AT the boundary: one more than (max - 1) reaches max exactly.
    GLINTFX_CHECK_EQ(saturating_increment(k_max - 1), k_max);
    // AT the boundary, from max itself: stays put, never wraps to 0.
    GLINTFX_CHECK_EQ(saturating_increment(k_max), k_max);
    // ONE STEP BEYOND the boundary: calling it again on the already-
    // saturated value still stays put.
    GLINTFX_CHECK_EQ(saturating_increment(saturating_increment(k_max)), k_max);
}
