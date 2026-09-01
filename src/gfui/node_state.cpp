// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfui/node_view.hpp>

#include <array>

// node_state.cpp - GFSS-NODE-VIEW, fatia A (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40): the name of each of the five gltfx_node_state
// bits, in ONE table - the SAME technique gfss/value_kind.cpp's own
// k_value_kind_table already established for gltfx_gfss_value_kind_
// name()/gltfx_gfss_length_unit_name().
//
// THE static_assert BELOW IS THIS FATIA'S OWN "ADD WITHOUT
// REGISTERING IT, WATCH IT FAIL TO COMPILE" PROOF (GODS_LAWS.md L-40):
// the table's own <..., N> size is a HAND-WRITTEN literal (5), not
// gltfx_node_state_count itself - if it read the mechanical count
// instead, a bit added to node_view.hpp's own GLINTFX_NODE_STATE_LIST
// without a matching row here would silently value-initialize a
// trailing table entry to {none, ""} instead of failing to build.
// Because the size is instead a literal that does NOT track the list
// automatically, the enum's own mechanically-derived count (which
// DOES track it) and this table's row count can disagree the moment
// node_view.hpp's own list grows - and static_assert makes that
// disagreement a compile error instead of a silently wrong runtime
// answer.

namespace glintfx::gfui {

namespace {

struct node_state_entry {
    gltfx_node_state bit = gltfx_node_state::none;
    std::string_view name;
};

// THE table - 5 rows, one per ESCOPO.md SS2 decision 6's own fact 5
// ("os cinco estados"). Order matches node_view.hpp's own
// GLINTFX_NODE_STATE_LIST.
constexpr std::array<node_state_entry, 5> k_node_state_table{{
    {gltfx_node_state::hover, "hover"},
    {gltfx_node_state::active, "active"},
    {gltfx_node_state::focus, "focus"},
    {gltfx_node_state::focus_visible, "focus_visible"},
    {gltfx_node_state::checked, "checked"},
}};

static_assert(k_node_state_table.size() == gltfx_node_state_count,
              "GODS_LAWS.md L-40: k_node_state_table's row count must track node_view.hpp's own "
              "gltfx_node_state_count - a bit added to the enum without a name row here must not "
              "compile silently");

} // namespace

std::string_view gltfx_node_state_name(gltfx_node_state one_bit) noexcept {
    for (const node_state_entry &entry : k_node_state_table) {
        if (entry.bit == one_bit) {
            return entry.name;
        }
    }
    // `none`, a combination of more than one bit, or a bit outside the
    // five named ones (docs/api-conventions.md R4's graceful-
    // degradation convention): never undefined behavior.
    return "unknown";
}

} // namespace glintfx::gfui
