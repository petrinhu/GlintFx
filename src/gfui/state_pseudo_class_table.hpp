// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "gfss/ascii_case.hpp"

// state_pseudo_class_table.hpp - GFSS-MATCH-SIMPLE fatia B (TODO.md,
// GODS_LAWS.md L-17/L-20/L-28/L-40; /var/tmp/glintfx-plan/gfss-match-
// simple-plano.md SS3.4): answers exactly one question, "which
// gltfx_node_state bit does this pseudo-class NAME (as gfss's own
// grammar spells it, hyphenated: "focus-visible") ask for" - the
// state-side counterpart to node_view.hpp's own GLINTFX_NODE_STATE_
// LIST, which names the SAME five bits through node_state.cpp's own
// diagnostic identifier vocabulary ("focus_visible", underscored,
// docs/api-conventions.md R7's own token convention for gltfx_node_
// state_name()).
//
// WHY A SEPARATE TABLE, NOT A REUSE OF node_state.cpp's OWN ONE (the
// plan's own L-17 "duas razoes de mudar" argument): node_state.cpp's
// table answers "what is this BIT called, for diagnostics" (R7); this
// one answers "what bit does THIS WORD OF THE LEAF SPELL, gfss's own
// pseudo-class vocabulary (selector_pseudo_vocabulary.hpp's own
// GLINTFX_GFSS_SIMPLE_PSEUDO_LIST) ask for". The two answers happen to
// look alike ("focus-visible" reads close to "focus_visible") but they
// change for two INDEPENDENT reasons - a diagnostic naming convention
// decided by R7, versus gfss's own leaf grammar decided by CSS
// Selectors Level 4 - so CONTRACT.md SS6's own "extract on the third
// occurrence, not before" rule does not apply here: this is not shared
// logic, it is two tables that happen to agree today.
//
// ASCII CASE-INSENSITIVE (D-MS-4 of the plan above, item 0.2's own
// finding): selector_parse.cpp's own is_known_simple_pseudo() already
// accepts ":HOVER" and stores the pseudo-class's own name RAW - a
// matcher that compared with `==` byte for byte would accept ":HOVER"
// at parse time and then never match it, forever, in silence. This
// table's own state_bit_for_pseudo_class() reuses gfss/ascii_case.hpp's
// own ascii_case_insensitive_equal() (the SAME function selector_
// pseudo_vocabulary.hpp already reuses for the identical reason) so the
// matcher and the parser agree.
//
// std::optional<gltfx_node_state>, not a bool-plus-out-param: this
// header is internal (GODS_LAWS.md L-19, not GLINTFX_API), so docs/
// api-conventions.md R1's own public-boundary convention does not
// govern it - std::optional is simply the clearer shape for a private,
// five-row lookup this small.

namespace glintfx::gfui::detail {

struct state_pseudo_class_entry {
    std::string_view name;
    gltfx_node_state bit = gltfx_node_state::none;
};

// THE table - 5 rows, one per GFSS-MATCH-SIMPLE's own scope line (plano
// SS1: "as cinco de estado"), gfss's own hyphenated spelling
// (selector_pseudo_vocabulary.hpp's own GLINTFX_GFSS_SIMPLE_PSEUDO_LIST
// rows 1-5, in the SAME order).
inline constexpr std::array<state_pseudo_class_entry, 5> k_state_pseudo_class_table{{
    {"hover", gltfx_node_state::hover},
    {"active", gltfx_node_state::active},
    {"focus", gltfx_node_state::focus},
    {"focus-visible", gltfx_node_state::focus_visible},
    {"checked", gltfx_node_state::checked},
}};

// GODS_LAWS.md L-40's own "add without registering it, watch it fail
// to compile" proof - the SAME technique node_state.cpp's own k_node_
// state_table already applies, for the SAME reason: the table's own
// size above is a HAND-WRITTEN literal (5), so it can silently drift
// from node_view.hpp's own mechanically-derived gltfx_node_state_count
// the moment a sixth state bit is added there without a matching row
// here - this static_assert turns that drift into a compile error.
static_assert(k_state_pseudo_class_table.size() == gltfx_node_state_count,
              "GODS_LAWS.md L-40: k_state_pseudo_class_table's row count must track "
              "node_view.hpp's own gltfx_node_state_count - a state bit added to the enum "
              "without a matching pseudo-class name row here must not compile silently");

[[nodiscard]] inline std::optional<gltfx_node_state>
state_bit_for_pseudo_class(std::string_view name) noexcept {
    for (const state_pseudo_class_entry &entry : k_state_pseudo_class_table) {
        if (glintfx::style::detail::ascii_case_insensitive_equal(name, entry.name)) {
            return entry.bit;
        }
    }
    return std::nullopt;
}

} // namespace glintfx::gfui::detail
