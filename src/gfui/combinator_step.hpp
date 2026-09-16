// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfui/node_view.hpp>

#include "gfss/selector_ast.hpp"

// combinator_step.hpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40; docs/plano-w6-folha-de-estilo.md, fatia S-2, D-W6-2):
// ONE atom - given a combinator and a node, the single next CANDIDATE
// to its left along that combinator's own axis. Nothing here decides
// whether that candidate is the ONLY one to try (`>`/`+`, D-W6-2's own
// "passo unico") or the first of several to try in turn
// (`descendant`/`~`, D-W6-2's own "iteram... e retrocedem") - that
// decision, and the loop it drives, belongs to complex_match.cpp's own
// caller, never to this file (GODS_LAWS.md L-17: this file answers
// exactly one question, "what is next", never "how many times do I
// ask it").
//
// WHY ONE FUNCTION SERVES ALL FOUR COMBINATORS, NOT FOUR (GODS_LAWS.md
// L-33's own DRY "regra de 3", satisfied here by a different route -
// the four combinators fall into exactly two AXES, ancestor and
// sibling, and every combinator on the same axis asks node_query.hpp
// for the SAME one fact): `child` and `descendant` both ask "who is my
// parent" (fact 6); `next_sibling` and `subsequent_sibling` both ask
// "who is my previous sibling" (fact 7's own half). A four-way
// function would just be this same two-way switch typed out twice -
// the SAME "duplicacao real" CONTRACT.md SS6.7 already forbids
// elsewhere in this codebase, not a case the rule of three exists to
// guard against.
//
// RETURNS A NULL VIEW WHEN THERE IS NO CANDIDATE (node_query.hpp's own
// is_null() convention, the SAME "nullptr means no node" shape the
// eight-fact contract itself already uses for `parent`/`previous_
// sibling`): the root has no parent, the first sibling has no previous
// sibling - complex_match.cpp's own caller is what turns that into
// "rejected" (for `>`/`+`) or "the backtrack loop ends" (for
// `descendant`/`~`), never this file.

namespace glintfx::gfui::detail {

// noexcept, no allocation - the SAME guarantee every function this
// track exposes across the gfss/gfui boundary already carries
// (compound_match.hpp's own header comment, plan SS3.8).
[[nodiscard]] gltfx_node_view combinator_next_candidate(style::detail::gfss_combinator combinator,
                                                        const gltfx_node_view &node) noexcept;

} // namespace glintfx::gfui::detail
