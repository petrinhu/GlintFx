// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfss/token.hpp>

// nesting_depth.hpp - GFSS-DECL-PARSE, DP-2/DP-6 (TODO.md wave W5,
// GODS_LAWS.md L-17/L-33/L-40): the ONE nesting-depth delta both
// declaration_split.cpp (top-level `;`) and declaration_value_check.cpp
// (top-level `,`) need to walk a token vector without stopping inside a
// parenthesized/bracketed/braced construct - CONTRACT.md SS6.7's own
// "regra de tres" (a THIRD real occurrence of the identical delta
// calculation - selector_parse.cpp's own paren_depth_delta() was the
// first, parens only; declaration_split's own top-level-`;` search was
// the second, extended to three pairs) is what earns this its own file
// instead of staying duplicated a third time.

namespace glintfx::style::detail {

// +1 entering a construct that can hide a delimiter of its own inside
// it (an opening paren, a <function-token>, an opening square bracket,
// an opening curly brace), -1 leaving one, 0 for everything else.
[[nodiscard]] constexpr int nesting_depth_delta(gltfx_gfss_token_kind kind) noexcept {
    if (kind == gltfx_gfss_token_kind::open_paren || kind == gltfx_gfss_token_kind::function ||
        kind == gltfx_gfss_token_kind::open_square || kind == gltfx_gfss_token_kind::open_curly) {
        return 1;
    }
    if (kind == gltfx_gfss_token_kind::close_paren || kind == gltfx_gfss_token_kind::close_square ||
        kind == gltfx_gfss_token_kind::close_curly) {
        return -1;
    }
    return 0;
}

} // namespace glintfx::style::detail
