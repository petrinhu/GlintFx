// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// match_verdict.hpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-40; docs/plano-w6-folha-de-estilo.md, fatia S-2):
// the three-value answer BOTH match levels this track owns now speak -
// compound_match.hpp's own match_compound() (one compound, one node,
// no combinator) and complex_match.hpp's own match_complex() (a whole
// selector, one subject node, combinators and all).
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
enum class match_verdict : std::uint8_t { matched, rejected, deferred };

} // namespace glintfx::gfui::detail
