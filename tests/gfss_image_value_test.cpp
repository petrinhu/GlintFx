// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string_view>
#include <vector>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

#include "gfss/image_value_read.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_image_value_test.cpp - R2D-IMAGE-RESOLVER, fatia R1 (TODO.md; contrato
// docs/auditoria-api-image-resolver.md; plano de fechamento W8 r19, D-W8-373).
//
// The expected values below come from the ORDER's table (the CSS-level reading
// of each input), never from running the reader: the verbatim rule of
// D-W8-373 says `url("a\"b.png")` yields the RAW text `a\"b.png`, not the
// decoded `a"b.png`.
//
// Mutants this file must kill (N1, mutation at the changed lines):
//   M1 quotes not stripped          -> cases 3, 4, 6
//   M2 escape decoded               -> case 5
//   M3 bad_url treated as url       -> case 10
//   M4 gradient treated as error    -> case 12

namespace {

using glintfx::style::gltfx_gfss_tokenize;
using glintfx::style::detail::image_value_kind;
using glintfx::style::detail::read_image_value;

} // namespace

GLINTFX_TEST(image_value_none_is_a_keyword_without_text) {
    const std::string_view source = "none";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::none);
    GLINTFX_CHECK(result.text.empty());
}

GLINTFX_TEST(image_value_none_is_ascii_case_insensitive) {
    const std::string_view source = "NONE";
    const auto tokens = gltfx_gfss_tokenize(source);
    GLINTFX_CHECK(read_image_value(tokens).kind == image_value_kind::none);
}

GLINTFX_TEST(image_value_quoted_url_text_is_inside_the_quotes) {
    const std::string_view source = R"gfss(url("a.png"))gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::url);
    GLINTFX_CHECK(result.text == "a.png");
}

GLINTFX_TEST(image_value_unquoted_url_text_is_inside_the_parentheses) {
    const std::string_view source = "url(a.png)";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::url);
    GLINTFX_CHECK(result.text == "a.png");
}

GLINTFX_TEST(image_value_escape_inside_quoted_url_stays_raw) {
    // D-W8-373: the text is VERBATIM. The backslash is kept; the consumer decodes.
    const std::string_view source = R"gfss(url("a\"b.png"))gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::url);
    GLINTFX_CHECK(result.text == R"gfss(a\"b.png)gfss");
}

GLINTFX_TEST(image_value_text_is_a_view_into_the_source_never_a_copy) {
    // Quoted: `a.png` starts after `url("` (5 bytes). Unquoted: after `url(` (4 bytes).
    const std::string_view quoted = R"gfss(url("a.png"))gfss";
    const auto quoted_tokens = gltfx_gfss_tokenize(quoted);
    const auto quoted_result = read_image_value(quoted_tokens);
    GLINTFX_CHECK(quoted_result.text.data() == quoted.data() + 5);
    GLINTFX_CHECK(quoted_result.text.size() == 5);

    const std::string_view unquoted = "url(a.png)";
    const auto unquoted_tokens = gltfx_gfss_tokenize(unquoted);
    const auto unquoted_result = read_image_value(unquoted_tokens);
    GLINTFX_CHECK(unquoted_result.text.data() == unquoted.data() + 4);
    GLINTFX_CHECK(unquoted_result.text.size() == 5);
}

GLINTFX_TEST(image_value_whitespace_inside_the_url_function_is_not_text) {
    const std::string_view unquoted = "url( a.png )";
    const auto unquoted_tokens = gltfx_gfss_tokenize(unquoted);
    const auto unquoted_result = read_image_value(unquoted_tokens);
    GLINTFX_CHECK(unquoted_result.kind == image_value_kind::url);
    GLINTFX_CHECK(unquoted_result.text == "a.png");

    const std::string_view quoted = R"gfss(url( "a.png" ))gfss";
    const auto quoted_tokens = gltfx_gfss_tokenize(quoted);
    const auto quoted_result = read_image_value(quoted_tokens);
    GLINTFX_CHECK(quoted_result.kind == image_value_kind::url);
    GLINTFX_CHECK(quoted_result.text == "a.png");
}

