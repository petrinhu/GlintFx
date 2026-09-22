// SPDX-License-Identifier: AGPL-3.0-or-later
#include "sheet_parse.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/gfss/tokenizer.hpp>

#include "at_rule_vocabulary.hpp"
#include "declaration_list_parse.hpp"
#include "diagnostic_vocabulary.hpp"
#include "nesting_depth.hpp"
#include "selector_parse.hpp"
#include "token_span_trim.hpp"

// sheet_parse.cpp - GFSS-SHEET-PARSE (TODO.md, GODS_LAWS.md L-17/L-20/
// L-27/L-28/L-40): the algorithm behind sheet_parse.hpp's own
// parse_sheet() - see that file's own header comment for the recovery
// policy and at-rule dispatch this implementation follows.
//
// TOKEN-STREAM SCANNING, NEVER RAW-BYTE SCANNING (the SAME "layered on
// top of GFSS-TOKEN's own token stream" relationship selector_parse.cpp's
// own header comment already documents for GFSS-SEL-PARSE-CORE): the
// whole sheet is tokenized ONCE, up front, into one flat
// std::vector<gltfx_gfss_token> - this is what makes "a `{` inside a
// quoted string never counts as a real brace" TRUE FOR FREE (community
// dossier item 4.6, ESCOPO.md's own onda-W6 answer to it): a string's
// own `{` character is already swallowed, whole, into that ONE token's
// own `.lexeme` by the tokenizer, and never surfaces as a separate
// `open_curly` token this file's own loop could mistake for a real one.
//
// TWO DIFFERENT BRACE QUESTIONS, TWO DIFFERENT SCANS, ON PURPOSE (GODS_
// LAWS.md L-17, "a frase sem e"): find_prelude_end() below answers
// "where does a selector's or an at-rule's own PRELUDE stop" - the
// FIRST unnested `{`/`;` (a prelude cannot legitimately contain either
// one un-nested inside itself, in this grammar, so no depth tracking is
// needed for this question at all). find_matching_close_curly() answers
// a DIFFERENT question - "given an ALREADY-OPEN `{`, where is the `}`
// that closes IT" - which DOES need nesting_depth.hpp's own delta (a
// `@keyframes` block legitimately nests its own `{ }` pairs per quadro,
// and a hostile leaf's declaration value could nest brackets a real one
// never would, LEI ZERO's own "base de consumidores aberta e
// desconhecida"). Conflating the two into one function would make it
// answer both questions at once, exactly what L-17's own "a unidade
// paga por duas leis" smell describes.
//
// "NO MORE SHEET TO TRUST" IS THE ONLY FATAL CONDITION (GODS_LAWS.md
// L-40's own "portao que nao olha e pior que ausente" - never GUESSING
// where a construct would have ended): an unclosed `{` - a style rule's
// own block, or an at-rule's own block - stops the WHOLE sheet scan,
// with one diagnostic, exactly the community dossier's own Firefox
// failure mode (item 4.5) recreated ON PURPOSE, refused instead of
// silently swallowed. EVERY OTHER malformed construct this file
// recognizes (a bad selector, an unnamed `@keyframes`, an at-rule with
// no block where one was expected, an unknown at-rule, a stray `}`)
// recovers and keeps reading - see each branch below for its own
// resync point.
//
// SCOPE OF THIS FATIA'S OWN CUT: see sheet_ast.hpp's own header comment.

