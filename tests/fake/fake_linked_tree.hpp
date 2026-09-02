// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/gfui/node_view.hpp>

// fake_linked_tree.hpp - GFSS-NODE-VIEW fatia C fixture: the OTHER
// alien tree shape this fatia proves against - intrusive nodes linked
// by real pointers, no arena, no index. `tree` is always nullptr for
// this fixture's own views (plano SS5, fatia C: "tree == nullptr; nao
// usa o contexto") - every callback ignores its own `tree` parameter
// on purpose, proving node_view.hpp's own SS3.4 item 3 clause
// ("nullptr em tree e legal") is not a theoretical allowance nobody
// exercises.
//
// NOT UNDER TDD RED/GREEN ITSELF - see fake_arena_tree.hpp's own
// header comment for why (this is ARRANGE data, not the SUT).

namespace glintfx::test::fake_linked {

struct node {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> attributes;
    glintfx::gfui::gltfx_node_state state = glintfx::gfui::gltfx_node_state::none;
    node *parent = nullptr;
    node *previous_sibling = nullptr;
    node *next_sibling = nullptr;
    std::size_t child_count = 0;
    node *first_child = nullptr;
};

inline std::string_view tag_name(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->tag;
}

inline std::string_view id(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->id;
}

inline void for_each_class(const void * /*tree*/, const void *node_ptr,
                           glintfx::gfui::gltfx_node_class_visitor_fn visit,
                           void *visitor_context) noexcept {
    for (const std::string &class_name : static_cast<const node *>(node_ptr)->classes) {
        if (!visit(visitor_context, class_name)) {
            return;
        }
    }
}

inline glintfx::gfui::gltfx_node_attribute attribute(const void * /*tree*/, const void *node_ptr,
                                                     std::string_view name) noexcept {
    for (const auto &[attr_name, attr_value] : static_cast<const node *>(node_ptr)->attributes) {
        if (attr_name == name) {
            return glintfx::gfui::gltfx_node_attribute{.present = true, .value = attr_value};
        }
    }
    return glintfx::gfui::gltfx_node_attribute{.present = false, .value = {}};
}

inline glintfx::gfui::gltfx_node_state state(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->state;
}

inline const void *parent(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->parent;
}

inline const void *previous_sibling(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->previous_sibling;
}

inline const void *next_sibling(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->next_sibling;
}

inline std::size_t child_count(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->child_count;
}

inline const void *first_child(const void * /*tree*/, const void *node_ptr) noexcept {
    return static_cast<const node *>(node_ptr)->first_child;
}

inline const glintfx::gfui::gltfx_node_facts &facts() noexcept {
    static constexpr glintfx::gfui::gltfx_node_facts k_facts{
        .tag_name = &tag_name,
        .id = &id,
        .for_each_class = &for_each_class,
        .attribute = &attribute,
        .state = &state,
        .parent = &parent,
        .previous_sibling = &previous_sibling,
        .next_sibling = &next_sibling,
        .child_count = &child_count,
        .first_child = &first_child,
    };
    return k_facts;
}

inline glintfx::gfui::gltfx_node_view view(const node &n) noexcept {
    return glintfx::gfui::gltfx_node_view{.facts = &facts(), .tree = nullptr, .node = &n};
}

} // namespace glintfx::test::fake_linked
