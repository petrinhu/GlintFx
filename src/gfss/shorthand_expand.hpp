// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "declaration_ast.hpp"
#include "shorthand_expand_result.hpp"

// shorthand_expand.hpp - GFSS-SHORTHAND (TODO.md wave W6, GODS_LAWS.md
// L-17/L-19/L-20/L-40, docs/plano-w6-folha-de-estilo.md S-3/D-W6-3): THE
// ENTRY POINT of this whole fatia - given one accepted-but-crude
// shorthand declaration (`is_shorthand == true`, declaration_parse.cpp's
// own build_shorthand()), expands it into the N longhand declarations it
// stands for, or a single diagnostic naming why it could not. Answers
// exactly one question - "which family owns this shorthand's own name,
// and what does the universal-keyword case do before any family ever
// sees the value?" - never HOW a family expands (shorthand_four_sides.
// hpp and its four siblings) nor WHERE the result lands in the block's
// own declaration list (declaration_list_parse.cpp's own fold_into_
// result(), the ONLY caller of this function).
//
// THE UNIVERSAL KEYWORD CHECK LIVES HERE, NOT IN ANY FAMILY (D-DP-7's
// own precedent, declaration_parse.cpp's build_known_property()): a
// shorthand whose ENTIRE value is exactly one of `inherit`/`initial`/
// `unset` expands to every one of its own longhands carrying that SAME
// keyword - checked ONCE, before dispatch, so no family file has to
// special-case it.

namespace glintfx::style::detail {

[[nodiscard]] shorthand_expand_result expand_shorthand(const gfss_declaration &shorthand);

} // namespace glintfx::style::detail
