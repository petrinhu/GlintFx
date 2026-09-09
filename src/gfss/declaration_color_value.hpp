// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/gfss/token.hpp>

// declaration_color_value.hpp - GFSS-DECL-PARSE, DP-6/D-DP-6 (TODO.md
// wave W5, GODS_LAWS.md L-17/L-20/L-27/L-28/L-40, docs/gfss-property-
// registry-v1.md D18, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-6): answers exactly
// one question - "what does a COLOR-typed property's own value read
// as?" - `currentColor` (D18's own natureza propria, resolved later by
// GFSS-INHERIT, never here) or a literal color (GFSS-COLOR-PARSE's own
// parse_color(), reused UNCHANGED - this file never re-implements any
// color grammar).
//
// currentColor RECOGNIZED HERE, BEFORE GFSS-COLOR-PARSE EVER SEES THE
// TEXT (D18, docs/gfss-property-registry-v1.md's own verbatim: "entra
// em GFSS-DECL-PARSE como palavra-chave de cor, nao como cor literal" -
// measured: zero occurrences in src/gfss/named_colors.cpp, this file's
// own D-DP-4 comment repeats the same measurement): a color-typed
// declaration whose value is EXACTLY the single ident `currentColor`
// (ASCII case-insensitive, CSS 2.1 SS4.1.3) never reaches parse_color()
// at all - color_parse.hpp/named_colors.cpp stay UNTOUCHED by this
// fatia, exactly as the plan's own D-DP-6 requires.

namespace glintfx::style::detail {

enum class declaration_color_value_kind : std::uint8_t {
    failed,
    current_color,
    resolved,
};

struct declaration_color_value_result {
    declaration_color_value_kind kind = declaration_color_value_kind::failed;
    gltfx_rgba value{};
    gltfx_gfss_diagnostic diagnostic{};
};

// Reads `tokens[begin, end)` - already known to hold at least one
// non-whitespace token (declaration_parse.cpp's own D-DP-8 pipeline
// checks that BEFORE calling this, the SAME `component_value` gate
// every other value shape shares) - as a color-typed declaration's own
// value.
[[nodiscard]] declaration_color_value_result
read_declaration_color_value(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                             std::size_t end) noexcept;

} // namespace glintfx::style::detail
