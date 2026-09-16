// SPDX-License-Identifier: AGPL-3.0-or-later
#include "combinator_step.hpp"

#include "gfui/node_query.hpp"

// combinator_step.cpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40): the algorithm behind combinator_step.hpp's own
// combinator_next_candidate() - see that file's own header comment for
// why one function serves all four combinators and what "candidate"
// means for each. Two axes, one node_query.hpp call each: no case here
// is more than a single forward.

namespace glintfx::gfui::detail {

gltfx_node_view combinator_next_candidate(style::detail::gfss_combinator combinator,
                                          const gltfx_node_view &node) noexcept {
    switch (combinator) {
    case style::detail::gfss_combinator::child:
    case style::detail::gfss_combinator::descendant:
        return parent(node);
    case style::detail::gfss_combinator::next_sibling:
    case style::detail::gfss_combinator::subsequent_sibling:
        return previous_sibling(node);
    }
    // Every gfss_combinator enumerator is handled above - no default
    // case, the SAME "closed enumeration is not closed" guard
    // structural_match.cpp's own two dispatchers already use
    // (GODS_LAWS.md L-40): a fifth combinator added to selector_ast.hpp
    // without a branch here fails to compile under -Werror instead of
    // silently returning a null view for it.
    return gltfx_node_view{};
}

} // namespace glintfx::gfui::detail
