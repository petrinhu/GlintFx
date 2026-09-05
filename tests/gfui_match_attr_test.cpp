// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfui/attribute_match.hpp"
#include "gfui/compound_match.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_match_attr_test.cpp - GFSS-MATCH-ATTR (TODO.md, GODS_LAWS.md
// L-20/L-40): TDD red/green witness for glintfx::gfui::detail::
// attribute_selector_holds() (attribute_match.hpp/.cpp) in isolation,
// and for its wiring into compound_match.cpp's own match_compound(),
// which no longer defers every attribute selector once this fatia
// exists (see compound_match.hpp's own updated header comment).
//
// FATIA A (attribute_selector_holds_species_matrix): every one of the
// SEVEN outcomes this evaluator has to tell apart (bare presence, plus
// the six named operators - selector_ast.hpp's own header comment on
// why presence is not a fabricated seventh operator VALUE) built by
// hand as gfss_simple_selector values, against a fake_arena node -
// proves the evaluator without going through the parser at all.
//
// FATIA B (match_compound_evaluates_attribute_selectors_through_real_
// text): the SAME operators, this time reached through real gfss
// selector TEXT parsed by selector_parse.hpp and judged by match_
// compound() - proves the parser-to-matcher-to-attribute-evaluator
// chain, not just the evaluator in isolation.

namespace {

using glintfx::gfui::gltfx_node_view;
using glintfx::style::detail::gfss_attribute_operator;
using glintfx::style::detail::gfss_simple_selector;
using glintfx::style::detail::gfss_simple_selector_kind;

// Builds one bare "[name]" or "[name<op>value]" attribute simple
// selector by hand - no parser involved, so fatia A proves the
// evaluator itself, independent of GFSS-SEL-PARSE-ATTR's own grammar.
[[nodiscard]] gfss_simple_selector make_attribute_selector(std::string_view name,
                                                           gfss_attribute_operator op,
                                                           std::string_view value) noexcept {
    gfss_simple_selector simple;
    simple.kind = gfss_simple_selector_kind::attribute;
    simple.name = name;
    simple.attribute_operator = op;
    simple.has_attribute_value = true;
    simple.attribute_value = value;
    return simple;
}

[[nodiscard]] gfss_simple_selector make_presence_selector(std::string_view name) noexcept {
    gfss_simple_selector simple;
    simple.kind = gfss_simple_selector_kind::attribute;
    simple.name = name;
    simple.has_attribute_value = false;
    return simple;
}

// SAME helper shape gfui_compound_match_test.cpp's own parse_one_
// compound() already establishes - reused verbatim in intent, kept as
// its own copy per file (CONTRACT.md SS6.7's own "duplicacao real"
// test: a ~10-line arrange helper shared by two TEST FILES is not the
// kind of shared LOGIC the "three occurrences" rule is aimed at, and a
// dependency between two sibling test files would be a stranger
// coupling than the duplication itself).
[[nodiscard]] glintfx::style::detail::gfss_compound_selector
parse_one_compound(std::string_view text) {
    const glintfx::style::detail::selector_parse_result result =
        glintfx::style::detail::parse_selector_list(text);
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(result.value.selectors[0].rest.empty());
    return result.value.selectors[0].head;
}

} // namespace

// --- fatia A: attribute_selector_holds(), no parser involved ---

