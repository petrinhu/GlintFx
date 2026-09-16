// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "fake/fake_counting_tree.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfui/complex_match.hpp"
#include "gfui/match_verdict.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_complex_match_test.cpp - GFSS-MATCH-COMBINE (TODO.md, GODS_
// LAWS.md L-20/L-40; docs/plano-w6-folha-de-estilo.md, fatia S-2):
// TDD red/green witness for glintfx::gfui::detail::match_complex() -
// see complex_match.hpp's own header comment for the algorithm this
// file proves, and the plan's own acceptance table (S-2 row) for the
// exact shape of the five cases below.
//
// FIVE CASES, EACH ITS OWN GLINTFX_TEST (GODS_LAWS.md L-17's "um
// assunto por caso"):
//
// 1. combinator_matrix: the closed 4x3 matrix (four combinators, each
//    x {casa, nao casa, precisa retroceder/nao se aplica}), twelve
//    cells, none omitted, count printed.
// 2. not_recursive: one argument, a list, ten levels of nesting (the
//    SAME shared budget D-W6-8 gives :not()), and the dossier's own
//    "body :not(table) a" case (SS5.5).
// 3. scope_two_contexts: with an explicit scope (only the scope node
//    itself), without one (the root, D-W6-5's own second context), and
//    "a + :scope" (scope as the SUBJECT of a working combinator chain
//    - never confused with ":scope + a", S-5's own dead-selector case,
//    D-W6-6, which this fatia does not implement).
// 4. deferral_preserved: `:placeholder-shown` and a pseudo-element
//    still answer deferred through match_complex(), never a guessed
//    matched.
// 5. right_to_left_by_counting: F10's own proof - a rejected subject
//    costs ZERO calls into the ancestor's own facts, via fake_
//    counting_tree.hpp, never by reading the source.

namespace {

using glintfx::gfui::gltfx_node_view;
using glintfx::gfui::detail::match_complex;
using glintfx::gfui::detail::match_verdict;
using glintfx::style::detail::gfss_complex_selector;

// Parses `text` as exactly one selector (no comma) and returns its
// complex selector whole - head AND rest, unlike gfui_compound_match_
// test.cpp's own parse_one_compound(), which throws `rest` away on
// purpose because that file never needs a combinator. This file is
// ABOUT combinators, so it keeps the whole shape.
//
// LIFETIME (same discipline parse_one_compound() already documents in
// the sibling file): every std::string_view inside the returned value
// points INTO `text` - every call site below passes a string literal
// or a local std::string that outlives the match_complex() call using
// the result.
[[nodiscard]] gfss_complex_selector parse_one_complex(std::string_view text) {
    const glintfx::style::detail::selector_parse_result result =
        glintfx::style::detail::parse_selector_list(text);
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    return result.value.selectors[0];
}

// The "no explicit scope" view every case that does not care about
// :scope anchoring passes - `node == nullptr` is node_query.hpp's own
// is_null() convention, D-W6-5's own "sem escopo" context.
constexpr gltfx_node_view k_no_scope{};

} // namespace

// --- case 1: the closed 4 x 3 combinator matrix, twelve cells ---

