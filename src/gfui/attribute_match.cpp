// SPDX-License-Identifier: AGPL-3.0-or-later
#include "attribute_match.hpp"

#include <cstddef>
#include <string_view>

#include "gfui/node_query.hpp"

// attribute_match.cpp - GFSS-MATCH-ATTR (TODO.md, GODS_LAWS.md
// L-17/L-20/L-22/L-40): the algorithm behind attribute_match.hpp's own
// attribute_selector_holds() - see that file's own header comment for
// scope, the six-vs-seven operator count, and the name/value case
// policy.
//
// EVERY OPERATOR IS ITS OWN NAMED FUNCTION (GODS_LAWS.md L-17's own
// "a frase sem e"): one function, one CSS attribute-matching rule, so
// the name alone states what it checks, and a defect in one operator
// never hides inside a branch of another. token_matches_value() below
// is the ONE piece three of them (includes, dash_match by way of its
// own "value or value-dash-prefix" shape) would otherwise duplicate.

namespace glintfx::gfui::detail {

namespace {

// "[foo~=\"bar\"]" (CSS Selectors' own "includes" operator, read under
// GODS_LAWS.md L-29): `value` splits on ASCII whitespace into TOKENS
// (never an empty token - two consecutive spaces do not produce one),
// and the selector holds if any token equals `needle` exactly. An
// EMPTY `needle` never matches: no token produced by splitting is ever
// itself empty, so the loop below already rejects it without a
// separate guard - the same "correct by construction, not by a special
// case" shape dash_match_holds() below also relies on.
[[nodiscard]] bool includes_holds(std::string_view value, std::string_view needle) noexcept {
    std::size_t pos = 0;
    while (pos < value.size()) {
        while (pos < value.size() && value[pos] == ' ') {
            ++pos;
        }
        const std::size_t token_start = pos;
        while (pos < value.size() && value[pos] != ' ') {
            ++pos;
        }
        if (pos > token_start && value.substr(token_start, pos - token_start) == needle) {
            return true;
        }
    }
    return false;
}

// "[foo|=\"bar\"]" (CSS Selectors' own "dash match" / language-range
// operator): holds if `value` equals `needle` EXACTLY, or if `value`
// starts with `needle` immediately followed by a hyphen - the rule a
// selector like `[lang|="pt"]` needs to match "pt" and "pt-BR" but not
// "ptx" (the hyphen must be the very next byte, not merely present
// somewhere later in `value`).
[[nodiscard]] bool dash_match_holds(std::string_view value, std::string_view needle) noexcept {
    if (value == needle) {
        return true;
    }
    return value.size() > needle.size() && value.starts_with(needle) && value[needle.size()] == '-';
}

// "[foo^=\"bar\"]", "[foo$=\"bar\"]", "[foo*=\"bar\"]" (CSS Selectors'
// own prefix/suffix/substring operators): CSS's own rule, and the case
// nearly everyone gets wrong (compound_match.hpp's own service order
// names it explicitly) - an EMPTY `needle` never matches ANY value,
// including an empty one. Without this guard, `"".starts_with("")` is
// true by plain string semantics, and `[foo^=""]` would silently match
// every node carrying the attribute at all - a selector nobody who
// wrote `^=""` actually meant.
[[nodiscard]] bool prefix_match_holds(std::string_view value, std::string_view needle) noexcept {
    return !needle.empty() && value.starts_with(needle);
}

[[nodiscard]] bool suffix_match_holds(std::string_view value, std::string_view needle) noexcept {
    return !needle.empty() && value.ends_with(needle);
}

[[nodiscard]] bool substring_match_holds(std::string_view value, std::string_view needle) noexcept {
    return !needle.empty() && value.find(needle) != std::string_view::npos;
}

// Dispatch over gfss_attribute_operator (selector_ast.hpp's own six-
// member enum) - reached only when `simple.has_attribute_value` is
// true (attribute_selector_holds() below handles bare presence, the
// seventh outcome that has no operator value at all, before this is
// ever called).
[[nodiscard]] bool operator_holds(style::detail::gfss_attribute_operator op, std::string_view value,
                                  std::string_view needle) noexcept {
    switch (op) {
    case style::detail::gfss_attribute_operator::equals:
        return value == needle;
    case style::detail::gfss_attribute_operator::includes:
        return includes_holds(value, needle);
    case style::detail::gfss_attribute_operator::dash_match:
        return dash_match_holds(value, needle);
    case style::detail::gfss_attribute_operator::prefix_match:
        return prefix_match_holds(value, needle);
    case style::detail::gfss_attribute_operator::suffix_match:
        return suffix_match_holds(value, needle);
    case style::detail::gfss_attribute_operator::substring_match:
        return substring_match_holds(value, needle);
    }
    // Every gfss_attribute_operator enumerator is handled above - no
    // default case, so an operator added to the enum without a branch
    // here fails to compile under -Werror (GODS_LAWS.md L-40's own
    // "closed enumeration is not closed" lesson, selector_ast.hpp's
    // own header comment on the SAME technique for gfss_combinator).
    return false;
}

} // namespace

bool attribute_selector_holds(const style::detail::gfss_simple_selector &simple,
                              const gltfx_node_view &node) noexcept {
    const gltfx_node_attribute answer = attribute(node, simple.name);
    if (!answer.present) {
        return false;
    }
    if (!simple.has_attribute_value) {
        // "[foo]" - bare presence, any value, including an empty one
        // (docs/node-view-and-matching.md's own "present == true with
        // an empty value means the attribute exists and its value
        // happens to be empty").
        return true;
    }
    return operator_holds(simple.attribute_operator, answer.value, simple.attribute_value);
}

} // namespace glintfx::gfui::detail
