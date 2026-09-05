// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfui/compound_match.hpp"
#include "gfui/structural_match.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_match_struct_test.cpp - GFSS-MATCH-STRUCT (TODO.md, GODS_LAWS.md
// L-20/L-40): TDD red/green witness for glintfx::gfui::detail's own
// eleven structural evaluators (structural_match.hpp/.cpp) in
// isolation, and for their wiring into compound_match.cpp's own match_
// compound(), which no longer defers a structural pseudo-class once
// this fatia exists (see compound_match.hpp's own updated header
// comment).
//
// FATIA A (structural_simple_holds_over_a_five_sibling_chain, plus a
// dedicated :empty case): the seven argument-less structural pseudo-
// classes, evaluated directly - no parser involved.
//
// FATIA B (structural_functional_holds_an_plus_b_over_the_same_chain):
// the four An+B functions, evaluated directly with hand-written raw
// argument text, over the SAME five-sibling fixture fatia A builds -
// same fixture, so a defect in the FIXTURE cannot hide by only ever
// showing up in one fatia's own assertions.
//
// FATIA C (match_compound_evaluates_structural_selectors_through_real_
// text): the same questions again, this time reached through real gfss
// selector TEXT parsed by selector_parse.hpp and judged by match_
// compound() - proves the parser-to-matcher-to-structural-evaluator
// chain, not just the evaluators in isolation.

namespace {

using glintfx::gfui::gltfx_node_view;
using glintfx::test::fake_arena::arena;
using glintfx::test::fake_arena::entry;
using glintfx::test::fake_arena::k_no_index;
using glintfx::test::fake_arena::view;

// Five siblings, tags a/b/a/b/a (1-indexed overall position 1..5),
// linked both ways - the ONE fixture every fatia below shares, so a
// mistake in the chain itself would show up as more than one failing
// case (this file's own header comment). Returns the five gltfx_node_
// view values in overall sibling order; `tree` must outlive every
// returned view.
[[nodiscard]] std::array<gltfx_node_view, 5> build_five_sibling_chain(arena &tree) noexcept {
    std::array<std::string_view, 5> tags{"a", "b", "a", "b", "a"};
    std::array<std::size_t, 5> indices{};
    for (std::size_t i = 0; i < 5; ++i) {
        indices[i] = tree.add(entry{
            .tag = std::string(tags[i]),
            .id = "",
            .classes = {},
            .attributes = {},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 0,
            .first_child = k_no_index,
        });
    }
    for (std::size_t i = 0; i < 5; ++i) {
        if (i > 0) {
            tree.entries[indices[i]].previous_sibling = indices[i - 1];
        }
        if (i + 1 < 5) {
            tree.entries[indices[i]].next_sibling = indices[i + 1];
        }
    }
    std::array<gltfx_node_view, 5> views{};
    for (std::size_t i = 0; i < 5; ++i) {
        views[i] = view(tree, indices[i]);
    }
    return views;
}

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

// --- fatia A: the seven simple structural pseudo-classes ---

GLINTFX_TEST(structural_simple_holds_over_a_five_sibling_chain) {
    using glintfx::gfui::detail::structural_simple_holds;
    using glintfx::gfui::detail::structural_simple_kind;

    arena tree;
    const std::array<gltfx_node_view, 5> chain = build_five_sibling_chain(tree);

    // first-child / last-child / only-child: position matters, tag
    // does not.
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::first_child, chain[0]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::first_child, chain[1]));
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::last_child, chain[4]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::last_child, chain[3]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::only_child, chain[0]));

    // A lone child (no siblings at all) is first, last AND only.
    {
        arena lone_tree;
        const std::size_t lone_index = lone_tree.add(entry{
            .tag = "a",
            .id = "",
            .classes = {},
            .attributes = {},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 0,
            .first_child = k_no_index,
        });
        const gltfx_node_view lone_node = view(lone_tree, lone_index);
        GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::first_child, lone_node));
        GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::last_child, lone_node));
        GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::only_child, lone_node));
    }

    // first-of-type / last-of-type / only-of-type: chain[0] is the
    // FIRST "a" (positions 1,3,5 are "a"); chain[2] is neither first
    // nor last of its own type; chain[4] is the LAST "a". chain[1] (a
    // lone "b" among two) is first-of-type, last-of-type AND only-of-
    // type for "b", because no OTHER "b" precedes or follows it at
    // THAT position in this fixture... wait: chain[1] and chain[3] are
    // BOTH "b" - chain[1] is first-of-type but not last-of-type;
    // chain[3] is last-of-type but not first-of-type; neither is
    // only-of-type.
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::first_of_type, chain[0]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::first_of_type, chain[2]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::last_of_type, chain[2]));
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::last_of_type, chain[4]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::only_of_type, chain[0]));
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::first_of_type, chain[1]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::last_of_type, chain[1]));
    GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::first_of_type, chain[3]));
    GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::last_of_type, chain[3]));

    // :empty counts CONTENT (child_count, fact 8's own count half),
    // never navigable-element-only child_first() (docs/node-view-and-
    // matching.md's own "child_count sees content, navigating children
    // sees only elements") - a node with pure text content has
    // child_count > 0 but first_child == null, and must NOT read as
    // :empty.
    {
        arena content_tree;
        const std::size_t truly_empty_index = content_tree.add(entry{
            .tag = "p",
            .id = "",
            .classes = {},
            .attributes = {},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 0,
            .first_child = k_no_index,
        });
        const std::size_t text_only_index = content_tree.add(entry{
            .tag = "p",
            .id = "",
            .classes = {},
            .attributes = {},
            .state = glintfx::gfui::gltfx_node_state::none,
            .parent = k_no_index,
            .previous_sibling = k_no_index,
            .next_sibling = k_no_index,
            .child_count = 1,          // one text node - CONTENT, not empty
            .first_child = k_no_index, // no navigable ELEMENT child
        });
        GLINTFX_CHECK(structural_simple_holds(structural_simple_kind::empty,
                                              view(content_tree, truly_empty_index)));
        GLINTFX_CHECK(!structural_simple_holds(structural_simple_kind::empty,
                                               view(content_tree, text_only_index)));
    }

    std::printf("gfui_match_struct_test: 7 simple structural pseudo-classes checked over the "
                "five-sibling chain, the lone-child case and the :empty content-vs-navigation "
                "distinction\n");
}

