// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <glintfx/gfss/token.hpp>

#include "declaration_ast.hpp"
#include "selector_ast.hpp"

// sheet_ast.hpp - GFSS-SHEET-PARSE (TODO.md, GODS_LAWS.md L-17/L-19/
// L-20/L-27/L-40): the shape a WHOLE parsed sheet takes - answers
// exactly one question, "what does reading a whole gfss style sheet
// produce", never HOW it got there (sheet_parse.hpp/.cpp) nor what an
// individual at-rule NAME means (at_rule_vocabulary.hpp).
//
// INTERNAL, ON PURPOSE (GODS_LAWS.md L-19: "o header nasce interno, em
// src/gfss/"): the SAME public-type-elsewhere/internal-parser-here split
// declaration_ast.hpp/selector_ast.hpp already establish for this track.
// GFSS-API (TODO.md, wave W10) decides the public shape - nothing here
// is GLINTFX_API, and this type is NOT yet ABI-frozen.
//
// THREE LISTS, NEVER FOLDED INTO ONE (the SAME "a consumer never has to
// guess which field is meaningful" principle declaration_ast.hpp's own
// header comment already states for gfss_declaration_value_form, GODS_
// LAWS.md L-27): a style rule, a raw block (today, only `@keyframes` -
// at_rule_vocabulary.hpp's own `raw` verdict), and a rejection diagnostic
// are three DIFFERENT things a top-level construct of a sheet can turn
// into - cramming a rejected rule into `rules` with a sentinel, or a
// `@keyframes` block into `rules` with an empty selector list, would
// force every future reader (GFSS-CASCADE, ANIM-KEYFRAMES) to first
// figure out which sentinel means what, the exact confusion keeping
// them apart avoids. `ignored_at_rules` is its OWN fourth list, distinct
// from `rejected`: an ignored at-rule (`@media`, or one this v1 does not
// recognize at all) is NOT a parse failure - the SHEET is well-formed,
// the construct is simply out of this v1's scope, and GODS_LAWS.md L-40
// still wants it counted and named, never silently dropped.
//
// SCOPE OF THIS FATIA'S OWN CUT (narrower than the full S-5 the CTO's
// plan at docs/plano-w6-folha-de-estilo.md proposes, TODO.md's own
// GFSS-SHEET-PARSE row text, 22/09/2026 - GODS_LAWS.md L-18, this is
// FACT read from the row, not this implementer's own inference): a
// `gfss_style_rule` below carries its selectors and its full declaration
// block, source order, and position - it does NOT carry a per-selector
// specificity (GFSS-SPECIFICITY's own future job to attach, still 🔍
// pendente verificação as of this fatia), and this file does not
// implement the dead-selector check (`:scope` + sibling combinator) or
// `:where()` integration the fuller plan also describes - those belong
// to sibling fatias not yet closed. Nothing here blocks either from
// being added later: `gfss_style_rule` is a plain aggregate, and a field
// appended to it is not a breaking change before GFSS-API freezes it.

namespace glintfx::style::detail {

// One style rule this fatia's own recovery-by-rule accepted: its own
// selector list (already validated by parse_selector_list(),
// selector_parse.hpp), its own declaration block (already validated by
// parse_declaration_list(), declaration_list_parse.hpp - carrying ITS
// OWN `rejected`/`notices` sub-lists for a malformed/reset-warned
// declaration inside an otherwise-good rule), the rule's own position in
// SOURCE ORDER (0-based, gaps never introduced by a rejected rule in
// between - sheet_parse.cpp's own header comment on why), and the
// sheet's own real line/column of the rule's opening `{` (never the
// selector text's OWN relative position - the same "the sheet's real
// position, not relative to the block" discipline declaration_list_
// parse.hpp's own header comment already requires of D-DP-1).
struct gfss_style_rule {
    gfss_selector_list selectors;
    declaration_list_parse_result declarations;
    std::size_t source_order = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

// One at-rule block at_rule_vocabulary.hpp's own `raw` verdict applies
// to (today, only `@keyframes`) - captured BYTE FOR BYTE, unanalyzed,
// for a later fatia (ANIM-KEYFRAMES, TODO.md wave W10) to read. `name`
// is the at-rule's own vocabulary word ("keyframes"), never the
// author's own prelude text; `prelude` is the raw text BETWEEN the
// at-keyword and the opening `{` (for `@keyframes spin { ... }`, the
// author's own animation name, "spin", untrimmed - sheet_parse.cpp's
// own header comment on why trimming is this struct's caller's job, not
// this struct's); `raw_body` is the exact byte span between the
// matching `{`/`}` pair, including any leading/trailing whitespace and
// every nested `{`/`}` inside it (rule_boundary-style nesting counting,
// sheet_parse.cpp's own header comment) - `memcmp`-identical to the
// sheet's own source bytes, never copied, trimmed, or re-tokenized here.
struct gfss_raw_block {
    std::string_view name;
    std::string_view prelude;
    std::string_view raw_body;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

// The whole sheet's own read. `swept_count` (GODS_LAWS.md L-40: "a
// contagem aparece na saida, mesmo quando passa") is the total number of
// top-level constructs this pass actually visited - every accepted rule,
// every raw block, every rejected construct, and every ignored at-rule,
// ALL counted here so a caller (or a test) never has to re-derive it by
// summing four vectors and risking one of them being the one that grew
// without the sum following - sheet_parse.cpp's own single fold point is
// the one place that increments this, the SAME "one place decides"
// discipline declaration_list_parse.cpp's own append_declaration()
// already follows for ITS OWN two counters.
struct gfss_sheet_parse_result {
    std::vector<gfss_style_rule> rules;
    std::vector<gfss_raw_block> raw_blocks;
    std::vector<gltfx_gfss_diagnostic> rejected;
    std::vector<gltfx_gfss_diagnostic> ignored_at_rules;
    std::size_t swept_count = 0;
};

} // namespace glintfx::style::detail
