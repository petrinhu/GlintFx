// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "fake/fake_counting_tree.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfss/selector_pseudo_vocabulary.hpp"
#include "gfui/compound_match.hpp"
#include "gfui/state_pseudo_class_table.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_compound_match_test.cpp - GFSS-MATCH-SIMPLE (TODO.md, GODS_
// LAWS.md L-20/L-40; /var/tmp/glintfx-plan/gfss-match-simple-plano.md
// SS6): TDD red/green witness for glintfx::gfui::detail::state_bit_
// for_pseudo_class() (fatia B) and glintfx::gfui::detail::match_
// compound() (fatias C/D/E) - see compound_match.hpp's own header
// comment for the algorithm and compound_match.cpp's own comments for
// the cost/order rationale each case below proves.
//
// FATIA B (state_pseudo_class_table_answers_the_five_names_...): the
// five-row table alone, no node, no parser.
//
// FATIA C (match_compound_species_matrix_case_state_and_deferral): id/
// tag/state/universal/case/deferral over a fake_arena tree, real gfss
// selector TEXT parsed through selector_parse.hpp - never an AST built
// by hand, so the case proves the parser-to-matcher chain, not just
// the matcher in isolation.
//
// FATIA D (match_compound_classes_bitmask_repetition_stop_and_
// fallback): the class-enumeration half in isolation - repetition
// tolerance, early stop, and the 64-requirement fallback boundary.
//
// FATIA E (match_compound_call_order_and_short_circuit_over_counting_
// tree): the D-MS-7 call order (id -> state -> tag -> classes) proved
// by COUNTING calls to a real gltfx_node_facts table, never by reading
// the source.

