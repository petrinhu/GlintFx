// SPDX-License-Identifier: AGPL-3.0-or-later
#include "important_flag.hpp"

#include <optional>

#include "ascii_case.hpp"
#include "diagnostic_vocabulary.hpp"

// important_flag.cpp - GFSS-DECL-PARSE, DP-3 (GODS_LAWS.md L-17: each
// function below answers exactly one question of important_flag.hpp's
// own header comment scope).

namespace glintfx::style::detail {

namespace {

[[nodiscard]] bool is_bang_delim(const gltfx_gfss_token &token) noexcept {
    return token.kind == gltfx_gfss_token_kind::delim && token.lexeme == "!";
}

[[nodiscard]] bool is_important_ident(const gltfx_gfss_token &token) noexcept {
    return token.kind == gltfx_gfss_token_kind::ident &&
           ascii_case_insensitive_equal(token.lexeme, "important");
}

// The index of the last non-whitespace token in `[begin, end)`, or
// nothing when the span holds none.
[[nodiscard]] std::optional<std::size_t>
last_non_whitespace_index(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                          std::size_t end) noexcept {
    for (std::size_t i = end; i > begin; --i) {
        if (tokens[i - 1].kind != gltfx_gfss_token_kind::whitespace) {
            return i - 1;
        }
    }
    return std::nullopt;
}

// The index of the last non-whitespace token strictly BEFORE `index`,
// within `[begin, index)` - used to walk from "important" back to the
// `!` that has to precede it, and again from the `!` back to whatever
// remains of the value.
[[nodiscard]] std::optional<std::size_t>
previous_non_whitespace_index(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                              std::size_t index) noexcept {
    return last_non_whitespace_index(tokens, begin, index);
}

// The FIRST bang delim token in `[begin, end)`, or nothing - this file's
// own header comment: every malformed shape reports its diagnostic "na
// coluna do !", and the first one found is the deterministic answer
// when more than one is present (`red !important !important`).
[[nodiscard]] std::optional<std::size_t>
first_bang_index(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                 std::size_t end) noexcept {
    for (std::size_t i = begin; i < end; ++i) {
        if (is_bang_delim(tokens[i])) {
            return i;
        }
    }
    return std::nullopt;
}

important_flag_result malformed_at(const std::vector<gltfx_gfss_token> &tokens,
                                   std::size_t bang_index, std::size_t end) noexcept {
    const gltfx_gfss_token &bang = tokens[bang_index];
    return important_flag_result{
        .ok = false,
        .important = false,
        .value_end = end,
        .diagnostic = gltfx_gfss_diagnostic{.line = bang.line,
                                            .column = bang.column,
                                            .expected = k_expected_important_flag_at_end_of_value,
                                            .detail = {}}};
}

} // namespace

important_flag_result read_important_flag(const std::vector<gltfx_gfss_token> &tokens,
                                          std::size_t begin, std::size_t end) noexcept {
    const std::optional<std::size_t> any_bang = first_bang_index(tokens, begin, end);
    if (!any_bang.has_value()) {
        // No `!` anywhere in the span - an ordinary value, no flag.
        return important_flag_result{
            .ok = true, .important = false, .value_end = end, .diagnostic = {}};
    }

    const std::optional<std::size_t> last = last_non_whitespace_index(tokens, begin, end);
    if (!last.has_value() || !is_important_ident(tokens[*last])) {
        // A `!` exists, but the value does not even END in an
        // "important" ident - never a valid trailing pair.
        return malformed_at(tokens, *any_bang, end);
    }

    const std::optional<std::size_t> before_important =
        previous_non_whitespace_index(tokens, begin, *last);
    if (!before_important.has_value() || !is_bang_delim(tokens[*before_important])) {
        // Ends in "important", but nothing (or something other than
        // `!`) precedes it - e.g. a bare property value that happens to
        // spell the word "important" with an unrelated `!` earlier.
        return malformed_at(tokens, *any_bang, end);
    }

    // A well-formed trailing pair was found - `[begin, *before_important)`
    // is what remains of the value.
    const std::size_t remainder_end = *before_important;
    const std::optional<std::size_t> leftover_bang = first_bang_index(tokens, begin, remainder_end);
    if (leftover_bang.has_value()) {
        // A SECOND `!` survives inside what is left after stripping the
        // trailing pair (`red !important !important`) - this file's own
        // header comment: "sobrou ! no valor depois de tirar o par
        // final".
        return malformed_at(tokens, *leftover_bang, end);
    }

    if (!last_non_whitespace_index(tokens, begin, remainder_end).has_value()) {
        // Stripping the pair leaves nothing behind (`!important` alone)
        // - this file's own header comment, the one declared inference.
        return malformed_at(tokens, *before_important, end);
    }

    return important_flag_result{
        .ok = true, .important = true, .value_end = remainder_end, .diagnostic = {}};
}

} // namespace glintfx::style::detail
