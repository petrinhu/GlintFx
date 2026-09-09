// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfss/tokenizer.hpp>

#include "declaration_ast.hpp"

// declaration_list_parse.hpp - GFSS-DECL-PARSE, DP-8 (TODO.md wave W5,
// GODS_LAWS.md L-17/L-19/L-20/L-27/L-28/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS3/D-DP-1): THE ENTRY POINT
// of this whole fatia - reads a WHOLE block's own miolo (or a style
// attribute's own text - CSS Style Attributes SS3, the SAME function
// serves both, this file's own header comment below) from a REAL
// gltfx_gfss_cursor, tokenizing it, splitting it into declarations
// (declaration_split.hpp), reading each one (declaration_parse.hpp),
// and recovering from a malformed one by DROPPING it and continuing
// with the next (CSS Syntax Module Level 3 SS2.2/SS5.4.5's own
// "discard only the current declaration" - the SAME recovery-by-
// declaration behavior tokenizer.hpp's own header comment already
// documents for tokenization itself, one layer up).
//
// D-DP-1: A REAL CURSOR, NOT A std::string_view (plan SS2): the
// diagnostic's own line/column (token.hpp's own gltfx_gfss_diagnostic,
// project leader's order of 26/08/2026) has to be the SHEET's own real
// position, not relative to the block - `start` is positioned at the
// FIRST byte of the block's own miolo (or the whole style attribute's
// text, for the inline case), with `start.source` already truncated at
// the closing `}` (GFSS-SHEET-PARSE's own job, TODO.md wave W6, to
// extract that span - out of THIS fatia's scope, SS6 of the plan).
//
// NOT noexcept, THE SAME REASON tokenizer.hpp's OWN gltfx_gfss_
// tokenize() AND selector_parse.hpp's OWN parse_selector_list() ARE
// NOT EITHER: every collection here (the token vector, `declarations`,
// `rejected`) is built with push_back - std::bad_alloc is never caught
// on this path.

namespace glintfx::style::detail {

[[nodiscard]] declaration_list_parse_result parse_declaration_list(gltfx_gfss_cursor start);

} // namespace glintfx::style::detail