namespace {

// Parses `text` as exactly one selector with no combinator and returns
// its head compound. GLINTFX_CHECK-fails (unwinds the calling case) if
// `text` fails to parse, holds more than one comma-separated selector,
// or carries a combinator - none of the cases below need any of that.
//
// LIFETIME (plano's own risk #2): the returned compound's own std::
// string_view fields point INTO `text`. Every call site below passes
// either a string literal (static storage, always safe) or a local
// std::string held in a variable that outlives the match_compound()
// call using the result - never a temporary.
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

// --- fatia B: the state pseudo-class name table, no node involved ---

GLINTFX_TEST(state_pseudo_class_table_answers_the_five_names_case_insensitively_and_nothing_else) {
    using glintfx::gfui::gltfx_node_state;
    using glintfx::gfui::detail::k_state_pseudo_class_table;
    using glintfx::gfui::detail::state_bit_for_pseudo_class;

    std::printf("gfui_compound_match_test: k_state_pseudo_class_table has %zu rows "
                "(gltfx_node_state_count = %zu)\n",
                k_state_pseudo_class_table.size(), glintfx::gfui::gltfx_node_state_count);
    GLINTFX_CHECK_EQ(k_state_pseudo_class_table.size(), glintfx::gfui::gltfx_node_state_count);

    // Default member initializers, not a user-declared constructor -
    // the same cppcheck uninitMemberVarNoCtor fix named_colors.hpp's
    // own named_color_entry already applies, so aggregate init at
    // every row below still sets every field.
    struct sample {
        std::string_view name;
        gltfx_node_state expected = gltfx_node_state::none;
    };
    // Enumerated whole (GODS_LAWS.md L-17: small closed space, never a
    // sample): all five exact spellings, then all five uppercase.
    constexpr std::array<sample, 5> exact_samples{{
        {"hover", gltfx_node_state::hover},
        {"active", gltfx_node_state::active},
        {"focus", gltfx_node_state::focus},
        {"focus-visible", gltfx_node_state::focus_visible},
        {"checked", gltfx_node_state::checked},
    }};
    constexpr std::array<sample, 5> uppercase_samples{{
        {"HOVER", gltfx_node_state::hover},
        {"ACTIVE", gltfx_node_state::active},
        {"FOCUS", gltfx_node_state::focus},
        {"FOCUS-VISIBLE", gltfx_node_state::focus_visible},
        {"CHECKED", gltfx_node_state::checked},
    }};
    for (const sample &s : exact_samples) {
        const std::optional<gltfx_node_state> bit = state_bit_for_pseudo_class(s.name);
        GLINTFX_CHECK(bit.has_value());
        GLINTFX_CHECK(*bit == s.expected);
    }
    for (const sample &s : uppercase_samples) {
        const std::optional<gltfx_node_state> bit = state_bit_for_pseudo_class(s.name);
        GLINTFX_CHECK(bit.has_value());
        GLINTFX_CHECK(*bit == s.expected);
    }
    // A structural name and a near-miss both answer nullopt, never a
    // default-constructed bit that could be mistaken for `none`.
    GLINTFX_CHECK(!state_bit_for_pseudo_class("first-child").has_value());
    GLINTFX_CHECK(!state_bit_for_pseudo_class("hove").has_value());
    std::printf("gfui_compound_match_test: 5 exact + 5 uppercase + 2 unknown state-pseudo-class "
                "samples checked\n");
}

// --- fatia C: id/tag/state/universal/case/deferral, real parsed text ---

GLINTFX_TEST(match_compound_species_matrix_case_state_and_deferral) {
    using glintfx::gfui::detail::compound_match_verdict;
    using glintfx::gfui::detail::match_compound;
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t a_index = tree.add(entry{
        .tag = "a",
        .id = "one",
        .classes = {"alpha", "beta", "gamma"},
        .attributes = {{"disabled", ""}, {"data-x", "42"}},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    // Bitmask enum by design - combining two named bits is the
    // ordinary case, the SAME known clang-analyzer false positive
    // gfui_node_view_test.cpp's own scenario already suppresses.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: see comment above
    const auto hover_and_focus = static_cast<glintfx::gfui::gltfx_node_state>(
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::hover) |
        static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::focus));
    const std::size_t b_index = tree.add(entry{
        .tag = "b",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = hover_and_focus,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const glintfx::gfui::gltfx_node_view node_a = view(tree, a_index);
    const glintfx::gfui::gltfx_node_view node_b = view(tree, b_index);

    // Closed species x outcome matrix (GODS_LAWS.md L-17): id, tag and
    // state, each x {present-and-equal, present-and-different,
    // absent-from-the-selector}, enumerated whole, count printed.
    struct matrix_case {
        std::string_view text;
        const glintfx::gfui::gltfx_node_view *node = nullptr;
        compound_match_verdict expected = compound_match_verdict::rejected;
    };
    const std::array<matrix_case, 9> matrix{{
        {"#one", &node_a, compound_match_verdict::matched},
        {"#other", &node_a, compound_match_verdict::rejected},
        {"a", &node_a, compound_match_verdict::matched},
        {"a", &node_a, compound_match_verdict::matched},
        {"b", &node_a, compound_match_verdict::rejected},
        {"#one", &node_a, compound_match_verdict::matched},
        {":hover", &node_b, compound_match_verdict::matched},
        {":active", &node_b, compound_match_verdict::rejected},
        {"b", &node_b, compound_match_verdict::matched},
    }};
    std::printf("gfui_compound_match_test: %zu species-matrix cases (id/tag/state x present-equal/"
                "present-different/absent) checked\n",
                matrix.size());
    for (const matrix_case &c : matrix) {
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(c.text);
        GLINTFX_CHECK(match_compound(compound, *c.node) == c.expected);
    }

    // Universal: matches unconditionally, alone or glued to a type.
    GLINTFX_CHECK(match_compound(parse_one_compound("*"), node_a) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound("*a"), node_a) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound("a*"), node_a) ==
                  compound_match_verdict::matched);

    // Case policy (D-MS-4 tag/pseudo-class ASCII case-insensitive,
    // D-MS-5 class/id exact).
    GLINTFX_CHECK(match_compound(parse_one_compound("A"), node_a) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound("#One"), node_a) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound(".Alpha"), node_a) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound(":HOVER"), node_b) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound(":Focus-Visible"), node_b) ==
                  compound_match_verdict::rejected);

    // Two state pseudo-classes in one compound fold into one mask.
    GLINTFX_CHECK(match_compound(parse_one_compound(":hover:focus"), node_b) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound(":hover:active"), node_b) ==
                  compound_match_verdict::rejected);

    // id conflict never matches any node; a repeated EQUAL id is not a
    // conflict.
    GLINTFX_CHECK(match_compound(parse_one_compound("#a#b"), node_a) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound("#one#one"), node_a) ==
                  compound_match_verdict::matched);

    // Type conflict is the same shape as id conflict, one sibling
    // branch below in compound_match.cpp (note_type_selector's own
    // type_conflict): two DIFFERENT type selectors in one compound
    // never match any node, and a repeated EQUAL type (case-
    // insensitive, D-MS-4) is not a conflict. node_a's own type is
    // "a" (matrix above, "a" matches / "b" rejects).
    GLINTFX_CHECK(match_compound(parse_one_compound("a*b"), node_a) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound("div*span"), node_a) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound("a*a"), node_a) ==
                  compound_match_verdict::matched);

    // Deferral: every simple pseudo-class NOT among the five state
    // ones (enumerated from selector_pseudo_vocabulary.hpp's own
    // closed list, never a hand-picked subset).
    std::size_t deferred_simple_count = 0;
    for (const std::string_view &name : glintfx::style::detail::k_simple_pseudo_names) {
        if (glintfx::gfui::detail::state_bit_for_pseudo_class(name).has_value()) {
            continue;
        }
        ++deferred_simple_count;
        const std::string text = "a:" + std::string(name);
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(text);
        GLINTFX_CHECK(match_compound(compound, node_a) == compound_match_verdict::deferred);
    }
    GLINTFX_CHECK_EQ(deferred_simple_count, glintfx::style::detail::k_simple_pseudo_count -
                                                glintfx::gfui::gltfx_node_state_count);
    std::printf("gfui_compound_match_test: %zu non-state simple pseudo-classes deferred\n",
                deferred_simple_count);

    // Deferral: every functional pseudo-class, argument content
    // irrelevant here (GFSS-SEL-PARSE-CORE leaves it raw and
    // unanalyzed; this fatia never reads it either).
    std::size_t deferred_functional_count = 0;
    for (const std::string_view &name : glintfx::style::detail::k_functional_pseudo_names) {
        ++deferred_functional_count;
        const std::string text = "a:" + std::string(name) + "(1)";
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(text);
        GLINTFX_CHECK(match_compound(compound, node_a) == compound_match_verdict::deferred);
    }
    GLINTFX_CHECK_EQ(deferred_functional_count, glintfx::style::detail::k_functional_pseudo_count);
    std::printf("gfui_compound_match_test: %zu functional pseudo-classes deferred\n",
                deferred_functional_count);

    // Deferral: every pseudo-element (GFSS-SEL-PSEUDO-ELEMENT, 05/09/
    // 2026), enumerated whole from selector_pseudo_vocabulary.hpp's own
    // closed list (GODS_LAWS.md L-17/L-40), never a hand-picked
    // sample. A pseudo-element ("::before", "::after") asks for a box
    // that does not exist in the consumer's tree - a node-only pass
    // like this one cannot judge it, so it defers the same way a
    // pseudo_function does (compound_match.cpp's own note_pseudo_
    // class_selector() comment, and LAYOUT-PSEUDO-BOXES's own future
    // scope, never this fatia's).
    std::size_t deferred_pseudo_element_count = 0;
    for (const std::string_view &name : glintfx::style::detail::k_pseudo_element_names) {
        ++deferred_pseudo_element_count;
        const std::string text = "a::" + std::string(name);
        const glintfx::style::detail::gfss_compound_selector compound = parse_one_compound(text);
        GLINTFX_CHECK(match_compound(compound, node_a) == compound_match_verdict::deferred);
    }
    GLINTFX_CHECK_EQ(deferred_pseudo_element_count, glintfx::style::detail::k_pseudo_element_count);
    std::printf("gfui_compound_match_test: %zu pseudo-elements deferred\n",
                deferred_pseudo_element_count);

    // Rejection beats deferral: an id that never matches settles the
    // answer before :first-child (this fatia's own, deferred) is even
    // considered.
    GLINTFX_CHECK(match_compound(parse_one_compound("#nope:first-child"), node_a) ==
                  compound_match_verdict::rejected);
}

