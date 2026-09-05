// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

// structural_match.hpp - GFSS-MATCH-STRUCT (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-40; docs/node-view-and-matching.md's own "What is
// not judged yet" section, item "Structural pseudo-classes"): the
// evaluator behind compound_match.cpp's own switch, filling in the
// structural half of what that section named as deferred - every
// pseudo-class that needs to walk to OTHER nodes (siblings) or count
// something about the node's own content, which a single-node judgment
// alone cannot answer, unlike the five state bits and the id/tag/class
// requirements compound_match.cpp's own PASS 2 already judges without
// leaving the node it was handed.
//
// ELEVEN NAMES, TWO SHAPES (selector_pseudo_vocabulary.hpp's own
// closed lists, cross-checked against TODO.md's own GFSS-MATCH-STRUCT
// row): seven SIMPLE pseudo-classes with no argument (first-child,
// last-child, only-child, first-of-type, last-of-type, only-of-type,
// empty) and four FUNCTIONAL ones that take an An+B argument
// (nth-child, nth-last-child, nth-of-type, nth-last-of-type) - the
// fifth functional name, "not", is NOT structural (it is combinator
// work, docs/node-view-and-matching.md's own "depends on the
// combinator work above") and stays deferred by compound_match.cpp,
// never routed here. `:placeholder-shown` and `:scope` are the two
// simple pseudo-classes this fatia does NOT own either (the doc's own
// "two gaps beyond that division of labor") and stay deferred too.
//
// COST IS LINEAR BY DESIGN, NOT AN OVERSIGHT (TODO.md's own GFSS-
// MATCH-STRUCT row, quoting the format's own documentation: "a
// referência declara estruturais 'não indexados' - custo linear por
// consulta é o esperado da categoria; cache é otimização pós-medição,
// não entra na fatia"): every function below walks siblings with the
// eight-fact contract's own previous_sibling()/next_sibling() one node
// at a time. No index, no cache, no memoized position is built
// anywhere in this file - GODS_LAWS.md L-32's own scope discipline
// applies here just as much as to a whole new capability: the fatia
// that was ordered is a linear-cost evaluator, not a faster one nobody
// asked for yet.
//
// SEPARATE FILE FROM compound_match.cpp/.hpp AND FROM attribute_match.
// hpp/.cpp - deliberate (GODS_LAWS.md L-17: "cada fatia distinta com
// sua fronteira propria"): this file answers "does this ONE structural
// requirement hold", never how a whole compound's several requirements
// (id/state/tag/classes/attribute/structural) assemble into one
// verdict - that assembly stays in compound_match.cpp, which routes
// into this file the same way it already routes into gfui/node_query.
// hpp and gfui/attribute_match.hpp for its own owned requirements.

namespace glintfx::gfui::detail {

// The seven argument-less structural names (selector_pseudo_
// vocabulary.hpp's own GLINTFX_GFSS_SIMPLE_PSEUDO_LIST rows 6-12, in
// the SAME order) - a closed enumeration, GODS_LAWS.md L-40's own "the
// space is small, enumerate it whole".
enum class structural_simple_kind : std::uint8_t {
    first_child,
    last_child,
    only_child,
    first_of_type,
    last_of_type,
    only_of_type,
    empty,
};

struct structural_simple_entry {
    std::string_view name;
    structural_simple_kind kind = structural_simple_kind::first_child;
};

inline constexpr std::array<structural_simple_entry, 7> k_structural_simple_table{{
    {"first-child", structural_simple_kind::first_child},
    {"last-child", structural_simple_kind::last_child},
    {"only-child", structural_simple_kind::only_child},
    {"first-of-type", structural_simple_kind::first_of_type},
    {"last-of-type", structural_simple_kind::last_of_type},
    {"only-of-type", structural_simple_kind::only_of_type},
    {"empty", structural_simple_kind::empty},
}};

// The four functional structural names (selector_pseudo_vocabulary.hpp
//'s own GLINTFX_GFSS_FUNCTIONAL_PSEUDO_LIST, EXCLUDING "not" - see
// this file's own header comment above for why "not" is combinator
// work, never structural).
enum class structural_functional_kind : std::uint8_t {
    nth_child,
    nth_last_child,
    nth_of_type,
    nth_last_of_type,
};

struct structural_functional_entry {
    std::string_view name;
    structural_functional_kind kind = structural_functional_kind::nth_child;
};

inline constexpr std::array<structural_functional_entry, 4> k_structural_functional_table{{
    {"nth-child", structural_functional_kind::nth_child},
    {"nth-last-child", structural_functional_kind::nth_last_child},
    {"nth-of-type", structural_functional_kind::nth_of_type},
    {"nth-last-of-type", structural_functional_kind::nth_last_of_type},
}};

// Membership lookups - ASCII case-insensitive, the SAME policy
// state_pseudo_class_table.hpp's own state_bit_for_pseudo_class()
// already applies for the identical reason (a pseudo-class name is
// vocabulary the LANGUAGE defines, D-MS-4, and selector_parse.cpp's
// own is_known_simple_pseudo()/is_known_functional_pseudo() already
// accept any case at parse time). nullopt means "not one of mine" -
// compound_match.cpp's own collect_requirements() uses that to decide
// whether a pseudo_class/pseudo_function selector is structural,
// state, or still deferred (placeholder-shown, scope, not).
[[nodiscard]] std::optional<structural_simple_kind>
structural_simple_kind_for_pseudo_class(std::string_view name) noexcept;

[[nodiscard]] std::optional<structural_functional_kind>
structural_functional_kind_for_name(std::string_view name) noexcept;

// Evaluates one of the seven argument-less structural pseudo-classes
// against `node`. noexcept, zero allocation - the same guarantee every
// other evaluator in this matcher already carries.
[[nodiscard]] bool structural_simple_holds(structural_simple_kind kind,
                                           const gltfx_node_view &node) noexcept;

// Evaluates one of the four An+B structural pseudo-classes against
// `node`. `raw_argument` is the text selector_parse.cpp's own parse_
// functional_pseudo() captured (selector_ast.hpp's own gfss_simple_
// selector::raw_argument) - this function re-parses it itself via
// gfss/anb_parse.hpp's own parse_anb() (the PARSED gfss_anb value is
// not stored anywhere in the AST, by GFSS-SPECIFICITY's own design -
// see anb_parse.hpp's own header comment).
//
// PRECONDITION, NOT A HOSTILE-INPUT CASE (GFSS-SEL-PARSE-NTH reopened
// 05/09/2026, GODS_LAWS.md L-20/L-40): `raw_argument` is valid An+B
// syntax by the time it reaches a real match_compound() call -
// selector_parse.cpp's own attach_anb_validation() already refused any
// selector whose nth-* argument fails parse_anb() at PARSE time, with
// a diagnostic (line, column, what was expected), the same refusal the
// project leader already chose once for an analogous silent-failure
// shape (ESCOPO.md, 02/09/2026 decision 1). A malformed argument
// reaching THIS function is therefore an INTERNAL CONTRACT VIOLATION,
// not a leaf author's mistake - structural_match.cpp's own
// implementation asserts on it (Debug diagnostic) and falls back to
// "never matches" (a defined, safe Release-build answer) rather than
// crashing a correctly-behaving consumer's process.
[[nodiscard]] bool structural_functional_holds(structural_functional_kind kind,
                                               std::string_view raw_argument,
                                               const gltfx_node_view &node) noexcept;

} // namespace glintfx::gfui::detail
