// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glintfx/gfss/token.hpp>

#include "declaration_ast.hpp"

// shorthand_reset_notice.hpp - GFSS-SHORTHAND, S-3b (TODO.md wave W6,
// GODS_LAWS.md L-17/L-20/L-40, docs/plano-w6-folha-de-estilo.md S-3b/
// D-W6-14, decisao do lider D4, 15/09/2026): the pure atom that finds
// the "reset acidental" the dossie desta onda names as the community's
// own dor numero um dos atalhos (SS1 item 3.1) - a shorthand SILENTLY
// erasing a longhand the SAME block already wrote, explicitly, before
// it. THE BEHAVIOR DOES NOT CHANGE (D-W6-4: an omitted longhand still
// resets to the registry's own initial value, and the shorthand still
// wins by POSITION, D-W6-3) - this atom only NOTICES, after the fact,
// that it happened, so declaration_list_parse.cpp's own fold_into_
// result() (the only caller, D-W6-13) can attach a diagnostic the
// author actually sees, in the block's own `notices` list.
//
// OPERATES ON THE ALREADY-EXPANDED LIST, NEVER EXPANDS ANYTHING ITSELF
// (D-W6-3's own "a posicao e preservada por construcao"): `declarations`
// is the block's own list AFTER the shorthand's N longhands were
// inserted IN PLACE at `expansion_begin` - this atom answers exactly
// one question, "of these N longhands, which ones already had an
// EXPLICIT value earlier in this SAME list?", by scanning BACKWARD from
// `expansion_begin`, exclusive - never forward. A longhand written
// AFTER the shorthand is D-W6-14's own explicit carve-out: that is
// "ultima vence", the cascade's own future design (W10), never an
// accident this shortcut caused - so this atom never looks past
// `expansion_begin`.
namespace glintfx::style::detail {

[[nodiscard]] std::vector<gltfx_gfss_diagnostic>
shorthand_reset_notices(const std::vector<gfss_declaration> &declarations,
                        std::size_t expansion_begin, std::size_t longhand_count,
                        std::uint32_t shorthand_line, std::uint32_t shorthand_column);

} // namespace glintfx::style::detail