// --- fatia D: classes - repetition, early stop, 64-requirement fallback ---

namespace {

// Wraps fake_arena's own for_each_class() to count how many times the
// VISITOR (whoever it is) was invoked before it stopped - not how many
// times for_each_class() itself was called (fake_counting_tree.hpp
// already proves that count, fatia E below). This is the ONLY way to
// observe visit_class_for_requirements()'s own early stop from outside
// compound_match.cpp: the fixture's own for_each_class relays every
// call to the real visitor while counting it first.
struct class_visit_relay {
    glintfx::gfui::gltfx_node_class_visitor_fn original_visit = nullptr;
    void *original_context = nullptr;
    std::size_t *visit_count = nullptr;
};

[[nodiscard]] bool count_then_forward_visit(void *raw_relay, std::string_view class_name) noexcept {
    auto *relay = static_cast<class_visit_relay *>(raw_relay);
    ++(*relay->visit_count);
    return relay->original_visit(relay->original_context, class_name);
}

struct class_visit_counting_tree {
    const glintfx::test::fake_arena::arena *arena = nullptr;
    std::size_t *visit_count = nullptr;
};

void for_each_class_counting_visits(const void *tree_ptr, const void *node,
                                    glintfx::gfui::gltfx_node_class_visitor_fn visit,
                                    void *visitor_context) noexcept {
    const auto *counting_tree = static_cast<const class_visit_counting_tree *>(tree_ptr);
    class_visit_relay relay{.original_visit = visit,
                            .original_context = visitor_context,
                            .visit_count = counting_tree->visit_count};
    glintfx::test::fake_arena::for_each_class(counting_tree->arena, node, &count_then_forward_visit,
                                              &relay);
}

[[nodiscard]] const glintfx::gfui::gltfx_node_facts &visit_counting_facts() noexcept {
    static const glintfx::gfui::gltfx_node_facts k_facts = [] {
        glintfx::gfui::gltfx_node_facts f = glintfx::test::fake_arena::facts();
        f.for_each_class = &for_each_class_counting_visits;
        return f;
    }();
    return k_facts;
}

[[nodiscard]] glintfx::gfui::gltfx_node_view
make_visit_counting_view(const class_visit_counting_tree &counting_tree,
                         std::size_t index) noexcept {
    return glintfx::gfui::gltfx_node_view{
        .facts = &visit_counting_facts(),
        .tree = &counting_tree,
        .node = glintfx::test::fake_arena::detail::encode_index(index),
    };
}

} // namespace

