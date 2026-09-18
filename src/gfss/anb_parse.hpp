// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/gfss/token.hpp>

#include "anb.hpp"

// anb_parse.hpp - GFSS-SEL-PARSE-NTH (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-27/L-40; source read under L-29/L-43: CSS
// Syntax Module Level 3 SS6 "The An+B microsyntax",
// https://www.w3.org/TR/css-syntax-3/#anb-microsyntax, and its own
// SS6.1 "Informal Syntax Description" for the whitespace/sign-omission
// rules, cross-checked against MDN's own :nth-child() page - see this
// track's own PR/report for the exact quotes each production below
// answers to): microparses the raw argument text selector_parse.cpp's
// own parse_functional_pseudo() (GFSS-SEL-PARSE-CORE) already captured,
// UNANALYZED, for the four nth-* functional pseudo-classes (nth-child,
// nth-last-child, nth-of-type, nth-last-of-type - "not" does not take
// an An+B argument, a selector list instead, GFSS-SEL-PARSE-NOT's own
// job) - this fatia's own scope line, "guarda o argumento cru para as
// duas fatias seguintes", GFSS-SEL-PARSE-NTH's half of it.
//
// NOT WIRED INTO THE SELECTOR AST'S OWN STORAGE, BUT CALLED AT PARSE
// TIME SINCE 05/09/2026 (GFSS-SEL-PARSE-NTH reopened, GODS_LAWS.md
// L-20/L-40 - a defect found by measurement: `:nth-child(banana)` used
// to parse as a VALID selector whose argument nobody ever read,
// producing a rule that silently never matched): selector_parse.cpp's
// own attach_anb_validation() now calls parse_anb() on every nth-*
// functional pseudo-class's own raw argument the moment it is
// captured, and REJECTS the whole selector with parse_anb()'s own
// diagnostic if it is not valid An+B syntax - the same "aceitar e
// nunca casar e falha silenciosa" refusal the project leader already
// made once for an unrelated contract (ESCOPO.md, 02/09/2026 decision
// 1), applied here. What is STILL true, and is a narrower claim than
// before: selector_ast.hpp's own gfss_simple_selector carries only the
// RAW argument text for a `pseudo_function` selector, never a parsed
// gfss_anb of its own - the PARSED value from this validation call is
// discarded, only its validity kept. Deciding WHETHER and HOW that
// struct grows a parsed gfss_anb field (so GFSS-MATCH-STRUCT would not
// have to re-parse the same text a second time at match time) is
// GFSS-SPECIFICITY's own call (or a dedicated integration fatia), not
// this one's - the SAME "GFSS-API decides the public shape, this fatia
// does not" boundary selector_parse.hpp's own header comment already
// draws for a different question.
//
// LAYERED ON TOP OF GFSS-TOKEN'S OWN TOKEN STREAM, THE SAME
// RELATIONSHIP EVERY OTHER PARSER IN THIS TRACK ALREADY HAS
// (color_parse.cpp/selector_parse.cpp's own header comments) - BUT,
// SINCE NOEXCEPT-ALLOC-B8 FATIA F4 (/var/tmp/glintfx-plan/plano-
// conserto-noexcept.md sec. "F4", ESCOPO.md Decisao 11, 17/09/2026),
// NOT THROUGH tokenizer.hpp's OWN gltfx_gfss_tokenize(): that
// convenience wrapper grows a std::vector one push_back() at a time,
// which allocates - anb_parse.cpp's own token stream is instead a
// fixed-capacity buffer, fed one token at a time by tokenizer.hpp's
// own gltfx_gfss_next_token() (the EXPORTED, non-allocating
// primitive), sized to this grammar's own measured worst case (that
// file's own top-of-namespace comment on k_max_anb_tokens). The An+B
// grammar's own token-level shapes (a <dimension-token> whose unit is
// "n" or "n-<digits>", a bare <ident-token> "n"/"-n"/"n-<digits>"/
// "-n-<digits>", a signed or signless <number-token>) are still
// exactly the CSS Syntax token classes GFSS-TOKEN already produces -
// there is still no reason for a second, hand-rolled character
// scanner to exist alongside it; only WHICH of tokenizer.hpp's own two
// entry points supplies them changed.
//
// DIAGNOSTIC-SHAPED RESULT, SAME UNRESOLVED TENSION selector_parse.hpp
// AND color_parse.hpp ALREADY NAME (GODS_LAWS.md L-27, marked
// INFERENCE): anb_parse_result below follows the SAME line/column/
// "what was expected" shape as selector_parse_result - a malformed
// An+B argument is a DIAGNOSABLE SYNTAX defect, not the OS/runtime
// failure category gltfx_err's own CE-3 fields are shaped around. An
// argument that needed MORE tokens than this parser's own fixed
// buffer holds is diagnosed the SAME way (k_expected_anb_expression_
// too_long, diagnostic_vocabulary.hpp) - a refusal, never silent
// truncation.
//
// noexcept, AND - SINCE FATIA F4 - HONESTLY SO (GODS_LAWS.md L-44:
// this paragraph used to claim more than the code before it actually
// delivered, and is corrected here rather than left to rot): gfss_anb
// (anb.hpp) is two plain long long fields, no std::vector anywhere in
// the type this function RETURNS - but before this fatia, the LOCAL
// token stream this function built ALONG THE WAY (gltfx_gfss_
// tokenize()'s own std::vector<gltfx_gfss_token>) very much could
// throw std::bad_alloc, from inside a function declared noexcept -
// exactly the defect NOEXCEPT-ALLOC-B8's own varredura found here
// (ESCOPO.md, point B5) and this fatia removed. What is true NOW,
// having actually been checked rather than assumed the way this
// paragraph originally was: nothing on this call's own path allocates
// at all any more (anb_parse.cpp's own k_max_anb_tokens buffer is a
// fixed std::array), so nothing on it can throw - the SAME reasoning
// color_parse.hpp's own parse_color() already gives for its own
// noexcept, now actually true of this function too.

namespace glintfx::style::detail {

// bool ok = false by default, the SAME R4 "empty/zero = never attached"
// convention selector_parse_result/color_parse_result already use - a
// default-constructed result reads back as a FAILURE with an absent
// diagnostic, never a fabricated gfss_anb a caller could mistake for a
// real answer.
struct anb_parse_result {
    bool ok = false;
    gfss_anb value{};
    gltfx_gfss_diagnostic diagnostic{};
};

// Analyzes the WHOLE of `text` as one An+B microsyntax value - the raw
// argument a nth-* functional pseudo-class's own parentheses enclosed,
// e.g. "2n+1", "odd", "-n + 6", "5". Leading and trailing whitespace
// inside `text` (CSS allows it immediately inside a function's own
// parentheses) is skipped; anything left over after a complete An+B
// value - a stray token, unbalanced trailing text - reproves with a
// diagnostic rather than being silently ignored.
[[nodiscard]] anb_parse_result parse_anb(std::string_view text) noexcept;

} // namespace glintfx::style::detail
