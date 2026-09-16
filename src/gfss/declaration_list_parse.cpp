// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_list_parse.hpp"

#include "declaration_parse.hpp"
#include "declaration_split.hpp"
#include "shorthand_expand.hpp"

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

// Appends one ALREADY-EXPANDED longhand to `result`, updating the SAME
// two L-40 counters a directly-authored declaration would (D-DP-2/D-DP-
// 4) - the one place BOTH fold_into_result() below and its own
// shorthand branch push a final declaration, so the counters can never
// drift between the two paths.
void append_declaration(gfss_declaration declaration, declaration_list_parse_result &result) {
    if (declaration.has_reserved_notice) {
        ++result.reserved_notice_count;
    }
    if (!declaration.is_shorthand && declaration.form == gfss_declaration_value_form::raw) {
        ++result.raw_composite_count;
    }
    result.declarations.push_back(std::move(declaration));
}

// Folds ONE parsed declaration into `result` - accepted declarations
// update the two L-40 counters D-DP-2/D-DP-4 require printed, rejected
// ones append their own diagnostic; this is the one place that decides
// which list a declaration_parse_result lands in.
//
// GFSS-SHORTHAND, D-W6-3 (TODO.md wave W6): an accepted SHORTHAND
// (`is_shorthand == true`) is never itself appended to `declarations` -
// expand_shorthand() (shorthand_expand.hpp) replaces it, IN PLACE, with
// its own N longhand declarations (D-W6-3's own "a posicao e preservada
// por construcao": this function is called once per declaration span,
// in source order, so appending the expansion right here, instead of
// the raw shorthand, is what makes the expansion land at the
// shorthand's own original index rather than the front or the back of
// the block). A shorthand that fails to expand contributes its own
// diagnostic to `rejected`, the SAME list a directly-rejected
// declaration already uses - never a declaration of its own.
void fold_into_result(declaration_parse_result parsed, declaration_list_parse_result &result) {
    if (!parsed.accepted) {
        result.rejected.push_back(parsed.diagnostic);
        return;
    }
    if (parsed.declaration.is_shorthand) {
        shorthand_expand_result expanded = expand_shorthand(parsed.declaration);
        if (!expanded.ok) {
            result.rejected.push_back(expanded.diagnostic);
            return;
        }
        for (gfss_declaration &longhand : expanded.longhands) {
            append_declaration(std::move(longhand), result);
        }
        return;
    }
    append_declaration(std::move(parsed.declaration), result);
}

} // namespace

// `start` is a small (32-byte), trivially-copyable value type (the same
// shape gltfx_gfss_cursor's own callers already pass it by in
// tokenizer.hpp), taken BY VALUE on purpose - the plan's own SS3 names
// this exact signature, and the body immediately hands it, by value
// again, to tokenize_from_real_cursor() above; a const reference here
// would only add one extra copy at that call, not remove one.
// cppcheck-suppress passedByValue
declaration_list_parse_result parse_declaration_list(gltfx_gfss_cursor start) {
    const std::vector<gltfx_gfss_token> tokens = tokenize_from_real_cursor(start);

    declaration_list_parse_result result;
    for (const declaration_span &span : split_declarations_at_top_level_semicolons(tokens)) {
        fold_into_result(parse_declaration(tokens, span.begin, span.end), result);
    }
    return result;
}

} // namespace glintfx::style::detail
