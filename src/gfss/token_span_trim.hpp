// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

#include <glintfx/gfss/token.hpp>

// token_span_trim.hpp - GFSS-DECL-PARSE (TODO.md wave W5, GODS_LAWS.md
// L-17/L-33/L-40): the ONE "trim leading/trailing whitespace tokens off
// a `[begin, end)` span" helper - CONTRACT.md SS6.7's own "regra de
// tres" (declaration_color_value.cpp, declaration_value_check.cpp and
// declaration_parse.cpp all needed the identical trim, independently,
// within the same fatia) is what earns it a file of its own instead of
// staying triplicated.

namespace glintfx::style::detail {

struct trimmed_token_span {
    bool has_content = false;
    std::size_t first = 0; // inclusive, valid iff has_content
    std::size_t last = 0;  // inclusive, valid iff has_content
};

[[nodiscard]] constexpr trimmed_token_span
trim_whitespace_tokens(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                       std::size_t end) noexcept {
    std::size_t first = begin;
    while (first < end && tokens[first].kind == gltfx_gfss_token_kind::whitespace) {
        ++first;
    }
    if (first == end) {
        return trimmed_token_span{.has_content = false, .first = 0, .last = 0};
    }
    std::size_t last = end - 1;
    while (last > first && tokens[last].kind == gltfx_gfss_token_kind::whitespace) {
        --last;
    }
    return trimmed_token_span{.has_content = true, .first = first, .last = last};
}

} // namespace glintfx::style::detail
