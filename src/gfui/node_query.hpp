// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

// node_query.hpp - GFSS-NODE-VIEW fatia C, INTERNAL header (TODO.md,
// GODS_LAWS.md L-19: "o header nasce interno" - not installed, not
// exported; NOT under include/glintfx/ - promoting any of this is
// GFSS-MATCH-SIMPLE's own decision, W10, plano SS3.3). One-line
// forwarders from a gltfx_node_view to the fact its own table
// answers, plus has_class(), the one DERIVED (not table-listed) query
// this fatia ships.

namespace glintfx::gfui::detail {

[[nodiscard]] inline bool is_null(const gltfx_node_view &view) noexcept {
    return view.node == nullptr;
}

[[nodiscard]] inline std::string_view tag_name(const gltfx_node_view &view) noexcept {
    return view.facts->tag_name(view.tree, view.node);
}

[[nodiscard]] inline std::string_view id(const gltfx_node_view &view) noexcept {
    return view.facts->id(view.tree, view.node);
}

[[nodiscard]] inline gltfx_node_attribute attribute(const gltfx_node_view &view, std::string_view name) noexcept {
    return view.facts->attribute(view.tree, view.node, name);
}

[[nodiscard]] inline gltfx_node_state state(const gltfx_node_view &view) noexcept {
    return view.facts->state(view.tree, view.node);
}

[[nodiscard]] inline gltfx_node_view parent(const gltfx_node_view &view) noexcept {
    return gltfx_node_view{.facts = view.facts, .tree = view.tree, .node = view.facts->parent(view.tree, view.node)};
}

[[nodiscard]] inline gltfx_node_view previous_sibling(const gltfx_node_view &view) noexcept {
    return gltfx_node_view{
        .facts = view.facts, .tree = view.tree, .node = view.facts->previous_sibling(view.tree, view.node)};
}

[[nodiscard]] inline gltfx_node_view next_sibling(const gltfx_node_view &view) noexcept {
    return gltfx_node_view{
        .facts = view.facts, .tree = view.tree, .node = view.facts->next_sibling(view.tree, view.node)};
}

[[nodiscard]] inline std::size_t child_count(const gltfx_node_view &view) noexcept {
    return view.facts->child_count(view.tree, view.node);
}

[[nodiscard]] inline gltfx_node_view first_child(const gltfx_node_view &view) noexcept {
    return gltfx_node_view{
        .facts = view.facts, .tree = view.tree, .node = view.facts->first_child(view.tree, view.node)};
}

// Derived (not a table entry): membership in ONE class, via fact 3's
// own visitor with early stop (plano D-NV-2's own chosen shape:
// "pede-se ao consumidor apenas o que ninguém além dele pode
// responder" - membership is derivable from the enumeration, so it
// stays here, not as an eleventh table entry).
namespace has_class_detail {

struct visitor_context {
    std::string_view target;
    bool found = false;
};

[[nodiscard]] inline bool visit_one_class(void *raw_context, std::string_view class_name) noexcept {
    auto *self = static_cast<visitor_context *>(raw_context);
    if (class_name == self->target) {
        self->found = true;
        return false;
    }
    return true;
}

} // namespace has_class_detail

[[nodiscard]] inline bool has_class(const gltfx_node_view &view, std::string_view name) noexcept {
    has_class_detail::visitor_context context{.target = name, .found = false};
    view.facts->for_each_class(view.tree, view.node, &has_class_detail::visit_one_class, &context);
    return context.found;
}

} // namespace glintfx::gfui::detail
