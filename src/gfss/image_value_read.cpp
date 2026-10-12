// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gfss/image_value_read.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "gfss/ascii_case.hpp"
#include "gfss/diagnostic_vocabulary.hpp"

// image_value_read.cpp - R2D-IMAGE-RESOLVER, fatia R1 (see the header for the
// contract). Pure: no allocation, no I/O, no OS header (GODS_LAWS.md L-19).
//
// THE SHAPE, IN ORDER (each step can only return an error or go on):
//   1. A token the tokenizer already flagged (bad_url, bad_string, or any token
//      with a diagnostic attached) is THE error. Its own expected identifier is
//      kept, its line and column are the token's.
//   2. The first significant token decides the form: `none` (ident), a url
//      token (unquoted), or a `url(` function followed by a string (quoted).
//      `linear-gradient(` is well formed and returns gradient_not_yet_supported.
//   3. Nothing may follow the value.

namespace glintfx::style::detail {
namespace {

// Walks the tokens of one value, skipping whitespace and the <EOF-token>.
class value_walker {
  public:
    explicit value_walker(std::span<const gltfx_gfss_token> tokens) noexcept : tokens_{tokens} {}

    // The next significant token, or nullptr when the value is over.
    [[nodiscard]] const gltfx_gfss_token *next() noexcept {
        while (position_ < tokens_.size()) {
            const gltfx_gfss_token &candidate = tokens_[position_++];
            if (candidate.kind != gltfx_gfss_token_kind::whitespace &&
                candidate.kind != gltfx_gfss_token_kind::eof) {
                return &candidate;
            }
        }
        return nullptr;
    }

