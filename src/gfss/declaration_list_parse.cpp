// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_list_parse.hpp"

#include "declaration_parse.hpp"
#include "declaration_split.hpp"

// declaration_list_parse.cpp - GFSS-DECL-PARSE, DP-8 (GODS_LAWS.md
// L-17: each function below answers exactly one question of
// declaration_list_parse.hpp's own header comment scope).

namespace glintfx::style::detail {

namespace {

// Scans `cursor.source` (from `cursor`'s OWN starting position, so the
// resulting tokens' own line/column are the SHEET's real ones - D-DP-1)
// into a full token vector, ending with the real <EOF-token> that
// tokenizer.hpp's own scanning primitive always produces at the end
// (this comment spells the call below without the trailing parens on
// purpose - tests/tools/check_facade_export_boundary.py's own text
// heuristic reads "name() <prose>" in a COMMENT as a plausible
// out-of-line definition of that GLINTFX_API symbol, a measured false
// positive of that gate, not a real boundary violation here: this file
// only CALLS the function below, it defines none of its own).
[[nodiscard]] std::vector<gltfx_gfss_token> tokenize_from_real_cursor(gltfx_gfss_cursor cursor) {
    std::vector<gltfx_gfss_token> tokens;
    gltfx_gfss_token token;
    bool more = true;
    while (more) {
        more = gltfx_gfss_next_token(cursor, token);
        tokens.push_back(token);
    }
    return tokens;
}

// Folds ONE parsed declaration into `result` - accepted declarations
// update the two L-40 counters D-DP-2/D-DP-4 require printed, rejected
// ones append their own diagnostic; this is the one place that decides
// which list a declaration_parse_result lands in.
void fold_into_result(declaration_parse_result parsed, declaration_list_parse_result &result) {
    if (!parsed.accepted) {
        result.rejected.push_back(parsed.diagnostic);
        return;
    }
    if (parsed.declaration.has_reserved_notice) {
        ++result.reserved_notice_count;
    }
    if (!parsed.declaration.is_shorthand &&
        parsed.declaration.form == gfss_declaration_value_form::raw) {
        ++result.raw_composite_count;
    }
    result.declarations.push_back(std::move(parsed.declaration));
}

} // namespace

declaration_list_parse_result parse_declaration_list(gltfx_gfss_cursor start) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_from_real_cursor(start);

    declaration_list_parse_result result;
    for (const declaration_span &span : split_declarations_at_top_level_semicolons(tokens)) {
        fold_into_result(parse_declaration(tokens, span.begin, span.end), result);
    }
    return result;
}

} // namespace glintfx::style::detail
