// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "sheet_ast.hpp"

// sheet_parse.hpp - GFSS-SHEET-PARSE (TODO.md, GODS_LAWS.md L-17/L-19/
// L-20/L-27/L-28/L-40): reads a WHOLE gfss style sheet's TEXT into
// sheet_ast.hpp's own gfss_sheet_parse_result - the entry point
// GFSS-SEL-PARSE-CORE's own header comment already calls "the leader of
// this whole track's fifth row" in spirit: it is what turns raw text
// into every OTHER fatia's own input (GFSS-CASCADE, ANIM-KEYFRAMES).
//
// RECOVERY BY RULE, THE SAME "discard only the current one" DISCIPLINE
// declaration_list_parse.hpp's own header comment already documents one
// layer down (CSS Syntax Module Level 3 SS2.2/SS5.4.5): a malformed
// selector, or a declaration block this fatia cannot even find the close
// of, never stops the WHOLE sheet from being read - see sheet_parse.cpp's
// own header comment for the ONE condition that DOES stop it (an
// unclosed `{`, which leaves no reliable place to resume from at all).
//
// THREE-WAY AT-RULE DISPATCH (at_rule_vocabulary.hpp's own header
// comment): `@media` is recognized and ignored, with a diagnostic naming
// it out-of-v1; `@keyframes` is recognized and its whole block captured
// byte for byte, with NO diagnostic (it is accepted v1 vocabulary,
// GODS_LAWS.md L-28 decisions 23-24); anything else is an unknown
// at-rule, ignored, with a diagnostic naming what IS supported.
//
// INTERNAL, ON PURPOSE (GODS_LAWS.md L-19: "o header nasce interno, em
// src/gfss/"), the SAME "public type via headers under include/glintfx/,
// internal parser under src/" split every sibling *_parse.hpp in this
// track already establishes - parse_sheet() below carries no
// GLINTFX_API. GFSS-API (TODO.md, wave W10) decides the public shape.
//
// SCOPE OF THIS FATIA'S OWN CUT: see sheet_ast.hpp's own header comment
// for what is deliberately NOT here (per-selector specificity, the
// `:scope`+sibling dead-selector check, `:where()`) - each belongs to a
// sibling fatia this one's own TODO.md row does not name as a
// prerequisite.
//
// NOT noexcept, the SAME REASON tokenizer.hpp's OWN gltfx_gfss_
// tokenize() AND selector_parse.hpp's OWN parse_selector_list() ARE
// NOT EITHER: every collection this function builds (the token vector,
// every list of gfss_sheet_parse_result) is built with push_back -
// std::bad_alloc is never caught on this path.

namespace glintfx::style::detail {

// Analyzes the WHOLE of `source` as one gfss style sheet - zero or more
// style rules and at-rules, in source order, with recovery by rule (see
// this file's own header comment above). `source` must outlive every
// std::string_view this result's own fields point into (selectors' own
// lexemes, a raw block's own `prelude`/`raw_body`, every diagnostic's
// own `detail`) - the SAME non-owning-view lifetime rule every other
// *_parse_result in this track already carries.
[[nodiscard]] gfss_sheet_parse_result parse_sheet(std::string_view source);

} // namespace glintfx::style::detail
