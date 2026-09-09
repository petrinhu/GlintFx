// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_color_value.hpp"

#include "ascii_case.hpp"
#include "color_parse.hpp"
#include "token_span_trim.hpp"

// declaration_color_value.cpp - GFSS-DECL-PARSE, DP-6 (GODS_LAWS.md
// L-17: each function below answers exactly one question of
// declaration_color_value.hpp's own header comment scope).
// trim_whitespace_tokens() itself now lives in token_span_trim.hpp -
// see that file's own header comment for why (CONTRACT.md SS6.7's
// "regra de tres").

namespace glintfx::style::detail {

namespace {

[[nodiscard]] bool is_current_color_keyword(const gltfx_gfss_token &token) noexcept {
    return token.kind == gltfx_gfss_token_kind::ident &&
           ascii_case_insensitive_equal(token.lexeme, "currentcolor");
}

[[nodiscard]] std::string_view span_text(const std::vector<gltfx_gfss_token> &tokens,
                                         std::size_t first, std::size_t last) noexcept {
    const char *begin_ptr = tokens[first].lexeme.data();
    const char *end_ptr = tokens[last].lexeme.data() + tokens[last].lexeme.size();
    return std::string_view(begin_ptr, static_cast<std::size_t>(end_ptr - begin_ptr));
}

} // namespace

declaration_color_value_result
read_declaration_color_value(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                             std::size_t end) noexcept {
    const trimmed_token_span span = trim_whitespace_tokens(tokens, begin, end);
    if (!span.has_content) {
        return declaration_color_value_result{.kind = declaration_color_value_kind::failed};
    }

    if (span.first == span.last && is_current_color_keyword(tokens[span.first])) {
        return declaration_color_value_result{.kind = declaration_color_value_kind::current_color};
    }

    const color_parse_result parsed = parse_color(span_text(tokens, span.first, span.last));
    if (!parsed.ok) {
        return declaration_color_value_result{.kind = declaration_color_value_kind::failed,
                                              .diagnostic = parsed.diagnostic};
    }
    return declaration_color_value_result{.kind = declaration_color_value_kind::resolved,
                                          .value = parsed.value};
}

} // namespace glintfx::style::detail