// --- fatia B: the four An+B functions ---

GLINTFX_TEST(structural_functional_holds_an_plus_b_over_the_same_chain) {
    using glintfx::gfui::detail::structural_functional_holds;
    using glintfx::gfui::detail::structural_functional_kind;

    arena tree;
    const std::array<gltfx_node_view, 5> chain = build_five_sibling_chain(tree);

    // nth-child: overall 1-based position, tag irrelevant. "2n+1" is
    // odd positions (1, 3, 5); "2n" is even positions (2, 4). Position
    // counting starts at ONE, never zero - the classic off-by-one.
    struct nth_child_case {
        std::size_t chain_index = 0;
        bool odd_expected = false;
    };
    const std::array<nth_child_case, 5> odd_cases{{
        {0, true},
        {1, false},
        {2, true},
        {3, false},
        {4, true},
    }};
    for (const nth_child_case &c : odd_cases) {
        GLINTFX_CHECK_EQ(structural_functional_holds(structural_functional_kind::nth_child, "2n+1",
                                                     chain[c.chain_index]),
                         c.odd_expected);
    }
    // A bare integer is a=0, b=N: "3" means EXACTLY the third element.
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_child, "3", chain[2]));
    GLINTFX_CHECK(
        !structural_functional_holds(structural_functional_kind::nth_child, "3", chain[1]));
    GLINTFX_CHECK(
        !structural_functional_holds(structural_functional_kind::nth_child, "3", chain[3]));

    // nth-last-child: same rule, counting from the LAST sibling.
    // chain[4] (last) is "1"; chain[0] (first of 5) is "5".
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_last_child, "1", chain[4]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_last_child, "5", chain[0]));
    GLINTFX_CHECK(
        !structural_functional_holds(structural_functional_kind::nth_last_child, "1", chain[3]));

    // nth-of-type: position among SAME-TAG siblings only. chain[2] is
    // "a" at overall position 3, but it is the SECOND "a" (chain[0] is
    // the first, chain[4] the third).
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_of_type, "1", chain[0]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_of_type, "2", chain[2]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_of_type, "3", chain[4]));
    GLINTFX_CHECK(
        !structural_functional_holds(structural_functional_kind::nth_of_type, "2", chain[4]));
    // chain[1] and chain[3] are the two "b" siblings - "of-type"
    // position among THEM only, ignoring the three "a" nodes between
    // and around them.
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_of_type, "1", chain[1]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_of_type, "2", chain[3]));

    // nth-last-of-type: symmetric, counting from the last SAME-TAG
    // sibling. chain[2] ("a") has one same-tag sibling after it
    // (chain[4]), so its own last-of-type position is 2.
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_last_of_type, "2", chain[2]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_last_of_type, "1", chain[4]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_last_of_type, "3", chain[0]));

    // even/odd keywords (An+B microsyntax's own named forms,
    // anb_parse.hpp's own header comment) - "odd" is the same set as
    // "2n+1" above, proved once more through the keyword spelling
    // rather than the numeric one.
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_child, "odd", chain[0]));
    GLINTFX_CHECK(
        !structural_functional_holds(structural_functional_kind::nth_child, "odd", chain[1]));
    GLINTFX_CHECK(
        structural_functional_holds(structural_functional_kind::nth_child, "even", chain[1]));

    // A malformed An+B argument never matches anything - the
    // graceful-degradation stance structural_match.hpp's own header
    // comment documents for hostile or malformed leaf content.
    GLINTFX_CHECK(!structural_functional_holds(structural_functional_kind::nth_child, "not-an-anb",
                                               chain[0]));

    std::printf("gfui_match_struct_test: 4 An+B structural functions checked over the same "
                "five-sibling chain (numeric, bare-integer, even/odd and malformed-argument "
                "cases)\n");
}

