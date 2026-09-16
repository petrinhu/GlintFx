// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_list_parse.hpp"

#include "declaration_parse.hpp"
#include "declaration_split.hpp"
#include "shorthand_expand.hpp"
#include "shorthand_reset_notice.hpp"

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
//
// GFSS-SHORTHAND, S-3b (D-W6-13/D-W6-14, docs/plano-w6-folha-de-
// estilo.md, decisao do lider D4, 15/09/2026): AFTER the expansion
// lands, shorthand_reset_notices() (shorthand_reset_notice.hpp) is
// asked, over the SAME `result.declarations` it just grew, which of
// the N just-appended longhands already had an explicit value earlier
// in THIS block - one notice per overridden longhand, appended to
// `result.notices`, the block's own THIRD list (declaration_ast.hpp).
// `shorthand_line`/`shorthand_column` come from the shortcut's OWN
// first raw value token (`parsed.declaration.raw_tokens.front()`) -
// the closest position this crude, pre-expansion declaration carries
// (build_shorthand(), declaration_parse.cpp, never leaves raw_tokens
// empty: parse_declaration() already rejects an empty value span
// before ever calling it). `parsed.declaration` is read here, never
// moved - only `expanded.longhands`' own elements are moved into
// `result`, so raw_tokens stays valid for this read even after the
// append loop below.
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
        const std::size_t expansion_begin = result.declarations.size();
        const std::size_t longhand_count = expanded.longhands.size();
        for (gfss_declaration &longhand : expanded.longhands) {
            append_declaration(std::move(longhand), result);
        }
        const gltfx_gfss_token &shorthand_token = parsed.declaration.raw_tokens.front();
        for (gltfx_gfss_diagnostic &notice :
             shorthand_reset_notices(result.declarations, expansion_begin, longhand_count,
                                     shorthand_token.line, shorthand_token.column)) {
            result.notices.push_back(notice);
            ++result.notice_count;
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