GLINTFX_TEST(attribute_selector_holds_species_matrix) {
    using glintfx::gfui::detail::attribute_selector_holds;
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t index = tree.add(entry{
        .tag = "input",
        .id = "",
        .classes = {},
        .attributes = {{"disabled", ""},
                       {"class", "alpha beta gamma"},
                       {"lang", "pt-BR"},
                       {"data-x", "hello world"}},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const gltfx_node_view node = view(tree, index);

    // Presence: [foo] holds for an attribute present with ANY value,
    // including an empty one - docs/node-view-and-matching.md's own
    // "present == true with an empty value means the attribute exists
    // and its value happens to be empty".
    GLINTFX_CHECK(attribute_selector_holds(make_presence_selector("disabled"), node));
    GLINTFX_CHECK(attribute_selector_holds(make_presence_selector("lang"), node));
    GLINTFX_CHECK(!attribute_selector_holds(make_presence_selector("missing"), node));

    // equals (=): exact, byte for byte - the VALUE is an author-chosen
    // identifier (attribute_match.hpp's own header comment).
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::equals, "pt-BR"), node));
    GLINTFX_CHECK(!attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::equals, "PT-BR"), node));
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("disabled", gfss_attribute_operator::equals, ""), node));

    // includes (~=): whitespace-separated TOKEN match, not substring -
    // "foo" must never match "foobar" or "barfoo".
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("class", gfss_attribute_operator::includes, "beta"), node));
    GLINTFX_CHECK(!attribute_selector_holds(
        make_attribute_selector("class", gfss_attribute_operator::includes, "alp"), node));
    {
        const std::size_t token_index = tree.add(entry{
            .tag = "span",
            .id = "",
            .classes = {},
            .attributes = {{"class", "foobar barfoo"}},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 0,
            .first_child = k_no_index,
        });
        const gltfx_node_view token_node = view(tree, token_index);
        GLINTFX_CHECK(!attribute_selector_holds(
            make_attribute_selector("class", gfss_attribute_operator::includes, "foo"),
            token_node));
    }

    // dash_match (|=): exact value, or value followed by a hyphen -
    // "pt" matches "pt" and "pt-BR", never "ptx".
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::dash_match, "pt"), node));
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::dash_match, "pt-BR"), node));
    {
        const std::size_t ptx_index = tree.add(entry{
            .tag = "span",
            .id = "",
            .classes = {},
            .attributes = {{"lang", "ptx"}},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 0,
            .first_child = k_no_index,
        });
        const gltfx_node_view ptx_node = view(tree, ptx_index);
        GLINTFX_CHECK(!attribute_selector_holds(
            make_attribute_selector("lang", gfss_attribute_operator::dash_match, "pt"), ptx_node));
    }

    // prefix/suffix/substring, empty needle never matches anything -
    // the case "quase todo mundo erra".
    struct empty_needle_case {
        gfss_attribute_operator op = gfss_attribute_operator::prefix_match;
    };
    const std::array<empty_needle_case, 3> empty_needle_cases{{
        {gfss_attribute_operator::prefix_match},
        {gfss_attribute_operator::suffix_match},
        {gfss_attribute_operator::substring_match},
    }};
    for (const empty_needle_case &c : empty_needle_cases) {
        GLINTFX_CHECK(!attribute_selector_holds(make_attribute_selector("disabled", c.op, ""),
                                                node)); // "disabled" attribute's own value is ""
        GLINTFX_CHECK(!attribute_selector_holds(make_attribute_selector("lang", c.op, ""), node));
    }

    // prefix_match (^=) / suffix_match ($=) / substring_match (*=),
    // the non-degenerate cases.
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::prefix_match, "pt"), node));
    GLINTFX_CHECK(!attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::prefix_match, "BR"), node));
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::suffix_match, "BR"), node));
    GLINTFX_CHECK(!attribute_selector_holds(
        make_attribute_selector("lang", gfss_attribute_operator::suffix_match, "pt"), node));
    GLINTFX_CHECK(attribute_selector_holds(
        make_attribute_selector("data-x", gfss_attribute_operator::substring_match, "lo wo"),
        node));
    GLINTFX_CHECK(!attribute_selector_holds(
        make_attribute_selector("data-x", gfss_attribute_operator::substring_match, "zzz"), node));

    std::printf("gfui_match_attr_test: attribute_selector_holds species matrix (presence + 6 "
                "operators + 3 empty-needle guards) checked\n");
}

// --- fatia B: real gfss selector text through match_compound() ---

GLINTFX_TEST(match_compound_evaluates_attribute_selectors_through_real_text) {
    using glintfx::gfui::detail::compound_match_verdict;
    using glintfx::gfui::detail::match_compound;
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t index = tree.add(entry{
        .tag = "input",
        .id = "",
        .classes = {},
        .attributes = {{"disabled", ""}, {"class", "alpha beta"}, {"lang", "pt-BR"}},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const gltfx_node_view node = view(tree, index);

    struct text_case {
        std::string_view text;
        compound_match_verdict expected = compound_match_verdict::rejected;
    };
    const std::array<text_case, 6> cases{{
        {"input[disabled]", compound_match_verdict::matched},
        {"input[missing]", compound_match_verdict::rejected},
        {"[class~=\"beta\"]", compound_match_verdict::matched},
        {"[class~=\"bet\"]", compound_match_verdict::rejected},
        {"[lang|=\"pt\"]", compound_match_verdict::matched},
        {"[lang^=\"\"]", compound_match_verdict::rejected},
    }};
    for (const text_case &c : cases) {
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(c.text);
        GLINTFX_CHECK(match_compound(compound, node) == c.expected);
    }
    std::printf(
        "gfui_match_attr_test: %zu match_compound() attribute-selector text cases checked\n",
        cases.size());

    // Rejection from a NOW-owned attribute requirement still beats
    // deferral from something this fatia does not own (:not() -
    // combinator work) - the same "rejeicao vence adiamento" rule
    // compound_match.hpp's own header comment already documents.
    const glintfx::style::detail::gfss_compound_selector rejecting_and_deferred =
        parse_one_compound("[missing]:not(.zzz)");
    GLINTFX_CHECK(match_compound(rejecting_and_deferred, node) == compound_match_verdict::rejected);

    // An attribute requirement that HOLDS, alongside something still
    // deferred, defers - the owned half no longer settles the answer
    // by itself.
    const glintfx::style::detail::gfss_compound_selector matching_and_deferred =
        parse_one_compound("[disabled]:not(.zzz)");
    GLINTFX_CHECK(match_compound(matching_and_deferred, node) == compound_match_verdict::deferred);
}
