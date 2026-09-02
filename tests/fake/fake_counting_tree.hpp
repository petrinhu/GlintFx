// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake_arena_tree.hpp"

// fake_counting_tree.hpp - GFSS-MATCH-SIMPLE fatia E fixture (TODO.md,
// GODS_LAWS.md L-20/L-29; /var/tmp/glintfx-plan/gfss-match-simple-
// plano.md SS6, fatia E): a gltfx_node_facts table whose ten callbacks
// each increment ONE counter per entry and then forward to fake_
// arena's own free functions (tests/fake/fake_arena_tree.hpp) - a
// deliberately thin wrapper around an EXISTING fixture, not a new
// alien tree of its own. What this proves is compound_match.cpp's own
// D-MS-7 ordering (id -> state -> tag -> classes, cheapest and most
// selective first, first rejection wins) by COUNTING how many times
// each fact was actually asked for, never by reading the source.
//
// NOT UNDER TDD RED/GREEN ITSELF - same role fake_arena_tree.hpp's own
// header comment already states for itself: this is test ARRANGE
// data. What IS under red/green is gfui_compound_match_test.cpp's own
// cases that read the counters this file produces.

namespace glintfx::test::fake_counting {

// One counter per gltfx_node_facts entry (node_view.hpp's own ten,
// same order), zero-initialized - GODS_LAWS.md L-40's own "the count
// is printed even when it is zero" discipline starts here: a case that
// expects zero calls to `state` reads counters::state back as a real,
// initialized 0, never an uninitialized field.
struct counters {
    std::size_t tag_name = 0;
    std::size_t id = 0;
    std::size_t for_each_class = 0;
    std::size_t attribute = 0;
    std::size_t state = 0;
    std::size_t parent = 0;
    std::size_t previous_sibling = 0;
    std::size_t next_sibling = 0;
    std::size_t child_count = 0;
    std::size_t first_child = 0;
};

// What gltfx_node_view::tree points at for this fixture: the SAME
// fake_arena::arena a scenario already built, plus the counters struct
// the ten wrappers below increment. A plain, non-owning pair - the
// arena and the counters both outlive every view this fixture's own
// view() below produces, the same lifetime discipline fake_arena_
// tree.hpp's own arena already documents for gltfx_node_view::tree.
struct tree {
    const fake_arena::arena *arena = nullptr;
    counters *counts = nullptr;
};

inline std::string_view tag_name(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->tag_name;
    return fake_arena::tag_name(t->arena, node);
}

inline std::string_view id(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->id;
    return fake_arena::id(t->arena, node);
}

inline void for_each_class(const void *tree_ptr, const void *node,
                           glintfx::gfui::gltfx_node_class_visitor_fn visit,
                           void *visitor_context) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->for_each_class;
    fake_arena::for_each_class(t->arena, node, visit, visitor_context);
}

inline glintfx::gfui::gltfx_node_attribute attribute(const void *tree_ptr, const void *node,
                                                     std::string_view name) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->attribute;
    return fake_arena::attribute(t->arena, node, name);
}

inline glintfx::gfui::gltfx_node_state state(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->state;
    return fake_arena::state(t->arena, node);
}

inline const void *parent(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->parent;
    return fake_arena::parent(t->arena, node);
}

inline const void *previous_sibling(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->previous_sibling;
    return fake_arena::previous_sibling(t->arena, node);
}

inline const void *next_sibling(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->next_sibling;
    return fake_arena::next_sibling(t->arena, node);
}

inline std::size_t child_count(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->child_count;
    return fake_arena::child_count(t->arena, node);
}

inline const void *first_child(const void *tree_ptr, const void *node) noexcept {
    const auto *t = static_cast<const tree *>(tree_ptr);
    ++t->counts->first_child;
    return fake_arena::first_child(t->arena, node);
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

inline glintfx::gfui::gltfx_node_view view(const tree &counting_tree, std::size_t index) noexcept {
    return glintfx::gfui::gltfx_node_view{
        .facts = &facts(),
        .tree = &counting_tree,
        .node = fake_arena::detail::encode_index(index),
    };
}

} // namespace glintfx::test::fake_counting
