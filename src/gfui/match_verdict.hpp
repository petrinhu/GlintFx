// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// match_verdict.hpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-40; docs/plano-w6-folha-de-estilo.md, fatia
// S-2; ESCOPO.md "Ordem de produto de 15/09/2026", Decisao 8): the
// four-value answer the match levels this track owns now speak -
// compound_match.hpp's own match_compound() (one compound, one node,
// no combinator, NEVER answers resource_exhausted - it does not
// allocate) and complex_match.hpp's/deferred_simple_match.hpp's own
// match_complex()/judge_deferred_simple_selectors() (a whole selector,
// one subject node, combinators and all - the two functions that DO
// allocate, an explicit std::vector<frame> stack per call).
//
// MOVED OUT OF compound_match.hpp, RENAMED FROM compound_match_verdict
// (plano S-2, table row 2, "compound_match_verdict deixa de existir"):
// before this fatia, only match_compound() answered in these three
// values. Now match_complex() answers in the SAME three values too -
// a compound_match_verdict named "matched" for a whole complex
// selector's own outcome would be a lie about WHAT was judged (a
// single compound, never a chain of them), the exact kind of
// misnamed-field GODS_LAWS.md L-17's "o nome e o arbitro" already
// forbids. One vocabulary, two callers - not two vocabularies that
// happen to share three spellings.
//
// SEPARATE FILE, NOT FOLDED BACK INTO compound_match.hpp (GODS_LAWS.md
// L-17: "arquivo e atomo de assunto"): this enum is not compound_
// match's own private detail any more - complex_match.hpp/deferred_
// simple_match.hpp both need it without needing compound_match.hpp's
// own match_compound() declaration alongside it.

namespace glintfx::gfui::detail {

// See compound_match.hpp's own header comment for the full reasoning
// behind three values instead of bool (plan SS3.3 of the original
// GFSS-MATCH-SIMPLE plan, unchanged here): "matched" is a complete,
// positive answer; "rejected" is a complete, negative one, decided
// BEFORE anything still-unowned is even looked at ("rejeicao vence
// adiamento", the same rule this file's own two callers both honor);
// "deferred" tells the truth when part of the question belongs to a
// judgment this call still does not own (docs/node-view-and-
// matching.md's own "What is not judged yet" - shrinking fatia by
// fatia, `:placeholder-shown` and the two pseudo-elements are what is
// left of it after this one).
//
// "resource_exhausted" (GFUI-VERDICT-RESOURCE-EXHAUSTED, ESCOPO.md
// Decisao 8, 16/09/2026) IS NOT A FOURTH ANSWER TO "does this selector
// hold" - it is "this call could not FINISH judging that question
// because the heap allocation its own explicit std::vector<frame>
// stack needed failed". Before this value existed, match_complex()
// was `noexcept` AND allocated (complex_match.hpp's own header
// comment used to say so): an allocation failure inside a `noexcept`
// function that lets the resulting std::bad_alloc escape calls
// std::terminate(), ending the CONSUMER's process with no chance to
// react - exactly the defect this value exists to remove. It OUTRANKS
// every other value: a caller that sees it must propagate it
// immediately, never combine it with, or silently downgrade it to,
// "rejected" (a real, confident negative answer this call never
// reached) or "deferred" (a real, honest "ask again once the owner of
// that gap resolves it" - resource exhaustion is neither, it is
// mid-computation failure). Produced ONLY by match_complex() (its own
// std::vector<frame>::reserve()/push_back() catch block,
// complex_match.cpp) and by judge_deferred_simple_selectors() through
// its own recursive call BACK into match_complex() for `:not()`'s
// arguments (deferred_simple_match.cpp's own judge_not()) - never by
// match_compound() itself (compound_match.cpp allocates nothing).
// PUBLIC API IS UNCHANGED (L-22 of the project: no exception ever
// crosses it, still true) and NOTHING here is exported
// (GLINTFX_API) - this remains `glintfx::gfui::detail`'s own internal
// vocabulary; whether/how a future public veredito (`GFSS-API`, W10)
// surfaces this state to the library's consumer is that later fatia's
// own revisao de API, not decided here (plano-w6-folha-de-estilo.md
// SS11).
enum class match_verdict : std::uint8_t { matched, rejected, deferred, resource_exhausted };

} // namespace glintfx::gfui::detail