// --- fatia C: real gfss selector text through match_compound() ---

GLINTFX_TEST(match_compound_evaluates_structural_selectors_through_real_text) {
    using glintfx::gfui::detail::compound_match_verdict;
    using glintfx::gfui::detail::match_compound;

    arena tree;
    const std::array<gltfx_node_view, 5> chain = build_five_sibling_chain(tree);

    struct text_case {
        std::string_view text;
        std::size_t chain_index = 0;
        compound_match_verdict expected = compound_match_verdict::rejected;
    };
    const std::array<text_case, 8> cases{{
        {":first-child", 0, compound_match_verdict::matched},
        {":first-child", 1, compound_match_verdict::rejected},
        {":last-child", 4, compound_match_verdict::matched},
        {"a:only-of-type", 0, compound_match_verdict::rejected},
        {"b:first-of-type", 1, compound_match_verdict::matched},
        {":nth-child(2n+1)", 2, compound_match_verdict::matched},
        {":nth-child(2n+1)", 1, compound_match_verdict::rejected},
        {"a:nth-of-type(3)", 4, compound_match_verdict::matched},
    }};
    for (const text_case &c : cases) {
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(c.text);
        GLINTFX_CHECK(match_compound(compound, chain[c.chain_index]) == c.expected);
    }
    std::printf(
        "gfui_match_struct_test: %zu match_compound() structural-selector text cases checked\n",
        cases.size());

    // Rejection from a NOW-owned structural requirement still beats
    // deferral from something this fatia does not own (:placeholder-
    // shown, unresolved by design) - the same "rejeicao vence
    // adiamento" rule compound_match.hpp's own header comment already
    // documents.
    const glintfx::style::detail::gfss_compound_selector rejecting_and_deferred =
        parse_one_compound(":last-child:placeholder-shown");
    GLINTFX_CHECK(match_compound(rejecting_and_deferred, chain[0]) ==
                  compound_match_verdict::rejected);

    // A structural requirement that HOLDS, alongside something still
    // deferred, defers.
    const glintfx::style::detail::gfss_compound_selector matching_and_deferred =
        parse_one_compound(":first-child:placeholder-shown");
    GLINTFX_CHECK(match_compound(matching_and_deferred, chain[0]) ==
                  compound_match_verdict::deferred);
}