GLINTFX_TEST(match_compound_classes_bitmask_repetition_stop_and_fallback) {
    using glintfx::gfui::detail::compound_match_verdict;
    using glintfx::gfui::detail::match_compound;
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t index = tree.add(entry{
        .tag = "a",
        .id = "",
        .classes = {"alpha", "beta", "gamma"},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const glintfx::gfui::gltfx_node_view node = view(tree, index);

    GLINTFX_CHECK(match_compound(parse_one_compound(".alpha.gamma"), node) ==
                  compound_match_verdict::matched);
    GLINTFX_CHECK(match_compound(parse_one_compound(".alpha.zzz"), node) ==
                  compound_match_verdict::rejected);
    GLINTFX_CHECK(match_compound(parse_one_compound(".Alpha"), node) ==
                  compound_match_verdict::rejected);

    // Repeated class in the consumer's OWN enumeration must not be
    // double-counted by a counter - the mutation a bit-per-requirement
    // scheme exists to defeat (compound_match.cpp's own comment).
    const std::size_t repeated_index = tree.add(entry{
        .tag = "a",
        .id = "",
        .classes = {"beta", "beta"},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const glintfx::gfui::gltfx_node_view repeated_node = view(tree, repeated_index);
    GLINTFX_CHECK(match_compound(parse_one_compound(".beta.gamma"), repeated_node) ==
                  compound_match_verdict::rejected);

    // Early stop: [alpha, beta, gamma] visited in author order; the
    // enumeration stops the MOMENT every requirement is satisfied.
    struct stop_case {
        std::string_view selector_text;
        std::size_t expected_visits = 0;
    };
    const std::array<stop_case, 3> stop_cases{{
        {".alpha", 1},
        {".gamma.alpha", 3},
        {".beta", 2},
    }};
    for (const stop_case &c : stop_cases) {
        std::size_t visit_count = 0;
        const class_visit_counting_tree counting_tree{.arena = &tree, .visit_count = &visit_count};
        const glintfx::gfui::gltfx_node_view counting_view =
            make_visit_counting_view(counting_tree, index);
        const glintfx::style::detail::gfss_compound_selector compound =
            parse_one_compound(c.selector_text);
        // Verdict discarded on purpose - this loop measures visit
        // COUNT (compound_match.hpp's own match_compound() is
        // [[nodiscard]]), so the read itself keeps GODS_LAWS.md L-40's
        // own "never a green without looking" contract honored.
        const compound_match_verdict verdict = match_compound(compound, counting_view);
        (void)verdict;
        GLINTFX_CHECK_EQ(visit_count, c.expected_visits);
    }
    std::printf("gfui_compound_match_test: %zu early-stop visit-count cases checked\n",
                stop_cases.size());

    // Fallback: a compound with MORE than 64 class requirements
    // (k_class_requirement_bitmask_capacity) - correct either way, one
    // step past the boundary (GODS_LAWS.md L-43: test a step past a
    // widened limit, not only the exact edge).
    std::vector<std::string> big_classes;
    big_classes.reserve(65);
    for (std::size_t i = 0; i < 65; ++i) {
        big_classes.push_back("c" + std::to_string(i));
    }
    const std::size_t big_index = tree.add(entry{
        .tag = "a",
        .id = "",
        .classes = big_classes,
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const glintfx::gfui::gltfx_node_view big_node = view(tree, big_index);

    std::string all_present_text = "a";
    for (std::size_t i = 0; i < 65; ++i) {
        all_present_text += ".c" + std::to_string(i);
    }
    const glintfx::style::detail::gfss_compound_selector all_present_compound =
        parse_one_compound(all_present_text);
    GLINTFX_CHECK(match_compound(all_present_compound, big_node) ==
                  compound_match_verdict::matched);

    std::string one_missing_text = "a";
    for (std::size_t i = 0; i < 64; ++i) {
        one_missing_text += ".c" + std::to_string(i);
    }
    one_missing_text += ".zzz_missing";
    const glintfx::style::detail::gfss_compound_selector one_missing_compound =
        parse_one_compound(one_missing_text);
    GLINTFX_CHECK(match_compound(one_missing_compound, big_node) ==
                  compound_match_verdict::rejected);
    std::printf("gfui_compound_match_test: 65-class fallback checked (all-present matched, "
                "65th-absent rejected)\n");
}

// --- fatia E: D-MS-7's own call order, proved by counting real calls ---

GLINTFX_TEST(match_compound_call_order_and_short_circuit_over_counting_tree) {
    using glintfx::gfui::detail::compound_match_verdict;
    using glintfx::gfui::detail::match_compound;
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;

    arena tree;
    const std::size_t index = tree.add(entry{
        .tag = "a",
        .id = "one",
        .classes = {"alpha"},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::hover,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });

    struct order_case {
        std::string_view selector_text;
        compound_match_verdict expected_verdict = compound_match_verdict::rejected;
        glintfx::test::fake_counting::counters expected_counts{};
    };
    const std::array<order_case, 6> cases{{
        {"div#nope.alpha:hover",
         compound_match_verdict::rejected,
         {.tag_name = 0,
          .id = 1,
          .for_each_class = 0,
          .attribute = 0,
          .state = 0,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
        {"a#one:active",
         compound_match_verdict::rejected,
         {.tag_name = 0,
          .id = 1,
          .for_each_class = 0,
          .attribute = 0,
          .state = 1,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
        {"b#one:hover",
         compound_match_verdict::rejected,
         {.tag_name = 1,
          .id = 1,
          .for_each_class = 0,
          .attribute = 0,
          .state = 1,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
        {"a#one.zzz",
         compound_match_verdict::rejected,
         {.tag_name = 1,
          .id = 1,
          .for_each_class = 1,
          .attribute = 0,
          .state = 0,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
        {"#a#b",
         compound_match_verdict::rejected,
         {.tag_name = 0,
          .id = 0,
          .for_each_class = 0,
          .attribute = 0,
          .state = 0,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
        {"a#one.alpha:first-child",
         compound_match_verdict::deferred,
         {.tag_name = 1,
          .id = 1,
          .for_each_class = 1,
          .attribute = 0,
          .state = 0,
          .parent = 0,
          .previous_sibling = 0,
          .next_sibling = 0,
          .child_count = 0,
          .first_child = 0}},
    }};

    for (const order_case &c : cases) {
        glintfx::test::fake_counting::counters counts{};
        const glintfx::test::fake_counting::tree counting_tree{.arena = &tree, .counts = &counts};
        const glintfx::gfui::gltfx_node_view node =
            glintfx::test::fake_counting::view(counting_tree, index);
        const glintfx::style::detail::gfss_compound_selector compound =
            parse_one_compound(c.selector_text);
        const compound_match_verdict verdict = match_compound(compound, node);
        GLINTFX_CHECK(verdict == c.expected_verdict);
        GLINTFX_CHECK_EQ(counts.tag_name, c.expected_counts.tag_name);
        GLINTFX_CHECK_EQ(counts.id, c.expected_counts.id);
        GLINTFX_CHECK_EQ(counts.for_each_class, c.expected_counts.for_each_class);
        GLINTFX_CHECK_EQ(counts.attribute, c.expected_counts.attribute);
        GLINTFX_CHECK_EQ(counts.state, c.expected_counts.state);
        GLINTFX_CHECK_EQ(counts.parent, c.expected_counts.parent);
        GLINTFX_CHECK_EQ(counts.previous_sibling, c.expected_counts.previous_sibling);
        GLINTFX_CHECK_EQ(counts.next_sibling, c.expected_counts.next_sibling);
        GLINTFX_CHECK_EQ(counts.child_count, c.expected_counts.child_count);
        GLINTFX_CHECK_EQ(counts.first_child, c.expected_counts.first_child);
    }
    std::printf("gfui_compound_match_test: %zu call-order/short-circuit cases checked over the "
                "counting fixture\n",
                cases.size());
}
