// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_component_split.hpp"

#include "nesting_depth.hpp"

// shorthand_component_split.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17:
// this file's own function answers exactly one question of shorthand_
// component_split.hpp's own header comment scope).

namespace glintfx::style::detail {

namespace {

[[nodiscard]] bool span_has_content(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                                    std::size_t end) noexcept {
    for (std::size_t i = begin; i < end; ++i) {
        if (tokens[i].kind != gltfx_gfss_token_kind::whitespace) {
            return true;
        }
    }
    return false;
}

void append_span_if_non_empty(std::vector<declaration_span> &spans,
                              const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                              std::size_t end) {
    if (span_has_content(tokens, begin, end)) {
        spans.push_back(declaration_span{.begin = begin, .end = end});
    }
}

} // namespace

std::vector<declaration_span>
split_shorthand_components(const std::vector<gltfx_gfss_token> &tokens) {
    std::vector<declaration_span> spans;
    std::size_t start = 0;
    int depth = 0;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const gltfx_gfss_token_kind kind = tokens[i].kind;

        if (kind == gltfx_gfss_token_kind::whitespace && depth == 0) {
            append_span_if_non_empty(spans, tokens, start, i);
            start = i + 1;
            continue;
        }

        // Same clamp as declaration_split.cpp's own loop (that file's
        // own header comment names the measured defect a missing clamp
        // caused there): a closing token nobody opened must never drive
        // depth negative and cancel out a real opening token later.
        depth += nesting_depth_delta(kind);
        if (depth < 0) {
            depth = 0;
        }
    }

    append_span_if_non_empty(spans, tokens, start, tokens.size());
    return spans;
}

} // namespace glintfx::style::detail