  private:
    std::span<const gltfx_gfss_token> tokens_;
    std::size_t position_ = 0;
};

[[nodiscard]] image_value_result error_result(std::uint32_t line, std::uint32_t column,
                                              std::string_view expected) noexcept {
    image_value_result result;
    result.kind = image_value_kind::error;
    result.diagnostic.line = line;
    result.diagnostic.column = column;
    result.diagnostic.expected = expected;
    return result;
}

// The token breaks the value. The tokenizer's own identifier wins when it
// flagged the token; otherwise the value as a whole was expected.
[[nodiscard]] image_value_result error_at(const gltfx_gfss_token &token) noexcept {
    const std::string_view expected =
        token.diagnostic.expected.empty() ? k_expected_component_value : token.diagnostic.expected;
    return error_result(token.line, token.column, expected);
}

[[nodiscard]] image_value_result url_result(std::string_view text) noexcept {
    image_value_result result;
    result.kind = image_value_kind::url;
    result.text = text;
    return result;
}

[[nodiscard]] image_value_result gradient_result() noexcept {
    image_value_result result;
    result.kind = image_value_kind::gradient_not_yet_supported;
    return result;
}

[[nodiscard]] bool is_flagged(const gltfx_gfss_token &token) noexcept {
    return token.kind == gltfx_gfss_token_kind::bad_url ||
           token.kind == gltfx_gfss_token_kind::bad_string || !token.diagnostic.expected.empty();
}

// Index of the first flagged token, or tokens.size() when none is flagged. An
// index, not a pointer: the caller reads the token back from its own span.
[[nodiscard]] std::size_t first_flagged_index(std::span<const gltfx_gfss_token> tokens) noexcept {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (is_flagged(tokens[index])) {
            return index;
        }
    }
    return tokens.size();
}

// CSS whitespace, the same five code points the tokenizer's own is_whitespace()
// recognizes (U+0020, U+0009, U+000A, U+000D, U+000C).
[[nodiscard]] constexpr bool is_css_whitespace(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
}

[[nodiscard]] std::string_view trim_css_whitespace(std::string_view text) noexcept {
    while (!text.empty() && is_css_whitespace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_css_whitespace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// The lexeme starts with `prefix`, ASCII case-insensitively (`url(` may carry a
// little whitespace after the paren in its lexeme; the prefix check ignores it).
[[nodiscard]] bool starts_with_ascii_ci(std::string_view lexeme, std::string_view prefix) noexcept {
    return lexeme.size() >= prefix.size() && glintfx::style::detail::ascii_case_insensitive_equal(
                                                 lexeme.substr(0, prefix.size()), prefix);
}

// `url(` ... `)` as one token: the text between the paren and the closing paren,
// trimmed. The tokenizer guarantees the closing paren when the token is not flagged.
[[nodiscard]] std::string_view unquoted_url_text(std::string_view lexeme) noexcept {
    constexpr std::string_view prefix = "url(";
    if (lexeme.size() < prefix.size()) {
        return {};
    }
    std::string_view inner = lexeme.substr(prefix.size());
    if (!inner.empty() && inner.back() == ')') {
        inner.remove_suffix(1);
    }
    return trim_css_whitespace(inner);
}

// `"...."` as a string token: the text between the quotes, VERBATIM (D-W8-373).
[[nodiscard]] std::string_view quoted_text(std::string_view lexeme) noexcept {
    if (lexeme.size() < 2) {
        return {};
    }
    return lexeme.substr(1, lexeme.size() - 2);
}

// `url(` then a string then `)`. The string token is already known not to be
// flagged (find_flagged ran first), so only the shape is checked here.
[[nodiscard]] image_value_result read_quoted_url(const gltfx_gfss_token &function_token,
                                                 value_walker &walker) noexcept {
    const gltfx_gfss_token *string_token = walker.next();
    if (string_token == nullptr) {
        return error_result(function_token.line, function_token.column,
                            k_expected_closing_parenthesis);
    }
    if (string_token->kind != gltfx_gfss_token_kind::string) {
        return error_at(*string_token);
    }
    const gltfx_gfss_token *close_token = walker.next();
    if (close_token == nullptr) {
        return error_result(function_token.line, function_token.column,
                            k_expected_closing_parenthesis);
    }
    if (close_token->kind != gltfx_gfss_token_kind::close_paren) {
        return error_at(*close_token);
    }
    return url_result(quoted_text(string_token->lexeme));
}

// A well-formed function this slice does not paint. The rest of the function is
// consumed up to its matching `)`, nested functions and parens counted, so the
// trailing-token check of the caller sees only what follows the gradient.
[[nodiscard]] image_value_result skip_gradient(const gltfx_gfss_token &function_token,
                                               value_walker &walker) noexcept {
    std::size_t depth = 1;
    while (depth > 0) {
        const gltfx_gfss_token *token = walker.next();
        if (token == nullptr) {
            return error_result(function_token.line, function_token.column,
                                k_expected_closing_parenthesis);
        }
        if (token->kind == gltfx_gfss_token_kind::function ||
            token->kind == gltfx_gfss_token_kind::open_paren) {
            ++depth;
        } else if (token->kind == gltfx_gfss_token_kind::close_paren) {
            --depth;
        }
    }
    return gradient_result();
}

[[nodiscard]] image_value_result read_function(const gltfx_gfss_token &function_token,
                                               value_walker &walker) noexcept {
    if (starts_with_ascii_ci(function_token.lexeme, "url(")) {
        return read_quoted_url(function_token, walker);
    }
    if (starts_with_ascii_ci(function_token.lexeme, "linear-gradient(")) {
        return skip_gradient(function_token, walker);
    }
    return error_at(function_token);
}

[[nodiscard]] image_value_result read_first(const gltfx_gfss_token &first,
                                            value_walker &walker) noexcept {
    switch (first.kind) {
    case gltfx_gfss_token_kind::ident:
        if (glintfx::style::detail::ascii_case_insensitive_equal(first.lexeme, "none")) {
            image_value_result result;
            result.kind = image_value_kind::none;
            return result;
        }
        return error_at(first);
    case gltfx_gfss_token_kind::url:
        return url_result(unquoted_url_text(first.lexeme));
    case gltfx_gfss_token_kind::function:
        return read_function(first, walker);
    default:
        return error_at(first);
    }
}

} // namespace

image_value_result read_image_value(std::span<const gltfx_gfss_token> tokens) noexcept {
    const std::size_t flagged = first_flagged_index(tokens);
    if (flagged < tokens.size()) {
        return error_at(tokens[flagged]);
    }
    value_walker walker{tokens};
    const gltfx_gfss_token *first = walker.next();
    if (first == nullptr) {
        // An empty value: located at the <EOF-token> when there is one.
        const std::uint32_t line = tokens.empty() ? 1U : tokens.back().line;
        const std::uint32_t column = tokens.empty() ? 1U : tokens.back().column;
        return error_result(line, column, k_expected_component_value);
    }
    image_value_result result = read_first(*first, walker);
    if (result.kind == image_value_kind::error) {
        return result;
    }
    if (const gltfx_gfss_token *extra = walker.next()) {
        return error_at(*extra);
    }
    return result;
}

} // namespace glintfx::style::detail
