// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/gfui/node_view.hpp>

#include "gfss/selector_ast.hpp"

// compound_match.hpp - GFSS-MATCH-SIMPLE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-28/L-40; /var/tmp/glintfx-plan/gfss-match-
// simple-plano.md): "decide se UM seletor composto casa com UM nó,
// usando só os fatos que o próprio nó responde, sem olhar para nenhum
// outro nó" (plan SS1, one sentence, GODS_LAWS.md L-17).
//
// SCOPE, WITH THE DESTINATION OF EVERYTHING LEFT OUT (plan SS1's own
// table): type/universal/class/id and the five STATE pseudo-classes
// (hover/active/focus/focus-visible/checked) are judged HERE. Every
// structural pseudo-class (first-child, last-child, ...), :scope,
// :placeholder-shown, every functional pseudo-class (nth-*, :not) and
// every attribute selector are judged by a LATER evaluator this fatia
// does not own (GFSS-MATCH-STRUCT/GFSS-MATCH-ATTR/GFSS-MATCH-COMBINE,
// TODO.md waves W5/W6) - see compound_match_verdict::deferred below for
// how this file tells its caller that. Combinators and specificity are
// never seen here at all: this file's own entry point takes a single
// gfss_compound_selector, never a gfss_complex_selector's own `rest`.
//
// WHY src/gfui/, NOT src/gfss/ (D-MS-1 of the plan above): ESCOPO.md
// SS4 fixes the trio - gfss is the leaf FORMAT (data), gfml the
// markup (data), gfui the motor (code, exported symbol). Matching a
// selector against a node is motor work, not format work - the SAME
// reasoning node_view.hpp's own header comment already gives for why
// the node contract itself lives in gfui/, not gfss/. This file
// depends on gfss/selector_ast.hpp (motor -> format, the correct
// direction) but nothing in gfss/ ever depends back on gfui/.
//
// INTERNAL IN THIS SLICE (GODS_LAWS.md L-19: "o header nasce interno"):
// not installed, not exported, no GLINTFX_API anywhere in this file.
// GFSS-API (TODO.md, wave W10) is the dedicated review that decides
// what, if anything, of this gets promoted to include/glintfx/.

namespace glintfx::gfui::detail {

// The answer of match_compound(): matched (every simple selector of
// the compound was judged HERE and all hold), rejected (at least one
// judged here does NOT hold - decided BEFORE any deferred requirement
// is even looked at, plan SS3.1's own "rejeicao vence adiamento"),
// deferred (none judged here failed, but the compound also carries at
// least one simple selector this evaluator does not own - a structural
// pseudo-class, :scope, :placeholder-shown, a functional pseudo-class
// or an attribute selector - so the final answer belongs to a later
// evaluator, GFSS-MATCH-COMBINE's own W6).
//
// WHY THREE VALUES, NOT bool (plan SS3.3): with bool, a compound
// "a:first-child" would have to answer true (a lie: :first-child was
// never looked at) or false (also a lie: it might match once the
// structural half is judged) - either one is exactly the "green
// without looking" GODS_LAWS.md L-40 forbids. deferred tells the
// truth: "what is mine is settled; what is not mine is still open".
enum class compound_match_verdict : std::uint8_t { matched, rejected, deferred };

// `compound` is one gfss_compound_selector (glued simple selectors,
// e.g. "button.primary#ok" - no combinator inside it); `node` must have
// node != nullptr and a complete gltfx_node_facts table (the SAME
// precondition node_query.hpp's own forwarders already carry - the
// caller, GFSS-API, validates it at the query's own entry via gltfx_
// node_facts_first_missing(), not here). noexcept: no allocation
// anywhere in the algorithm (plan SS3.8), the same guarantee every
// gltfx_node_facts callback already carries across the ABI boundary.
[[nodiscard]] compound_match_verdict
match_compound(const style::detail::gfss_compound_selector &compound,
               const gltfx_node_view &node) noexcept;

} // namespace glintfx::gfui::detail
