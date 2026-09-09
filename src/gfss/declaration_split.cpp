// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_split.hpp"

#include "nesting_depth.hpp"

// declaration_split.cpp - GFSS-DECL-PARSE, DP-2 (GODS_LAWS.md L-17:
// each function below answers exactly one question of this file's own
// header comment scope). nesting_depth_delta() itself now lives in
// nesting_depth.hpp - see that file's own header comment for why
// (CONTRACT.md SS6.7's "regra de tres").

namespace glintfx::style::detail {

namespace {

// Whether `tokens[begin, end)` holds at least one non-whitespace token
// - the test this file's own header comment names for dropping an
// empty span, never producing one.
[[nodiscard]] bool span_has_content(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                                    std::size_t end) noexcept {
    for (std::size_t i = begin; i < end; ++i) {
        if (tokens[i].kind != gltfx_gfss_token_kind::whitespace) {
            return true;
        }
    }
    return false;
}

// Appends `[begin, end)` to `spans` - but only when it holds content
// (span_has_content() above); this is the ONE place that decides
// whether a candidate boundary becomes a real declaration_span, so the
// caller never has to repeat the check.
void append_span_if_non_empty(std::vector<declaration_span> &spans,
                              const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                              std::size_t end) noexcept {
    if (span_has_content(tokens, begin, end)) {
        spans.push_back(declaration_span{.begin = begin, .end = end});
    }
}

} // namespace

std::vector<declaration_span>
split_declarations_at_top_level_semicolons(const std::vector<gltfx_gfss_token> &tokens) {
    std::vector<declaration_span> spans;
    std::size_t start = 0;
    int depth = 0;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const gltfx_gfss_token_kind kind = tokens[i].kind;

        // The trailing <EOF-token> gltfx_gfss_tokenize()'s own
        // convention always appends - whatever is still open (even a
        // `{` never closed, depth > 0) ends HERE, never scanned past
        // the vector's own bound (plan SS7 risco 3: "o fatiador
        // consome um token por iteracao ... `{` sem fecho termina no
        // EOF do vetor").
        if (kind == gltfx_gfss_token_kind::eof) {
            append_span_if_non_empty(spans, tokens, start, i);
            break;
        }

        if (kind == gltfx_gfss_token_kind::semicolon && depth == 0) {
            append_span_if_non_empty(spans, tokens, start, i);
            start = i + 1;
            continue;
        }

        // MEASURED DEFECT, not a preventive guess: the first version of
        // this loop had no clamp, and the DP-8 directed case for a
        // stray `}` alone (this file's own test, "invalid_declaration_
        // is_dropped_and_the_next_one_survives") went RED the moment it
        // was added - the recovery this whole function exists for broke
        // on exactly the input it is supposed to recover from. A stray
        // closing token (`}`/`)`/`]` with no matching open) drove
        // `depth` NEGATIVE, and every `;` for the REST of the block then
        // looked "nested" (`depth != 0`) and stopped being recognized as
        // a boundary - one bare `}` silently swallowed every declaration
        // after it into a single span. Clamp at zero: a closing token
        // nobody opened contributes nothing, it does not go on to cancel
        // out a REAL opening token later. Sabotaged and restored against
        // a committed SHA to prove the fix (GODS_LAWS.md L-27) - see
        // this fatia's own delivery report for the exact cycle.
        depth += nesting_depth_delta(kind);
        if (depth < 0) {
            depth = 0;
        }
    }

    return spans;
}

} // namespace glintfx::style::detail
