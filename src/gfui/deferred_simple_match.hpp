// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfui/node_view.hpp>

#include "gfss/selector_ast.hpp"
#include "gfui/match_verdict.hpp"

// deferred_simple_match.hpp - GFSS-MATCH-COMBINE (TODO.md, GODS_
// LAWS.md L-17/L-19/L-20/L-22/L-28/L-40; docs/plano-w6-folha-de-
// estilo.md, fatia S-2): judges, at ONE node, with a scope, the simple
// selectors of a compound that compound_match.hpp's own match_
// compound() marks deferred AND that THIS fatia now owns -
// `:not(...)` and `:scope` anchoring (D-W6-2's own dossier answer,
// F2's own contract) - never re-judging what match_compound() already
// settled (its own caller, complex_match.cpp, only reaches this
// function once match_compound() has already returned `deferred` for
// the SAME compound, "rejeicao vence adiamento" already spent).
//
// WHY A SEPARATE FILE FROM compound_match.cpp, NOT A NEW BRANCH THERE
// (GODS_LAWS.md L-17 "arquivo e atomo de assunto"): compound_match.cpp
// judges a compound against `node` ALONE, no other node ever consulted
// (its own header comment's own scope line) - `:not()` needs to run
// the FULL two-level match (compound AND combinator) on its own
// argument selectors, and `:scope` needs a second node (the scope
// root) neither match_compound()'s own signature nor its own algorithm
// ever receives. Folding either in would give compound_match.cpp a
// second reason to change (a combinator-level concern bleeding into a
// single-node evaluator), exactly what L-17 forbids.
//
// WHAT STILL DEFERS THROUGH THIS FUNCTION TOO, ON PURPOSE:
// `:placeholder-shown` (docs/node-view-and-matching.md's own gap 1,
// unresolved by product decision) and every pseudo-element
// (`::before`/`::after`, LAYOUT-PSEUDO-BOXES's own future scope) -
// neither is owned HERE either, so a compound carrying one of those
// alongside a satisfied `:not()`/`:scope` still comes back `deferred`
// overall, never a false `matched` (GODS_LAWS.md L-40's own "green
// without looking").
//
// `:where()` IS NOT THIS FATIA'S CONCERN (D-W6-12/D-W6-7, plano S-4,
// DESBLOQUEADA but scheduled AFTER this one): the vocabulary does not
// exist yet in selector_pseudo_vocabulary.hpp, so no compound this
// fatia's own parser produces can carry it - nothing to special-case
// here in advance of S-4 actually adding it.

namespace glintfx::gfui::detail {

// `compound` is the SAME compound match_compound() already judged
// `deferred` for `node`; `scope` is the query's own scoping root
// (`scope.node == nullptr` means "no explicit scope" - D-W6-5's own
// second context, where `:scope` behaves as `:root` and holds only for
// a node with no parent). Returns `rejected` the instant any owned
// deferred selector fails (same "rejeicao vence adiamento" order this
// whole track uses), `matched` when every owned deferred selector
// holds AND nothing still-unowned (`:placeholder-shown`, a pseudo-
// element) is present, `deferred` otherwise. `noexcept`, but NOT
// allocation-free (same gap complex_match.hpp's own header comment
// corrects, GODS_LAWS.md L-17's "gêmeo"): the recursion `:not()`
// drives (through complex_match.hpp's own match_complex(), called back
// from this function's own .cpp) is bounded by selector_parse.cpp's
// own k_max_nested_selector_list_depth parse-time limit (D-W6-8),
// never by anything this function enforces itself - but each level of
// that bounded recursion calls back into match_complex(), which DOES
// allocate (its own explicit heap stack, `std::vector<frame>`). A
// `noexcept` function calling into an allocating one carries the SAME
// consequence documented there: allocation failure surfaces as
// `std::terminate()`, not as a caught exception this function's own
// caller could recover from.
[[nodiscard]] match_verdict
judge_deferred_simple_selectors(const style::detail::gfss_compound_selector &compound,
                                const gltfx_node_view &node, const gltfx_node_view &scope) noexcept;

} // namespace glintfx::gfui::detail