namespace glintfx::style::detail {

namespace {

using token_vector = std::vector<gltfx_gfss_token>;

// `tokens[a]`/`tokens[b]` are ADJACENT in the ORIGINAL sheet buffer iff
// `a`'s own lexeme ends exactly where `b`'s own begins - the SAME
// byte-pointer-arithmetic technique selector_parse.cpp's own
// tokens_are_adjacent() already establishes, reused here (not
// re-derived) for the SAME reason: a token vector built by ONE
// gltfx_gfss_tokenize() call over the WHOLE sheet, never re-tokenized
// per rule, is what lets a byte SPAN be recovered from two token
// INDEXES by plain pointer subtraction.
[[nodiscard]] std::string_view span_between(const gltfx_gfss_token &begin_after,
                                            const gltfx_gfss_token &end_before) noexcept {
    const char *begin = begin_after.lexeme.data() + begin_after.lexeme.size();
    const char *end = end_before.lexeme.data();
    return std::string_view(begin, static_cast<std::size_t>(end - begin));
}

// An at-keyword's own lexeme always starts with the '@' that opened it
// (tokenizer.hpp's own consume_at_keyword_or_delim()) - this strips
// exactly that one leading byte, the mirror image of selector_parse.cpp's
// own function_name() (which strips a trailing '(' instead).
[[nodiscard]] std::string_view at_rule_name(std::string_view lexeme) noexcept {
    return lexeme.substr(1);
}

// Advances `index` past every consecutive whitespace/cdo/cdc token
// starting there - the SAME "gap between real content" skip selector_
// parse.cpp's own skip_whitespace() performs for a selector's own
// tokens, extended here with `cdo`/`cdc` (CSS Syntax Module Level 3's
// own `<!--`/`-->` tokens, legal ONLY between top-level rules/at-rules
// in a real stylesheet, never inside one) because THIS is the
// stylesheet's own top level, the one place those two token kinds are
// meaningful noise rather than a syntax error.
void skip_insignificant_top_level_tokens(const token_vector &tokens, std::size_t &index) noexcept {
    for (;;) {
        const gltfx_gfss_token_kind kind = tokens[index].kind;
        if (kind != gltfx_gfss_token_kind::whitespace && kind != gltfx_gfss_token_kind::cdo &&
            kind != gltfx_gfss_token_kind::cdc) {
            return;
        }
        ++index;
    }
}

struct prelude_boundary {
    bool found_open_curly = false; // true: stopped at '{'; false: stopped at ';' (has_semicolon)
    bool has_semicolon = false;
    std::size_t terminator_index = 0; // index of the '{', ';', '}', or eof token that stopped this
};

// Scans forward from `index` for the FIRST `{`, `;`, stray `}`, or eof -
// see this file's own header comment above for why NO depth tracking is
// needed for this particular question (a selector's or an at-rule's own
// prelude cannot legitimately contain any of the three, un-nested, in
// this grammar). `found_open_curly`/`has_semicolon` are both false when
// the scan stopped at a stray `}` or eof instead - the caller's own job
// to turn THAT into a diagnosis (this function only locates, never
// judges).
[[nodiscard]] prelude_boundary find_prelude_end(const token_vector &tokens,
                                                std::size_t index) noexcept {
    for (std::size_t idx = index; idx < tokens.size(); ++idx) {
        const gltfx_gfss_token_kind kind = tokens[idx].kind;
        if (kind == gltfx_gfss_token_kind::open_curly) {
            return {.found_open_curly = true, .has_semicolon = false, .terminator_index = idx};
        }
        if (kind == gltfx_gfss_token_kind::semicolon) {
            return {.found_open_curly = false, .has_semicolon = true, .terminator_index = idx};
        }
        if (kind == gltfx_gfss_token_kind::close_curly || kind == gltfx_gfss_token_kind::eof) {
            return {.found_open_curly = false, .has_semicolon = false, .terminator_index = idx};
        }
    }
    return {
        .found_open_curly = false, .has_semicolon = false, .terminator_index = tokens.size() - 1};
}

struct close_curly_search {
    bool ok = false;
    std::size_t close_index = 0; // valid iff ok
};

// `tokens[open_index]` is an `open_curly` - finds the `close_curly` that
// closes IT, counting every nested `{`/`(`/`[` this block's own body
// opens along the way (nesting_depth.hpp's own delta, reused rather than
// re-derived - CONTRACT.md SS6.7's own "regra de tres" is long since
// satisfied by declaration_split.cpp/declaration_value_check.cpp; this
// is simply ANOTHER real call site, the SAME reuse selector_pseudo_
// vocabulary.hpp's own header comment already argues for
// ascii_case_insensitive_equal()). Never recurses - a flat loop with an
// integer depth counter, the SAME anti-DoS shape selector_parse.cpp's
// own capture_functional_argument() already establishes for the
// identical reason (a hostile leaf's own arbitrarily deep nesting must
// never grow the call stack, LEI ZERO). `ok == false` means EOF was
// reached first - "no more sheet to trust" (this file's own header
// comment).
[[nodiscard]] close_curly_search find_matching_close_curly(const token_vector &tokens,
                                                           std::size_t open_index) noexcept {
    int depth = 1;
    for (std::size_t idx = open_index + 1; idx < tokens.size(); ++idx) {
        if (tokens[idx].kind == gltfx_gfss_token_kind::eof) {
            break;
        }
        depth += nesting_depth_delta(tokens[idx].kind);
        if (depth == 0) {
            return {.ok = true, .close_index = idx};
        }
    }
    return {.ok = false, .close_index = 0};
}

// A selector diagnostic parse_selector_list() returns is relative to the
// STANDALONE slice it was handed (its own gltfx_gfss_tokenize() call
// starts a FRESH cursor at line 1, column 1 for that slice alone,
// selector_parse.cpp's own parse_selector_list_impl()) - never the
// sheet's own real position. This corrects it back to the sheet's own
// coordinates, given the slice's own real starting line/column (the
// SAME "the sheet's own real position, not relative to the block"
// requirement declaration_list_parse.hpp's own header comment already
// states for D-DP-1, applied here to the OTHER parser this file also
// slices text for). A diagnostic on the slice's own first line shifts
// BOTH line and column (the slice's own column-1 numbering starts where
// the real sheet's `slice_start_column` does); a diagnostic on a LATER
// line only shifts the line number - a newline inside the slice is the
// SAME newline in the real sheet, so a later line's own column already
// counts correctly from ITS OWN start in both coordinate systems.
[[nodiscard]] gltfx_gfss_diagnostic
offset_diagnostic_to_sheet_position(gltfx_gfss_diagnostic diagnostic,
                                    std::uint32_t slice_start_line,
                                    std::uint32_t slice_start_column) noexcept {
    if (diagnostic.line == 1) {
        diagnostic.column = slice_start_column + (diagnostic.column - 1);
    }
    diagnostic.line = slice_start_line + (diagnostic.line - 1);
    return diagnostic;
}

[[nodiscard]] gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &at,
                                                    std::string_view expected,
                                                    std::string_view detail = {}) noexcept {
    return gltfx_gfss_diagnostic{
        .line = at.line, .column = at.column, .expected = expected, .detail = detail};
}

// The space-separated vocabulary at_rule_vocabulary.hpp's own
// k_at_rule_names carries, joined ONCE - `supported_at_rule`'s own
// `detail` (docs/api-conventions.md R7: space-separated identifiers,
// never a sentence). A `static` local, not a namespace-scope constant:
// std::string owns a heap buffer, and every OTHER constant in this
// track's own vocabulary headers is a constexpr std::string_view
// specifically to avoid static-init-order concerns (selector_pseudo_
// vocabulary.hpp's own header comment on why its own lists take a bare
// string literal) - building this ONE joined string lazily, on first
// use, inside the one function that needs it, sidesteps that question
// entirely rather than reopening it.
[[nodiscard]] std::string_view supported_at_rule_names_joined() {
    static const std::string joined = [] {
        std::string text;
        for (std::size_t i = 0; i < k_at_rule_names.size(); ++i) {
            if (i != 0) {
                text += ' ';
            }
            text += k_at_rule_names[i];
        }
        return text;
    }();
    return joined;
}

// Builds the gltfx_gfss_cursor GFSS-DECL-PARSE's own parse_declaration_
// list() requires (D-DP-1: "a REAL cursor, not a std::string_view" - the
// diagnostic's own line/column has to be the SHEET's own real position).
// `open_curly`/`close_curly` are the block's own bracketing tokens
// (already located by find_matching_close_curly() above); when the
// block is EMPTY (nothing between them), the cursor's own line/column
// falls back to the `close_curly` token's own position - there is
// nothing inside for a diagnostic to ever point at either way.
[[nodiscard]] gltfx_gfss_cursor declaration_block_cursor(const token_vector &tokens,
                                                         std::size_t open_curly_index,
                                                         std::size_t close_curly_index) noexcept {
    const gltfx_gfss_token &open_curly = tokens[open_curly_index];
    const gltfx_gfss_token &close_curly = tokens[close_curly_index];
    const std::string_view miolo = span_between(open_curly, close_curly);
    const std::size_t first_inner_index = open_curly_index + 1;
    const gltfx_gfss_token &position_source =
        first_inner_index < close_curly_index ? tokens[first_inner_index] : close_curly;
    return gltfx_gfss_cursor{.source = miolo,
                             .byte_offset = 0,
                             .line = position_source.line,
                             .column = position_source.column};
}

// One at-rule handled - `tokens[index]` is the `at_keyword` token
// itself. Advances `index` past the whole at-rule (prelude + block, if
// any) on every non-fatal outcome; returns false ONLY when the block
// never closes (this file's own one fatal condition), leaving `index`
// unmoved so the caller can report exactly where the unclosed `{` was.
[[nodiscard]] bool handle_at_rule(const token_vector &tokens, std::size_t &index,
                                  gfss_sheet_parse_result &result) {
    const gltfx_gfss_token &at_token = tokens[index];
    const std::string_view name = at_rule_name(at_token.lexeme);
    const std::size_t prelude_start = index + 1;
    const prelude_boundary boundary = find_prelude_end(tokens, prelude_start);
    const std::optional<at_rule_verdict> verdict = at_rule_verdict_for(name);

    if (!boundary.found_open_curly && !boundary.has_semicolon) {
        // Stopped at a stray '}' or eof - this at-rule's own prelude
        // never reaches a block or a ';'. The SAME "no more sheet to
        // trust" stop this file's own header comment names for an
        // unclosed brace - there is no reliable resync point past
        // trailing garbage that is not even well-formed enough to end.
        result.rejected.push_back(
            make_diagnostic(at_token, k_expected_opening_curly_brace_after_at_rule));
        ++result.swept_count;
        return false;
    }

    if (boundary.has_semicolon) {
        // No block. `keyframes` REQUIRES one (at_rule_vocabulary.hpp's
        // own `raw` verdict always carries a body) - a semicolon instead
        // is malformed, recorded and skipped, never fatal (';' is a
        // reliable resync point).
        if (verdict.has_value() && *verdict == at_rule_verdict::raw) {
            result.rejected.push_back(
                make_diagnostic(at_token, k_expected_opening_curly_brace_after_at_rule));
        } else if (verdict.has_value()) {
            result.ignored_at_rules.push_back(
                make_diagnostic(at_token, k_expected_at_rule_media_is_outside_v1));
        } else {
            result.ignored_at_rules.push_back(make_diagnostic(
                at_token, k_expected_supported_at_rule, supported_at_rule_names_joined()));
        }
        index = boundary.terminator_index + 1;
        ++result.swept_count;
        return true;
    }

    // Has a block: locate its own matching close.
    const std::size_t open_curly_index = boundary.terminator_index;
    const close_curly_search close = find_matching_close_curly(tokens, open_curly_index);
    if (!close.ok) {
        result.rejected.push_back(
            make_diagnostic(tokens[open_curly_index], k_expected_closing_curly_brace));
        ++result.swept_count;
        return false;
    }

    if (!verdict.has_value()) {
        result.ignored_at_rules.push_back(make_diagnostic(at_token, k_expected_supported_at_rule,
                                                          supported_at_rule_names_joined()));
        index = close.close_index + 1;
        ++result.swept_count;
        return true;
    }
    if (*verdict == at_rule_verdict::ignored_with_diagnostic) {
        result.ignored_at_rules.push_back(
            make_diagnostic(at_token, k_expected_at_rule_media_is_outside_v1));
        index = close.close_index + 1;
        ++result.swept_count;
        return true;
    }

    // *verdict == at_rule_verdict::raw ("keyframes"): the prelude
    // (untrimmed, for gfss_raw_block::prelude's own field) is the span
    // between the at-keyword and the '{'; the TRIMMED prelude is what
    // decides whether a name was actually given.
    const std::string_view prelude = span_between(at_token, tokens[open_curly_index]);
    const trimmed_token_span trimmed_name =
        trim_whitespace_tokens(tokens, prelude_start, open_curly_index);
    if (!trimmed_name.has_content) {
        result.rejected.push_back(make_diagnostic(at_token, k_expected_keyframes_name));
        index = close.close_index + 1;
        ++result.swept_count;
        return true;
    }
    // trimmed_name.first is always >= prelude_start >= 1 (index 0 is
    // always the at-keyword itself), so first - 1 never underflows.
    // span_between()'s own [begin_after.end, end_before.start) shape,
    // applied to the tokens immediately BEFORE and AFTER the trimmed
    // range, recovers the trimmed range's own bytes - every token in
    // between is byte-adjacent to its neighbor (the whole sheet was
    // tokenized in ONE pass), so this is the SAME "recover a span from
    // two indexes" technique this file's own span_between() already
    // performs, applied one token wider on each side.
    const std::string_view name_text =
        span_between(tokens[trimmed_name.first - 1], tokens[trimmed_name.last + 1]);
    const std::string_view raw_body =
        span_between(tokens[open_curly_index], tokens[close.close_index]);
    result.raw_blocks.push_back(gfss_raw_block{.name = name_text,
                                               .prelude = prelude,
                                               .raw_body = raw_body,
                                               .line = at_token.line,
                                               .column = at_token.column});
    index = close.close_index + 1;
    ++result.swept_count;
    return true;
}

// One style rule handled - `tokens[index]` is the FIRST token of its
// own selector prelude. Advances `index` past the whole rule (selector +
// block) on every non-fatal outcome; returns false ONLY when the block
// never closes, the SAME fatal-condition contract handle_at_rule() above
// follows.
[[nodiscard]] bool handle_style_rule(const token_vector &tokens, std::size_t &index,
                                     gfss_sheet_parse_result &result) {
    const std::size_t prelude_start = index;
    const prelude_boundary boundary = find_prelude_end(tokens, prelude_start);
    if (!boundary.found_open_curly) {
        // Stopped at a stray '}', ';', or eof - no reliable resync point
        // past trailing garbage that never reaches a real block open.
        result.rejected.push_back(
            make_diagnostic(tokens[prelude_start], k_expected_opening_curly_brace_after_selector));
        ++result.swept_count;
        return false;
    }
    const std::size_t open_curly_index = boundary.terminator_index;
    const close_curly_search close = find_matching_close_curly(tokens, open_curly_index);
    if (!close.ok) {
        result.rejected.push_back(
            make_diagnostic(tokens[open_curly_index], k_expected_closing_curly_brace));
        ++result.swept_count;
        return false;
    }

    // The prelude's own raw bytes, from the FIRST byte of the prelude's
    // first token (never "after" it - unlike span_between()'s own
    // convention, which is built for spans BETWEEN two tokens, this
    // slice's own left edge IS the first token's own first byte) to the
    // '{' token's own first byte.
    const gltfx_gfss_token &prelude_first_token = tokens[prelude_start];
    const char *selector_begin = prelude_first_token.lexeme.data();
    const char *selector_end = tokens[open_curly_index].lexeme.data();
    const std::string_view selector_text(selector_begin,
                                         static_cast<std::size_t>(selector_end - selector_begin));

    const selector_parse_result parsed = parse_selector_list(selector_text);
    if (!parsed.ok) {
        result.rejected.push_back(offset_diagnostic_to_sheet_position(
            parsed.diagnostic, prelude_first_token.line, prelude_first_token.column));
        index = close.close_index + 1;
        ++result.swept_count;
        return true;
    }

    const gltfx_gfss_cursor cursor =
        declaration_block_cursor(tokens, open_curly_index, close.close_index);
    declaration_list_parse_result declarations = parse_declaration_list(cursor);

    gfss_style_rule rule{.selectors = parsed.value,
                         .declarations = std::move(declarations),
                         .source_order = result.rules.size(),
                         .line = tokens[open_curly_index].line,
                         .column = tokens[open_curly_index].column};
    result.rules.push_back(std::move(rule));
    index = close.close_index + 1;
    ++result.swept_count;
    return true;
}

} // namespace

gfss_sheet_parse_result parse_sheet(std::string_view source) {
    const token_vector tokens = gltfx_gfss_tokenize(source);
    gfss_sheet_parse_result result;
    std::size_t index = 0;

    for (;;) {
        skip_insignificant_top_level_tokens(tokens, index);
        const gltfx_gfss_token &tok = tokens[index];
        if (tok.kind == gltfx_gfss_token_kind::eof) {
            break;
        }
        if (tok.kind == gltfx_gfss_token_kind::close_curly) {
            // A stray '}' at the top level: a reliable resync point (the
            // NEXT token is exactly where a new rule or at-rule could
            // legitimately start), so this recovers, never stops the
            // sheet.
            result.rejected.push_back(make_diagnostic(tok, k_expected_style_rule_or_at_rule));
            ++index;
            ++result.swept_count;
            continue;
        }
        if (tok.kind == gltfx_gfss_token_kind::at_keyword) {
            if (!handle_at_rule(tokens, index, result)) {
                break;
            }
            continue;
        }
        if (!handle_style_rule(tokens, index, result)) {
            break;
        }
    }

    return result;
}

} // namespace glintfx::style::detail
