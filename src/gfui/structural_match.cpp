// SPDX-License-Identifier: AGPL-3.0-or-later
#include "structural_match.hpp"

#include <cassert>
#include <cstddef>

#include "gfss/anb.hpp"
#include "gfss/anb_parse.hpp"
#include "gfss/ascii_case.hpp"
#include "gfui/node_query.hpp"

// structural_match.cpp - GFSS-MATCH-STRUCT (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40): the algorithm behind structural_match.hpp's own
// seven simple and four functional evaluators - see that file's own
// header comment for scope, the eleven-name closed vocabulary, and
// why the traversal below stays linear on purpose.
//
// EVERY QUESTION IS ITS OWN NAMED FUNCTION (GODS_LAWS.md L-17's own
// "a frase sem e"), THEN TWO SMALL DISPATCHERS AT THE BOTTOM: the
// eleven pseudo-classes read as eleven small, separately named
// answers - not one function with an eleven-way if/else that would
// have to be described with "and".

namespace glintfx::gfui::detail {

namespace {

// "first-child": no PREVIOUS element sibling at all - the eight-fact
// contract's own previous_sibling() already skips text (docs/node-
// view-and-matching.md's own "child_count sees content, navigating
// children sees only elements", ESCOPO.md's own confirmation of the
// same rule), so this is exactly CSS's own notion of "no earlier
// element sibling", with no separate "skip text nodes" step needed.
[[nodiscard]] bool is_first_child(const gltfx_node_view &node) noexcept {
    return is_null(previous_sibling(node));
}

[[nodiscard]] bool is_last_child(const gltfx_node_view &node) noexcept {
    return is_null(next_sibling(node));
}

[[nodiscard]] bool is_only_child(const gltfx_node_view &node) noexcept {
    return is_first_child(node) && is_last_child(node);
}

// "of-type" variants count only siblings sharing `node`'s own tag
// (TODO.md's own GFSS-MATCH-STRUCT row: "variantes of-type (conta
// irmãos de mesma tag)") - ASCII case-insensitive, the SAME D-MS-4
// policy compound_match.cpp's own type_holds() already applies to tag
// comparison (tag is vocabulary the LANGUAGE defines, not an author
// identifier).
[[nodiscard]] bool is_first_of_type(const gltfx_node_view &node) noexcept {
    const std::string_view own_tag = tag_name(node);
    gltfx_node_view cursor = previous_sibling(node);
    while (!is_null(cursor)) {
        if (glintfx::style::detail::ascii_case_insensitive_equal(tag_name(cursor), own_tag)) {
            return false;
        }
        cursor = previous_sibling(cursor);
    }
    return true;
}

[[nodiscard]] bool is_last_of_type(const gltfx_node_view &node) noexcept {
    const std::string_view own_tag = tag_name(node);
    gltfx_node_view cursor = next_sibling(node);
    while (!is_null(cursor)) {
        if (glintfx::style::detail::ascii_case_insensitive_equal(tag_name(cursor), own_tag)) {
            return false;
        }
        cursor = next_sibling(cursor);
    }
    return true;
}

[[nodiscard]] bool is_only_of_type(const gltfx_node_view &node) noexcept {
    return is_first_of_type(node) && is_last_of_type(node);
}

// ":empty" - CSS's own rule is "no children at all", and this eight-
// fact contract answers that with child_count() (fact 8's own COUNT
// half), never with first_child()/next_sibling() navigation: those two
// deliberately see ONLY elements (docs/node-view-and-matching.md's own
// wording, reaffirmed as decision 5 in ESCOPO.md's "02/09" log), so a
// node holding nothing but a run of plain text would have a null
// first_child and zero navigable siblings while still very much having
// CONTENT - exactly the node ":empty" must NOT match. child_count()
// counts that content (element AND text alike, the same fact 8 half
// fake_arena_tree.hpp's own `entry::child_count` comment documents),
// so it is the one fact that answers the real question instead of the
// navigable-elements-only approximation.
[[nodiscard]] bool is_empty(const gltfx_node_view &node) noexcept { return child_count(node) == 0; }

// Position of `node` among its own siblings, ONE-indexed (CSS's own
// "the An+B-th element" always starts counting at 1, never 0 - the
// classic off-by-one this evaluator exists to get right). When
// `same_type_only` is true, only siblings sharing `node`'s own tag are
// counted - the shared walk nth-of-type/nth-last-of-type need, the
// same tag policy is_first_of_type() above already applies.
//
// Linear, one previous_sibling()/next_sibling() call per hop
// (structural_match.hpp's own header comment: "custo linear... cache é
// otimização pós-medição, não entra na fatia").
[[nodiscard]] std::size_t position_from_start(const gltfx_node_view &node,
                                              bool same_type_only) noexcept {
    const std::string_view own_tag = tag_name(node);
    std::size_t position = 1;
    gltfx_node_view cursor = previous_sibling(node);
    while (!is_null(cursor)) {
        if (!same_type_only ||
            glintfx::style::detail::ascii_case_insensitive_equal(tag_name(cursor), own_tag)) {
            ++position;
        }
        cursor = previous_sibling(cursor);
    }
    return position;
}

[[nodiscard]] std::size_t position_from_end(const gltfx_node_view &node,
                                            bool same_type_only) noexcept {
    const std::string_view own_tag = tag_name(node);
    std::size_t position = 1;
    gltfx_node_view cursor = next_sibling(node);
    while (!is_null(cursor)) {
        if (!same_type_only ||
            glintfx::style::detail::ascii_case_insensitive_equal(tag_name(cursor), own_tag)) {
            ++position;
        }
        cursor = next_sibling(cursor);
    }
    return position;
}

// "Is there a non-negative integer n with a*n + b == position?" - the
// literal reading of CSS Syntax Module Level 3's own "the An+B-th
// element of a list" (An+B microsyntax, read under GODS_LAWS.md L-29),
// for a single already-computed 1-based `position`. `a == 0` collapses
// to "position equals b exactly" (":nth-child(5)" parses to a=0, b=5,
// anb_parse.hpp's own header comment); otherwise the candidate n is
// (position - b) / a, and it counts only when that division is EXACT
// and the quotient is not negative (n ranges over 0, 1, 2, ... only).
[[nodiscard]] bool anb_matches_position(const style::detail::gfss_anb &anb,
                                        std::size_t position) noexcept {
    const long long diff = static_cast<long long>(position) - anb.b;
    if (anb.a == 0) {
        return diff == 0;
    }
    if (diff % anb.a != 0) {
        return false;
    }
    return (diff / anb.a) >= 0;
}

} // namespace

std::optional<structural_simple_kind>
structural_simple_kind_for_pseudo_class(std::string_view name) noexcept {
    for (const structural_simple_entry &entry : k_structural_simple_table) {
        if (glintfx::style::detail::ascii_case_insensitive_equal(name, entry.name)) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

std::optional<structural_functional_kind>
structural_functional_kind_for_name(std::string_view name) noexcept {
    for (const structural_functional_entry &entry : k_structural_functional_table) {
        if (glintfx::style::detail::ascii_case_insensitive_equal(name, entry.name)) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

bool structural_simple_holds(structural_simple_kind kind, const gltfx_node_view &node) noexcept {
    switch (kind) {
    case structural_simple_kind::first_child:
        return is_first_child(node);
    case structural_simple_kind::last_child:
        return is_last_child(node);
    case structural_simple_kind::only_child:
        return is_only_child(node);
    case structural_simple_kind::first_of_type:
        return is_first_of_type(node);
    case structural_simple_kind::last_of_type:
        return is_last_of_type(node);
    case structural_simple_kind::only_of_type:
        return is_only_of_type(node);
    case structural_simple_kind::empty:
        return is_empty(node);
    }
    // Every structural_simple_kind enumerator is handled above - no
    // default case, so a kind added to the enum without a branch here
    // fails to compile under -Werror (GODS_LAWS.md L-40's own "closed
    // enumeration is not closed" lesson).
    return false;
}

bool structural_functional_holds(structural_functional_kind kind, std::string_view raw_argument,
                                 const gltfx_node_view &node) noexcept {
    const style::detail::anb_parse_result parsed = style::detail::parse_anb(raw_argument);
    // INTERNAL CONTRACT VIOLATION, NOT HOSTILE LEAF CONTENT ANY MORE
    // (GFSS-SEL-PARSE-NTH reopened 05/09/2026, GODS_LAWS.md L-20/L-40):
    // `raw_argument` reaching a real match_compound() call was already
    // validated by selector_parse.cpp's own attach_anb_validation() at
    // PARSE time - a leaf author's own malformed text (`:nth-child
    // (banana)`) is refused there and never reaches this evaluator at
    // all any more. A failure HERE means some OTHER caller (a hand-
    // built AST in a test, a future integration bypassing the parser)
    // handed this evaluator text the validated pipeline never approved
    // - the SAME "assert() for the Debug diagnostic, a defined value
    // for the Release build that never crashes a correctly-behaving
    // consumer's process" two-reaction shape numeric_lexeme.cpp's own
    // decode_number_lexeme() and named_colors.cpp's own named_color_
    // at() already use for an analogous internal-only invariant.
    assert(parsed.ok &&
           "structural_functional_holds(): raw_argument was not a valid An+B expression - "
           "GFSS-SEL-PARSE-NTH's own attach_anb_validation() should have refused this at parse "
           "time, before it ever reached match_compound()");
    if (!parsed.ok) {
        return false;
    }
    switch (kind) {
    case structural_functional_kind::nth_child:
        return anb_matches_position(parsed.value, position_from_start(node, false));
    case structural_functional_kind::nth_last_child:
        return anb_matches_position(parsed.value, position_from_end(node, false));
    case structural_functional_kind::nth_of_type:
        return anb_matches_position(parsed.value, position_from_start(node, true));
    case structural_functional_kind::nth_last_of_type:
        return anb_matches_position(parsed.value, position_from_end(node, true));
    }
    // Every structural_functional_kind enumerator is handled above -
    // no default case, same GODS_LAWS.md L-40 reasoning as above.
    return false;
}

} // namespace glintfx::gfui::detail