GLINTFX_TEST(match_complex_combinator_matrix_direction_and_backtrack) {
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    // `>` (child): single-step, D-W6-2's own "sem retrocesso" - the
    // third cell (no second candidate to retry) is what stands in for
    // "nao se aplica" here, counted, never omitted.
    arena child_tree;
    const std::size_t child_parent_idx =
        child_tree.add(entry{.tag = "div",
                             .id = "",
                             .classes = {"mid"},
                             .attributes = {},
                             .state = glintfx::gfui::gltfx_node_state::none,
                             .parent = k_no_index,
                             .previous_sibling = k_no_index,
                             .next_sibling = k_no_index,
                             .child_count = 0,
                             .first_child = k_no_index});
    const std::size_t child_subject_idx =
        child_tree.add(entry{.tag = "a",
                             .id = "",
                             .classes = {"leaf"},
                             .attributes = {},
                             .state = glintfx::gfui::gltfx_node_state::none,
                             .parent = child_parent_idx,
                             .previous_sibling = k_no_index,
                             .next_sibling = k_no_index,
                             .child_count = 0,
                             .first_child = k_no_index});
    // Matches ".leaf" on its OWN compound, same as child_subject above,
    // but has no parent at all - proves "no second candidate" REJECTS
    // even when the subject's own half would otherwise hold, never
    // conflating "subject itself does not match" with "no candidate to
    // try" (a node that merely lacks class "leaf" would reject for the
    // wrong reason if used here instead).
    const std::size_t child_orphan_idx =
        child_tree.add(entry{.tag = "a",
                             .id = "",
                             .classes = {"leaf"},
                             .attributes = {},
                             .state = glintfx::gfui::gltfx_node_state::none,
                             .parent = k_no_index,
                             .previous_sibling = k_no_index,
                             .next_sibling = k_no_index,
                             .child_count = 0,
                             .first_child = k_no_index});
    const gltfx_node_view child_subject = view(child_tree, child_subject_idx);
    const gltfx_node_view child_orphan = view(child_tree, child_orphan_idx);

    // `+` (next_sibling): single-step, same "no second candidate" shape
    // as `>` above, mirrored on the sibling axis instead of the
    // ancestor one.
    arena next_tree;
    const std::size_t next_s1_idx =
        next_tree.add(entry{.tag = "a",
                            .id = "",
                            .classes = {"x"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = k_no_index,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const std::size_t next_s2_idx =
        next_tree.add(entry{.tag = "b",
                            .id = "",
                            .classes = {"y"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = k_no_index,
                            .previous_sibling = next_s1_idx,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    next_tree.entries[next_s1_idx].next_sibling = next_s2_idx;
    // Matches ".y" on its own compound, same as next_subject above, but
    // has no previous sibling at all - the SAME "reject for the right
    // reason" discipline child_orphan above documents.
    const std::size_t next_orphan_idx =
        next_tree.add(entry{.tag = "b",
                            .id = "",
                            .classes = {"y"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = k_no_index,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const gltfx_node_view next_subject = view(next_tree, next_s2_idx);
    const gltfx_node_view next_orphan = view(next_tree, next_orphan_idx);

    // ` ` (descendant): iterates every ancestor, backtracking past a
    // dead end - the dossier's own ".a > .b .c" over ".a > .b > .b >
    // .c" (SS5.1), built exactly (A -> B1 -> B2 -> C, C is the
    // subject): the NEAREST ".b" (B2) leads to a dead end (its own
    // parent B1 is ".b", not ".a"), so the walk must retreat to the
    // FARTHER ".b" (B1), whose own parent A really is ".a".
    arena desc_tree;
    const std::size_t desc_a_idx =
        desc_tree.add(entry{.tag = "div",
                            .id = "",
                            .classes = {"a"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = k_no_index,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const std::size_t desc_b1_idx =
        desc_tree.add(entry{.tag = "div",
                            .id = "",
                            .classes = {"b"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = desc_a_idx,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const std::size_t desc_b2_idx =
        desc_tree.add(entry{.tag = "div",
                            .id = "",
                            .classes = {"b"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = desc_b1_idx,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const std::size_t desc_c_idx =
        desc_tree.add(entry{.tag = "div",
                            .id = "",
                            .classes = {"c"},
                            .attributes = {},
                            .state = glintfx::gfui::gltfx_node_state::none,
                            .parent = desc_b2_idx,
                            .previous_sibling = k_no_index,
                            .next_sibling = k_no_index,
                            .child_count = 0,
                            .first_child = k_no_index});
    const gltfx_node_view desc_backtrack_subject = view(desc_tree, desc_c_idx);

    arena desc_simple_tree;
    const std::size_t desc_simple_a_idx =
        desc_simple_tree.add(entry{.tag = "div",
                                   .id = "",
                                   .classes = {"a"},
                                   .attributes = {},
                                   .state = glintfx::gfui::gltfx_node_state::none,
                                   .parent = k_no_index,
                                   .previous_sibling = k_no_index,
                                   .next_sibling = k_no_index,
                                   .child_count = 0,
                                   .first_child = k_no_index});
    const std::size_t desc_simple_c_idx =
        desc_simple_tree.add(entry{.tag = "div",
                                   .id = "",
                                   .classes = {"c"},
                                   .attributes = {},
                                   .state = glintfx::gfui::gltfx_node_state::none,
                                   .parent = desc_simple_a_idx,
                                   .previous_sibling = k_no_index,
                                   .next_sibling = k_no_index,
                                   .child_count = 0,
                                   .first_child = k_no_index});
    const gltfx_node_view desc_simple_subject = view(desc_simple_tree, desc_simple_c_idx);

    // `~` (subsequent_sibling): same broad backtracking shape as
    // descendant, mirrored on the sibling axis - ".x + .y ~ .z", built
    // so the NEAREST ".y" candidate is a trap (its own immediately-
    // preceding sibling is not ".x", and `+` never retries), forcing
    // the walk to retreat to the FARTHER ".y", whose own immediately-
    // preceding sibling really is ".x".
    arena subseq_tree;
    const std::size_t subseq_x_idx =
        subseq_tree.add(entry{.tag = "a",
                              .id = "",
                              .classes = {"x"},
                              .attributes = {},
                              .state = glintfx::gfui::gltfx_node_state::none,
                              .parent = k_no_index,
                              .previous_sibling = k_no_index,
                              .next_sibling = k_no_index,
                              .child_count = 0,
                              .first_child = k_no_index});
    const std::size_t subseq_real_y_idx =
        subseq_tree.add(entry{.tag = "b",
                              .id = "",
                              .classes = {"y"},
                              .attributes = {},
                              .state = glintfx::gfui::gltfx_node_state::none,
                              .parent = k_no_index,
                              .previous_sibling = subseq_x_idx,
                              .next_sibling = k_no_index,
                              .child_count = 0,
                              .first_child = k_no_index});
    subseq_tree.entries[subseq_x_idx].next_sibling = subseq_real_y_idx;
    const std::size_t subseq_other_idx =
        subseq_tree.add(entry{.tag = "c",
                              .id = "",
                              .classes = {"other"},
                              .attributes = {},
                              .state = glintfx::gfui::gltfx_node_state::none,
                              .parent = k_no_index,
                              .previous_sibling = subseq_real_y_idx,
                              .next_sibling = k_no_index,
                              .child_count = 0,
                              .first_child = k_no_index});
    subseq_tree.entries[subseq_real_y_idx].next_sibling = subseq_other_idx;
    const std::size_t subseq_trap_y_idx =
        subseq_tree.add(entry{.tag = "b",
                              .id = "",
                              .classes = {"y"},
                              .attributes = {},
                              .state = glintfx::gfui::gltfx_node_state::none,
                              .parent = k_no_index,
                              .previous_sibling = subseq_other_idx,
                              .next_sibling = k_no_index,
                              .child_count = 0,
                              .first_child = k_no_index});
    subseq_tree.entries[subseq_other_idx].next_sibling = subseq_trap_y_idx;
    const std::size_t subseq_subject_idx =
        subseq_tree.add(entry{.tag = "d",
                              .id = "",
                              .classes = {"z"},
                              .attributes = {},
                              .state = glintfx::gfui::gltfx_node_state::none,
                              .parent = k_no_index,
                              .previous_sibling = subseq_trap_y_idx,
                              .next_sibling = k_no_index,
                              .child_count = 0,
                              .first_child = k_no_index});
    subseq_tree.entries[subseq_trap_y_idx].next_sibling = subseq_subject_idx;
    const gltfx_node_view subseq_backtrack_subject = view(subseq_tree, subseq_subject_idx);

    arena subseq_simple_tree;
    const std::size_t subseq_simple_x_idx =
        subseq_simple_tree.add(entry{.tag = "a",
                                     .id = "",
                                     .classes = {"x"},
                                     .attributes = {},
                                     .state = glintfx::gfui::gltfx_node_state::none,
                                     .parent = k_no_index,
                                     .previous_sibling = k_no_index,
                                     .next_sibling = k_no_index,
                                     .child_count = 0,
                                     .first_child = k_no_index});
    const std::size_t subseq_simple_y_idx =
        subseq_simple_tree.add(entry{.tag = "b",
                                     .id = "",
                                     .classes = {"y"},
                                     .attributes = {},
                                     .state = glintfx::gfui::gltfx_node_state::none,
                                     .parent = k_no_index,
                                     .previous_sibling = subseq_simple_x_idx,
                                     .next_sibling = k_no_index,
                                     .child_count = 0,
                                     .first_child = k_no_index});
    const gltfx_node_view subseq_simple_subject = view(subseq_simple_tree, subseq_simple_y_idx);

    struct matrix_case {
        std::string_view label;
        std::string_view text;
        const gltfx_node_view *subject = nullptr;
        match_verdict expected = match_verdict::rejected;
    };
    const std::array<matrix_case, 12> matrix{{
        // child (`>`)
        {"child casa", ".mid > .leaf", &child_subject, match_verdict::matched},
        {"child nao casa", ".other > .leaf", &child_subject, match_verdict::rejected},
        {"child nao se aplica (sem 2o candidato)", ".mid > .leaf", &child_orphan,
         match_verdict::rejected},
        // next_sibling (`+`)
        {"next_sibling casa", ".x + .y", &next_subject, match_verdict::matched},
        {"next_sibling nao casa", ".z + .y", &next_subject, match_verdict::rejected},
        {"next_sibling nao se aplica (sem 2o candidato)", ".x + .y", &next_orphan,
         match_verdict::rejected},
        // descendant (` `)
        {"descendant casa", ".a .c", &desc_simple_subject, match_verdict::matched},
        {"descendant nao casa", ".z .c", &desc_simple_subject, match_verdict::rejected},
        {"descendant precisa retroceder", ".a > .b .c", &desc_backtrack_subject,
         match_verdict::matched},
        // subsequent_sibling (`~`)
        {"subsequent_sibling casa", ".x ~ .y", &subseq_simple_subject, match_verdict::matched},
        {"subsequent_sibling nao casa", ".z ~ .y", &subseq_simple_subject, match_verdict::rejected},
        {"subsequent_sibling precisa retroceder", ".x + .y ~ .z", &subseq_backtrack_subject,
         match_verdict::matched},
    }};

    std::size_t checked = 0;
    for (const matrix_case &row : matrix) {
        const gfss_complex_selector selector = parse_one_complex(row.text);
        const match_verdict actual = match_complex(selector, *row.subject, k_no_scope);
        GLINTFX_CHECK(actual == row.expected);
        ++checked;
    }
    GLINTFX_CHECK_EQ(checked, matrix.size());
    std::printf("gfui_complex_match_test: combinator matrix, %zu/%zu cells checked\n", checked,
                matrix.size());
    std::printf("SCANCOUNT gfui_complex_match_test.combinator_matrix_cells=%zu\n", checked);
}

// --- case 2: `:not()`, recursive - one argument, a list, ten levels ---

GLINTFX_TEST(match_complex_not_is_recursive_and_shares_the_nesting_budget) {
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;

    arena tree;
    const std::size_t x_idx = tree.add(entry{.tag = "a",
                                             .id = "",
                                             .classes = {"x"},
                                             .attributes = {},
                                             .state = glintfx::gfui::gltfx_node_state::none,
                                             .parent = k_no_index,
                                             .previous_sibling = k_no_index,
                                             .next_sibling = k_no_index,
                                             .child_count = 0,
                                             .first_child = k_no_index});
    const std::size_t neither_idx = tree.add(entry{.tag = "a",
                                                   .id = "",
                                                   .classes = {"neither"},
                                                   .attributes = {},
                                                   .state = glintfx::gfui::gltfx_node_state::none,
                                                   .parent = k_no_index,
                                                   .previous_sibling = k_no_index,
                                                   .next_sibling = k_no_index,
                                                   .child_count = 0,
                                                   .first_child = k_no_index});
    const gltfx_node_view node_x = glintfx::test::fake_arena::view(tree, x_idx);
    const gltfx_node_view node_neither = glintfx::test::fake_arena::view(tree, neither_idx);

    // One argument: `:not(.x)` holds iff the node does NOT have class x.
    {
        const gfss_complex_selector selector = parse_one_complex(":not(.x)");
        GLINTFX_CHECK(match_complex(selector, node_x, k_no_scope) == match_verdict::rejected);
        GLINTFX_CHECK(match_complex(selector, node_neither, k_no_scope) == match_verdict::matched);
    }

    // A list: `:not(.x, .y)` holds iff NEITHER matches (S-1's own rule,
    // confirmed here at the combinator-aware level too).
    {
        arena list_tree;
        const std::size_t has_x_idx =
            list_tree.add(entry{.tag = "a",
                                .id = "",
                                .classes = {"x"},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
        const std::size_t has_y_idx =
            list_tree.add(entry{.tag = "a",
                                .id = "",
                                .classes = {"y"},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
        const std::size_t has_neither_idx =
            list_tree.add(entry{.tag = "a",
                                .id = "",
                                .classes = {"z"},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
        const gfss_complex_selector selector = parse_one_complex(":not(.x, .y)");
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(list_tree, has_x_idx),
                                    k_no_scope) == match_verdict::rejected);
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(list_tree, has_y_idx),
                                    k_no_scope) == match_verdict::rejected);
        GLINTFX_CHECK(match_complex(selector,
                                    glintfx::test::fake_arena::view(list_tree, has_neither_idx),
                                    k_no_scope) == match_verdict::matched);
    }

    // Ten levels of nesting, D-W6-8's own shared budget
    // (k_max_nested_selector_list_depth, still spelled k_max_not_
    // nesting_depth in the tree today per S-4's own future rename) -
    // double negation cancels: an EVEN depth is equivalent to the bare
    // base selector, proved here by running the base fact (has class
    // "zzz") through ten wraps and checking it comes back UNCHANGED.
    {
        std::string nested_text = ".zzz";
        for (int depth = 0; depth < 10; ++depth) {
            nested_text.insert(0, ":not(");
            nested_text += ")";
        }
        const gfss_complex_selector selector = parse_one_complex(nested_text);
        arena nest_tree;
        const std::size_t has_zzz_idx =
            nest_tree.add(entry{.tag = "a",
                                .id = "",
                                .classes = {"zzz"},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
        const std::size_t lacks_zzz_idx =
            nest_tree.add(entry{.tag = "a",
                                .id = "",
                                .classes = {"other"},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
        GLINTFX_CHECK(match_complex(selector,
                                    glintfx::test::fake_arena::view(nest_tree, has_zzz_idx),
                                    k_no_scope) == match_verdict::matched);
        GLINTFX_CHECK(match_complex(selector,
                                    glintfx::test::fake_arena::view(nest_tree, lacks_zzz_idx),
                                    k_no_scope) == match_verdict::rejected);
    }

    // Dossier SS5.5: "body :not(table) a" matches an "a" descended
    // from a "table" - a naive implementation that rejected the whole
    // chain the instant ANY table ancestor existed anywhere would get
    // this wrong; the correct reading only needs ONE ancestor (any
    // position) that is not itself a table, which the nearest ancestor
    // already satisfies here.
    {
        arena doc_tree;
        const std::size_t body_idx =
            doc_tree.add(entry{.tag = "body",
                               .id = "",
                               .classes = {},
                               .attributes = {},
                               .state = glintfx::gfui::gltfx_node_state::none,
                               .parent = k_no_index,
                               .previous_sibling = k_no_index,
                               .next_sibling = k_no_index,
                               .child_count = 0,
                               .first_child = k_no_index});
        const std::size_t table_idx =
            doc_tree.add(entry{.tag = "table",
                               .id = "",
                               .classes = {},
                               .attributes = {},
                               .state = glintfx::gfui::gltfx_node_state::none,
                               .parent = body_idx,
                               .previous_sibling = k_no_index,
                               .next_sibling = k_no_index,
                               .child_count = 0,
                               .first_child = k_no_index});
        const std::size_t tr_idx =
            doc_tree.add(entry{.tag = "tr",
                               .id = "",
                               .classes = {},
                               .attributes = {},
                               .state = glintfx::gfui::gltfx_node_state::none,
                               .parent = table_idx,
                               .previous_sibling = k_no_index,
                               .next_sibling = k_no_index,
                               .child_count = 0,
                               .first_child = k_no_index});
        const std::size_t td_idx =
            doc_tree.add(entry{.tag = "td",
                               .id = "",
                               .classes = {},
                               .attributes = {},
                               .state = glintfx::gfui::gltfx_node_state::none,
                               .parent = tr_idx,
                               .previous_sibling = k_no_index,
                               .next_sibling = k_no_index,
                               .child_count = 0,
                               .first_child = k_no_index});
        const std::size_t a_idx = doc_tree.add(entry{.tag = "a",
                                                     .id = "",
                                                     .classes = {},
                                                     .attributes = {},
                                                     .state = glintfx::gfui::gltfx_node_state::none,
                                                     .parent = td_idx,
                                                     .previous_sibling = k_no_index,
                                                     .next_sibling = k_no_index,
                                                     .child_count = 0,
                                                     .first_child = k_no_index});
        const gfss_complex_selector selector = parse_one_complex("body :not(table) a");
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(doc_tree, a_idx),
                                    k_no_scope) == match_verdict::matched);
    }

    std::printf("gfui_complex_match_test: 4 :not() recursive cases checked (one argument, list, "
                "ten levels, dossier SS5.5)\n");
    std::printf("SCANCOUNT gfui_complex_match_test.not_recursive_cases=4\n");
}

// --- case 3: `:scope`, the two D-W6-5 contexts plus a working chain ---

GLINTFX_TEST(match_complex_scope_anchors_to_the_explicit_root_or_the_tree_root) {
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;

    // "com escopo = so ele": an explicit scope only ever matches
    // ITSELF, never a different node, even a directly related one.
    {
        arena tree;
        const std::size_t root_idx = tree.add(entry{.tag = "div",
                                                    .id = "",
                                                    .classes = {},
                                                    .attributes = {},
                                                    .state = glintfx::gfui::gltfx_node_state::none,
                                                    .parent = k_no_index,
                                                    .previous_sibling = k_no_index,
                                                    .next_sibling = k_no_index,
                                                    .child_count = 0,
                                                    .first_child = k_no_index});
        const std::size_t scope_idx = tree.add(entry{.tag = "section",
                                                     .id = "",
                                                     .classes = {},
                                                     .attributes = {},
                                                     .state = glintfx::gfui::gltfx_node_state::none,
                                                     .parent = root_idx,
                                                     .previous_sibling = k_no_index,
                                                     .next_sibling = k_no_index,
                                                     .child_count = 0,
                                                     .first_child = k_no_index});
        const std::size_t other_idx = tree.add(entry{.tag = "section",
                                                     .id = "",
                                                     .classes = {},
                                                     .attributes = {},
                                                     .state = glintfx::gfui::gltfx_node_state::none,
                                                     .parent = root_idx,
                                                     .previous_sibling = k_no_index,
                                                     .next_sibling = k_no_index,
                                                     .child_count = 0,
                                                     .first_child = k_no_index});
        const gltfx_node_view scope_view = glintfx::test::fake_arena::view(tree, scope_idx);
        const gfss_complex_selector selector = parse_one_complex(":scope");
        GLINTFX_CHECK(match_complex(selector, scope_view, scope_view) == match_verdict::matched);
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(tree, other_idx),
                                    scope_view) == match_verdict::rejected);
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(tree, root_idx),
                                    scope_view) == match_verdict::rejected);
    }

    // "sem escopo = raiz": CSS Selectors 4 SS3.5, "if no scoping root,
    // :scope is equivalent to :root" - here, a node with no parent.
    {
        arena tree;
        const std::size_t root_idx = tree.add(entry{.tag = "div",
                                                    .id = "",
                                                    .classes = {},
                                                    .attributes = {},
                                                    .state = glintfx::gfui::gltfx_node_state::none,
                                                    .parent = k_no_index,
                                                    .previous_sibling = k_no_index,
                                                    .next_sibling = k_no_index,
                                                    .child_count = 0,
                                                    .first_child = k_no_index});
        const std::size_t child_idx = tree.add(entry{.tag = "span",
                                                     .id = "",
                                                     .classes = {},
                                                     .attributes = {},
                                                     .state = glintfx::gfui::gltfx_node_state::none,
                                                     .parent = root_idx,
                                                     .previous_sibling = k_no_index,
                                                     .next_sibling = k_no_index,
                                                     .child_count = 0,
                                                     .first_child = k_no_index});
        const gfss_complex_selector selector = parse_one_complex(":scope");
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(tree, root_idx),
                                    k_no_scope) == match_verdict::matched);
        GLINTFX_CHECK(match_complex(selector, glintfx::test::fake_arena::view(tree, child_idx),
                                    k_no_scope) == match_verdict::rejected);
    }

    // "a + :scope casa": :scope as the SUBJECT (rightmost compound),
    // preceded by a working `+` chain - the normal, USABLE order.
    // Never to be confused with ":scope + a" (:scope as the HEAD,
    // S-5's own always-dead selector under D-W6-6 - not this fatia's
    // job, and not exercised here).
    {
        arena tree;
        const std::size_t a_idx = tree.add(entry{.tag = "a",
                                                 .id = "",
                                                 .classes = {},
                                                 .attributes = {},
                                                 .state = glintfx::gfui::gltfx_node_state::none,
                                                 .parent = k_no_index,
                                                 .previous_sibling = k_no_index,
                                                 .next_sibling = k_no_index,
                                                 .child_count = 0,
                                                 .first_child = k_no_index});
        const std::size_t scope_idx = tree.add(entry{.tag = "span",
                                                     .id = "",
                                                     .classes = {},
                                                     .attributes = {},
                                                     .state = glintfx::gfui::gltfx_node_state::none,
                                                     .parent = k_no_index,
                                                     .previous_sibling = a_idx,
                                                     .next_sibling = k_no_index,
                                                     .child_count = 0,
                                                     .first_child = k_no_index});
        tree.entries[a_idx].next_sibling = scope_idx;
        const gltfx_node_view scope_view = glintfx::test::fake_arena::view(tree, scope_idx);
        const gfss_complex_selector selector = parse_one_complex("a + :scope");
        GLINTFX_CHECK(match_complex(selector, scope_view, scope_view) == match_verdict::matched);
    }

    std::printf(
        "gfui_complex_match_test: 3 :scope cases checked (explicit root, tree root, a + :scope)\n");
    std::printf("SCANCOUNT gfui_complex_match_test.scope_cases=3\n");
}

// --- case 4: deferral preserved for what this fatia still does not own ---

GLINTFX_TEST(match_complex_still_defers_placeholder_shown_and_pseudo_elements) {
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;

    arena tree;
    const std::size_t input_idx = tree.add(entry{.tag = "input",
                                                 .id = "",
                                                 .classes = {},
                                                 .attributes = {},
                                                 .state = glintfx::gfui::gltfx_node_state::none,
                                                 .parent = k_no_index,
                                                 .previous_sibling = k_no_index,
                                                 .next_sibling = k_no_index,
                                                 .child_count = 0,
                                                 .first_child = k_no_index});
    const gltfx_node_view node = glintfx::test::fake_arena::view(tree, input_idx);

    {
        const gfss_complex_selector selector = parse_one_complex("input:placeholder-shown");
        GLINTFX_CHECK(match_complex(selector, node, k_no_scope) == match_verdict::deferred);
    }
    {
        const gfss_complex_selector selector = parse_one_complex("input::before");
        GLINTFX_CHECK(match_complex(selector, node, k_no_scope) == match_verdict::deferred);
    }

    // BLOQUEIA #1 da revisão adversarial de d1e677b (2025/2026): os dois
    // casos acima só exercitam o atalho `top.index == 0` de
    // match_complex.cpp - um composto SOZINHO, sem combinador, nunca
    // chama combine(). Os dois casos abaixo cruzam um combinador de
    // verdade, provando que "adiamento e infeccioso" (GODS_LAWS.md L-40)
    // sobrevive à combinação, nas DUAS ordens de argumento de combine()
    // (local deferido + upstream decidido, e o inverso).
    arena crossing_tree;
    const std::size_t crossing_parent_idx =
        crossing_tree.add(entry{.tag = "div",
                                .id = "",
                                .classes = {},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = k_no_index,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
    const std::size_t crossing_child_idx =
        crossing_tree.add(entry{.tag = "input",
                                .id = "",
                                .classes = {},
                                .attributes = {},
                                .state = glintfx::gfui::gltfx_node_state::none,
                                .parent = crossing_parent_idx,
                                .previous_sibling = k_no_index,
                                .next_sibling = k_no_index,
                                .child_count = 0,
                                .first_child = k_no_index});
    const gltfx_node_view crossing_child =
        glintfx::test::fake_arena::view(crossing_tree, crossing_child_idx);

    {
        // Sujeito (`input:placeholder-shown`, local=deferred) cruza o
        // combinador descendente até o ancestral (`div`, local=matched):
        // combine(deferred, matched) tem de continuar deferred. A
        // mutação da revisão (combine() sempre devolvendo `matched`)
        // faria este bloco enxergar `matched` em vez de `deferred`.
        const gfss_complex_selector selector = parse_one_complex("div input:placeholder-shown");
        GLINTFX_CHECK(match_complex(selector, crossing_child, k_no_scope) ==
                      match_verdict::deferred);
    }
    {
        // Ordem invertida dos argumentos de combine(): sujeito
        // (`input`, local=matched) cruza o mesmo combinador até um
        // ancestral cujo PRÓPRIO composto já defere
        // (`div:placeholder-shown`, local=deferred) - combine(matched,
        // deferred) tem de continuar deferred pelo mesmo motivo.
        const gfss_complex_selector selector = parse_one_complex("div:placeholder-shown input");
        GLINTFX_CHECK(match_complex(selector, crossing_child, k_no_scope) ==
                      match_verdict::deferred);
    }

    std::printf("gfui_complex_match_test: 4 deferral-preserved cases checked "
                "(:placeholder-shown, ::before, and 2 crossing a combinator)\n");
    std::printf("SCANCOUNT gfui_complex_match_test.deferral_preserved_cases=4\n");
}

// --- case 5: right-to-left proved by counting, never by reading the source ---

GLINTFX_TEST(match_complex_rejects_the_subject_with_zero_calls_into_the_ancestor) {
    using glintfx::test::fake_counting::counters;
    using glintfx::test::fake_counting::tree;
    using glintfx::test::fake_counting::view;

    glintfx::test::fake_arena::arena arena_tree;
    const std::size_t ancestor_idx = arena_tree.add(
        glintfx::test::fake_arena::entry{.tag = "div",
                                         .id = "",
                                         .classes = {},
                                         .attributes = {},
                                         .state = glintfx::gfui::gltfx_node_state::none,
                                         .parent = glintfx::test::fake_arena::k_no_index,
                                         .previous_sibling = glintfx::test::fake_arena::k_no_index,
                                         .next_sibling = glintfx::test::fake_arena::k_no_index,
                                         .child_count = 0,
                                         .first_child = glintfx::test::fake_arena::k_no_index});
    const std::size_t subject_idx = arena_tree.add(
        glintfx::test::fake_arena::entry{.tag = "",
                                         .id = "actual",
                                         .classes = {},
                                         .attributes = {},
                                         .state = glintfx::gfui::gltfx_node_state::none,
                                         .parent = ancestor_idx,
                                         .previous_sibling = glintfx::test::fake_arena::k_no_index,
                                         .next_sibling = glintfx::test::fake_arena::k_no_index,
                                         .child_count = 0,
                                         .first_child = glintfx::test::fake_arena::k_no_index});

    counters counts;
    const tree counting_tree{.arena = &arena_tree, .counts = &counts};
    const gltfx_node_view subject = view(counting_tree, subject_idx);

    // Subject requires id "nope" (it does not have it); the ancestor
    // requires type "div" (which WOULD hold, if it were ever asked) -
    // F10's own proof: the subject rejects on `id` alone, and the
    // combinator/ancestor half must never be reached at all.
    const gfss_complex_selector selector = parse_one_complex("div #nope");
    const match_verdict verdict = match_complex(selector, subject, k_no_scope);

    GLINTFX_CHECK(verdict == match_verdict::rejected);
    GLINTFX_CHECK_EQ(counts.id, static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(counts.tag_name, static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(counts.parent, static_cast<std::size_t>(0));

    std::printf("gfui_complex_match_test: right-to-left proved by counting - id=%zu tag_name=%zu "
                "parent=%zu\n",
                counts.id, counts.tag_name, counts.parent);
    std::printf("SCANCOUNT gfui_complex_match_test.right_to_left_by_counting_cases=1\n");
}