GLINTFX_TEST(image_value_unclosed_url_is_error_at_the_token_position) {
    const std::string_view source = "url(";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "closing_parenthesis");
    GLINTFX_CHECK(result.diagnostic.line == 1);
    GLINTFX_CHECK(result.diagnostic.column == 1);
}

GLINTFX_TEST(image_value_unclosed_url_reports_its_line_and_column) {
    // The url token starts on line 2, column 3 (two spaces of indent).
    const std::string_view source = "\n  url(a.png";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "closing_parenthesis");
    GLINTFX_CHECK(result.diagnostic.line == 2);
    GLINTFX_CHECK(result.diagnostic.column == 3);
}

GLINTFX_TEST(image_value_bad_url_is_error_and_never_url) {
    // `a"b` is a quote inside an unquoted url: the tokenizer makes a bad_url (M3).
    const std::string_view source = R"gfss(url(a"b))gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "closing_parenthesis");
    GLINTFX_CHECK(result.diagnostic.line == 1);
    GLINTFX_CHECK(result.diagnostic.column == 1);
}

GLINTFX_TEST(image_value_unclosed_string_inside_url_is_error_with_closing_quote) {
    // The string starts at column 5, right after `url(`.
    const std::string_view source = R"gfss(url("abc)gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "closing_quote");
    GLINTFX_CHECK(result.diagnostic.line == 1);
    GLINTFX_CHECK(result.diagnostic.column == 5);
}

GLINTFX_TEST(image_value_linear_gradient_is_not_an_error) {
    // Well-formed, out of THIS slice (M4): not an error, and no text.
    const std::string_view source = "linear-gradient(red, blue)";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::gradient_not_yet_supported);
    GLINTFX_CHECK(result.text.empty());
}

GLINTFX_TEST(image_value_gradient_without_closing_paren_is_error) {
    const std::string_view source = "linear-gradient(red";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "closing_parenthesis");
    GLINTFX_CHECK(result.diagnostic.column == 1);
}

GLINTFX_TEST(image_value_trailing_token_after_url_is_error) {
    // `x` starts at column 14: `url("a.png")` is 12 bytes, then a space.
    const std::string_view source = R"gfss(url("a.png") x)gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "component_value");
    GLINTFX_CHECK(result.diagnostic.column == 14);
}

GLINTFX_TEST(image_value_empty_value_is_error) {
    const std::string_view source = "";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "component_value");
}

GLINTFX_TEST(image_value_plain_identifier_other_than_none_is_error) {
    // Never defaults to `none`: an unknown keyword is an error, not silence.
    const std::string_view source = "auto";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::error);
    GLINTFX_CHECK(result.diagnostic.expected == "component_value");
    GLINTFX_CHECK(result.diagnostic.column == 1);
}

GLINTFX_TEST(image_value_url_function_name_is_ascii_case_insensitive) {
    // CSS function names compare ASCII case-insensitively: `URL(` is `url(` (M7).
    const std::string_view source = R"gfss(URL("a.png"))gfss";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::url);
    GLINTFX_CHECK(result.text == "a.png");
}

GLINTFX_TEST(image_value_gradient_function_name_is_ascii_case_insensitive) {
    // `LINEAR-GRADIENT(` is `linear-gradient(` (M7, the gradient prefix).
    const std::string_view source = "LINEAR-GRADIENT(red, blue)";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::gradient_not_yet_supported);
    GLINTFX_CHECK(result.text.empty());
}

GLINTFX_TEST(image_value_quoted_url_accepts_single_quotes) {
    const std::string_view source = "url('a.png')";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::url);
    GLINTFX_CHECK(result.text == "a.png");
}

GLINTFX_TEST(image_value_gradient_with_a_nested_function_is_one_gradient) {
    // The nested `rgb(` is one level deeper: its `)` must not close the gradient (M9).
    const std::string_view source = "linear-gradient(rgb(1, 2, 3), blue)";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::gradient_not_yet_supported);
}

GLINTFX_TEST(image_value_gradient_with_a_nested_url_is_one_gradient) {
    const std::string_view source = "linear-gradient(url(a.png), red)";
    const auto tokens = gltfx_gfss_tokenize(source);
    const auto result = read_image_value(tokens);
    GLINTFX_CHECK(result.kind == image_value_kind::gradient_not_yet_supported);
}
