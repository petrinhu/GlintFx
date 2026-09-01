// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/gfui/node_view.hpp>

// fake_arena_tree.hpp - GFSS-NODE-VIEW fatia C fixture (TODO.md,
// GODS_LAWS.md L-20/L-29): one of the two alien trees this fatia
// proves ESCOPO.md SS2 decision 6's eight facts are both
// IMPLEMENTABLE and SUFFICIENT against (plano SS5, fatia C). This one
// represents a consumer whose tree lives in a flat arena, indexed -
// node identity is a plain std::size_t index, disguised as a
// `const void*` at the fixture's own edge (node_view.hpp's own header
// comment, SS3.2: "o node carrega o indice disfarcado de ponteiro") -
// and every callback USES the `tree` context parameter to resolve
// that index back into a real entry, unlike fake_linked_tree.hpp's
// own pointer-based fixture, which needs no context at all. Both
// shapes are deliberate coverage of what ESCOPO.md SS2 decision 6's
// own contract has to serve (D-NV-4's own reasoning for why the view
// carries a `tree` pointer at all).
//
// NOT UNDER TDD RED/GREEN ITSELF (unlike node_query.hpp): this file
// is test ARRANGE data, the same role gfss_color_parse_test.cpp's own
// sample tables already play in this suite - what fatia C's own
// red/green cycle exercises is src/gfui/node_query.hpp's forwarders,
// using this fixture (and fake_linked_tree.hpp) as the alien tree
// underneath them.

namespace glintfx::test::fake_arena {

inline constexpr std::size_t k_no_index = static_cast<std::size_t>(-1);

struct entry {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;                              // author order (fact 3)
    std::vector<std::pair<std::string, std::string>> attributes;    // presence = found in this list (fact 4)
    glintfx::gfui::gltfx_node_state state = glintfx::gfui::gltfx_node_state::none; // fact 5
    std::size_t parent = k_no_index;           // fact 6
    std::size_t previous_sibling = k_no_index; // fact 7
    std::size_t next_sibling = k_no_index;     // fact 7
    std::size_t child_count = 0;               // fact 8 - conteudo (elemento E texto), D-NV-3
    std::size_t first_child = k_no_index;      // fact 8 - so elemento; k_no_index se so ha texto
};

// The arena itself - what gltfx_node_view::tree points at for this
// fixture. std::vector, not a raw array: a fixture whose entries never
// shrink and are only appended (add() below) never invalidates an
// already-issued INDEX, only a raw pointer would be at risk on
// growth - which is exactly why this fixture identifies nodes by
// index, not by address.
struct arena {
    std::vector<entry> entries;

    std::size_t add(entry e) {
        entries.push_back(std::move(e));
        return entries.size() - 1;
    }
};

namespace detail {

inline const void *encode_index(std::size_t index) noexcept {
    if (index == k_no_index) {
        return nullptr;
    }
    // +1 so index 0 never collides with nullptr's own "no node"
    // meaning.
    return reinterpret_cast<const void *>(static_cast<std::uintptr_t>(index) + 1U);
}

inline std::size_t decode_index(const void *node) noexcept {
    if (node == nullptr) {
        return k_no_index;
    }
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(node)) - 1U;
}

inline const entry &resolve(const void *tree, const void *node) noexcept {
    const auto *a = static_cast<const arena *>(tree);
    return a->entries[decode_index(node)];
}

} // namespace detail

inline std::string_view tag_name(const void *tree, const void *node) noexcept {
    return detail::resolve(tree, node).tag;
}

inline std::string_view id(const void *tree, const void *node) noexcept {
    return detail::resolve(tree, node).id;
}

inline void for_each_class(const void *tree, const void *node, glintfx::gfui::gltfx_node_class_visitor_fn visit,
                            void *visitor_context) noexcept {
    for (const std::string &class_name : detail::resolve(tree, node).classes) {
        if (!visit(visitor_context, class_name)) {
            return;
        }
    }
}

inline glintfx::gfui::gltfx_node_attribute attribute(const void *tree, const void *node,
                                                      std::string_view name) noexcept {
    for (const auto &[attr_name, attr_value] : detail::resolve(tree, node).attributes) {
        if (attr_name == name) {
            return glintfx::gfui::gltfx_node_attribute{.present = true, .value = attr_value};
        }
    }
    return glintfx::gfui::gltfx_node_attribute{.present = false, .value = {}};
}

inline glintfx::gfui::gltfx_node_state state(const void *tree, const void *node) noexcept {
    return detail::resolve(tree, node).state;
}

inline const void *parent(const void *tree, const void *node) noexcept {
    return detail::encode_index(detail::resolve(tree, node).parent);
}

inline const void *previous_sibling(const void *tree, const void *node) noexcept {
    return detail::encode_index(detail::resolve(tree, node).previous_sibling);
}

inline const void *next_sibling(const void *tree, const void *node) noexcept {
    return detail::encode_index(detail::resolve(tree, node).next_sibling);
}

inline std::size_t child_count(const void *tree, const void *node) noexcept {
    return detail::resolve(tree, node).child_count;
}

inline const void *first_child(const void *tree, const void *node) noexcept {
    return detail::encode_index(detail::resolve(tree, node).first_child);
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

inline glintfx::gfui::gltfx_node_view view(const arena &tree, std::size_t index) noexcept {
    return glintfx::gfui::gltfx_node_view{
        .facts = &facts(),
        .tree = &tree,
        .node = detail::encode_index(index),
    };
}

} // namespace glintfx::test::fake_arena
