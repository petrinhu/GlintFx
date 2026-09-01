// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "fake/fake_linked_tree.hpp"
#include "gfui/node_query.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_node_view_test.cpp - GFSS-NODE-VIEW (TODO.md, GODS_LAWS.md
// L-20/L-40): TDD red/green witness for glintfx::gfui::gltfx_node_
// facts/gltfx_node_view/gltfx_node_attribute and gltfx_node_facts_
// first_missing() - see node_view.hpp's own header comment for the
// design rationale.
//
// FATIA B (this file, structural cases below): the closed-
// enumeration counts and the first_missing() diagnostic, built on a
// table of stub callbacks that answer nothing real - a valid table
// shape, not a valid tree, is all this fatia's own cases need.
//
// FATIA C (appended to this SAME file, plano SS5: no separate file is
// listed for fatia C): navigation over two real, alien fixture trees
// (tests/fake/fake_arena_tree.hpp, tests/fake/fake_linked_tree.hpp),
// via src/gfui/node_query.hpp's own internal forwarders - proving the
// table is both IMPLEMENTABLE by an alien tree and SUFFICIENT to
// derive what ESCOPO.md SS2 decision 6 says the library must not ask
// for (sibling index, of-type position).

namespace {

std::string_view stub_tag_name(const void * /*tree*/, const void * /*node*/) noexcept {
    return "div";
}
std::string_view stub_id(const void * /*tree*/, const void * /*node*/) noexcept {
    return "";
}
void stub_for_each_class(const void * /*tree*/, const void * /*node*/,
                          glintfx::gfui::gltfx_node_class_visitor_fn /*visit*/,
                          void * /*visitor_context*/) noexcept {}
glintfx::gfui::gltfx_node_attribute stub_attribute(const void * /*tree*/, const void * /*node*/,
                                                    std::string_view /*name*/) noexcept {
    return {};
}
glintfx::gfui::gltfx_node_state stub_state(const void * /*tree*/, const void * /*node*/) noexcept {
    return glintfx::gfui::gltfx_node_state::none;
}
const void *stub_parent(const void * /*tree*/, const void * /*node*/) noexcept {
    return nullptr;
}
const void *stub_previous_sibling(const void * /*tree*/, const void * /*node*/) noexcept {
    return nullptr;
}
const void *stub_next_sibling(const void * /*tree*/, const void * /*node*/) noexcept {
    return nullptr;
}
std::size_t stub_child_count(const void * /*tree*/, const void * /*node*/) noexcept {
    return 0;
}
const void *stub_first_child(const void * /*tree*/, const void * /*node*/) noexcept {
    return nullptr;
}

// A COMPLETE table - every one of the ten entries filled with a stub
// callback. gltfx_node_facts_first_missing_of_complete_table_is_empty
// and gltfx_node_facts_first_missing_names_the_one_null_entry below
// each start from a copy of this and null out at most one field.
glintfx::gfui::gltfx_node_facts make_full_facts() noexcept {
    return glintfx::gfui::gltfx_node_facts{
        .tag_name = &stub_tag_name,
        .id = &stub_id,
        .for_each_class = &stub_for_each_class,
        .attribute = &stub_attribute,
        .state = &stub_state,
        .parent = &stub_parent,
        .previous_sibling = &stub_previous_sibling,
        .next_sibling = &stub_next_sibling,
        .child_count = &stub_child_count,
        .first_child = &stub_first_child,
    };
}

} // namespace

// GODS_LAWS.md L-40 (contagem impressa mesmo quando passa): both
// counts are MECHANICALLY derived (node_view.hpp's own gltfx_node_
// facts_entry_count/gltfx_node_facts_fact_count), never hand-typed
// here - this case only fixes what ESCOPO.md SS2 decision 6 promises:
// ten callback entries answering eight facts.
GLINTFX_TEST(gltfx_node_facts_counts_are_ten_entries_eight_facts) {
    std::printf("gfui_node_view_test: gltfx_node_facts_entry_count = %zu\n",
                 glintfx::gfui::gltfx_node_facts_entry_count);
    std::printf("gfui_node_view_test: gltfx_node_facts_fact_count = %zu\n",
                 glintfx::gfui::gltfx_node_facts_fact_count);
    GLINTFX_CHECK_EQ(glintfx::gfui::gltfx_node_facts_entry_count, static_cast<std::size_t>(10));
    GLINTFX_CHECK_EQ(glintfx::gfui::gltfx_node_facts_fact_count, static_cast<std::size_t>(8));
}

GLINTFX_TEST(gltfx_node_facts_first_missing_of_complete_table_is_empty) {
    const glintfx::gfui::gltfx_node_facts facts = make_full_facts();
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts).empty());
}

// Enumerates the WHOLE closed space (GODS_LAWS.md L-17: espaco
// pequeno e fechado se enumera inteiro, nunca amostra) - all ten
// entries, one case each, never left to compile as null by omission.
GLINTFX_TEST(gltfx_node_facts_first_missing_names_the_one_null_entry) {
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.tag_name = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"tag_name"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.id = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"id"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.for_each_class = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"for_each_class"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.attribute = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"attribute"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.state = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"state"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.parent = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"parent"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.previous_sibling = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"previous_sibling"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.next_sibling = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"next_sibling"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.child_count = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"child_count"});
    }
    {
        glintfx::gfui::gltfx_node_facts facts = make_full_facts();
        facts.first_child = nullptr;
        GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(facts) == std::string_view{"first_child"});
    }
    std::printf("gfui_node_view_test: 10 first_missing cases checked (one per entry, whole closed space)\n");
}

GLINTFX_TEST(gltfx_node_view_default_is_no_node) {
    constexpr glintfx::gfui::gltfx_node_view view;
    GLINTFX_CHECK(view.facts == nullptr);
    GLINTFX_CHECK(view.tree == nullptr);
    GLINTFX_CHECK(view.node == nullptr);
}

GLINTFX_TEST(gltfx_node_attribute_default_reads_back_as_absent) {
    // R4 convention (docs/api-conventions.md): present == false means
    // "the node has no such attribute" - a default-constructed
    // gltfx_node_attribute answers that.
    constexpr glintfx::gfui::gltfx_node_attribute attribute;
    GLINTFX_CHECK(!attribute.present);
    GLINTFX_CHECK(attribute.value.empty());
}

// --- fatia C: navigation over two alien trees, through node_query.hpp
// (TODO.md, GODS_LAWS.md L-20/L-27/L-29/L-40) ---
//
// The SAME scenario shape is built twice below (once per fixture) and
// checked by this ONE shared function - not duplicated per-fixture,
// per ESCOPO.md SS2 decision 6's own principle proven twice on
// purpose: a flat html>a.one[disabled][data-x=42].alpha.beta.gamma,
// b(hover+focus), a, c row of four siblings under one root, plus a
// standalone "p" node with content but no element child (D-NV-3: the
// b/c between child_count and first_child).
namespace {

void check_scenario(std::string_view label, const glintfx::gfui::gltfx_node_view &root,
                     const glintfx::gfui::gltfx_node_view &first_child,
                     const glintfx::gfui::gltfx_node_view &second_child,
                     const glintfx::gfui::gltfx_node_view &third_child,
                     const glintfx::gfui::gltfx_node_view &fourth_child,
                     const glintfx::gfui::gltfx_node_view &text_only_child) {
    namespace detail = glintfx::gfui::detail;

    // fact 1 - tag name
    GLINTFX_CHECK(detail::tag_name(first_child) == std::string_view{"a"});
    GLINTFX_CHECK(detail::tag_name(second_child) == std::string_view{"b"});

    // fact 2 - id, present and absent (empty = none, R4)
    GLINTFX_CHECK(detail::id(first_child) == std::string_view{"one"});
    GLINTFX_CHECK(detail::id(second_child).empty());

    // fact 3 - classes in author order; has_class finds the SECOND of
    // three and STOPS - the fixture's own visitor is called exactly
    // twice, never a third time (proves the stop, not just the
    // answer).
    struct call_counter {
        std::string_view target;
        bool found = false;
        int calls = 0;
    };
    call_counter counter{.target = "beta", .found = false, .calls = 0};
    first_child.facts->for_each_class(
        first_child.tree, first_child.node,
        [](void *raw_context, std::string_view class_name) noexcept -> bool {
            auto *self = static_cast<call_counter *>(raw_context);
            ++self->calls;
            if (class_name == self->target) {
                self->found = true;
                return false;
            }
            return true;
        },
        &counter);
    GLINTFX_CHECK(counter.found);
    GLINTFX_CHECK_EQ(counter.calls, 2);
    GLINTFX_CHECK(detail::has_class(first_child, "beta"));
    GLINTFX_CHECK(!detail::has_class(first_child, "zzz"));

    // fact 4 - attribute absent / present empty / present with value
    GLINTFX_CHECK(!detail::attribute(first_child, "missing-attr").present);
    const glintfx::gfui::gltfx_node_attribute disabled_attr = detail::attribute(first_child, "disabled");
    GLINTFX_CHECK(disabled_attr.present);
    GLINTFX_CHECK(disabled_attr.value.empty());
    const glintfx::gfui::gltfx_node_attribute data_x_attr = detail::attribute(first_child, "data-x");
    GLINTFX_CHECK(data_x_attr.present);
    GLINTFX_CHECK(data_x_attr.value == std::string_view{"42"});

    // fact 5 - the five bits individually and combined, ONE call
    const glintfx::gfui::gltfx_node_state second_state = detail::state(second_child);
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_has(second_state, glintfx::gfui::gltfx_node_state::hover));
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_has(second_state, glintfx::gfui::gltfx_node_state::focus));
    GLINTFX_CHECK(!glintfx::gfui::gltfx_node_state_has(second_state, glintfx::gfui::gltfx_node_state::active));

    // fact 6 - pai da raiz e nulo; filho tem pai
    GLINTFX_CHECK(detail::is_null(detail::parent(root)));
    GLINTFX_CHECK(!detail::is_null(detail::parent(first_child)));

    // fact 7 - irmaos nas duas pontas nulos
    GLINTFX_CHECK(detail::is_null(detail::previous_sibling(first_child)));
    GLINTFX_CHECK(detail::is_null(detail::next_sibling(fourth_child)));

    // fact 8 - child_count de folha zero; no com so texto tem
    // child_count == 1 e first_child nulo (D-NV-3's own distinction)
    GLINTFX_CHECK_EQ(detail::child_count(fourth_child), static_cast<std::size_t>(0));
    GLINTFX_CHECK(detail::is_null(detail::first_child(fourth_child)));
    GLINTFX_CHECK_EQ(detail::child_count(text_only_child), static_cast<std::size_t>(1));
    GLINTFX_CHECK(detail::is_null(detail::first_child(text_only_child)));

    // DERIVABILIDADE (ESCOPO.md SS2 decision 6's own promise): sibling
    // index and of-type position of third_child, computed HERE by
    // walking only previous_sibling over the four mixed-tag siblings
    // (a, b, a, c) - no entry beyond the ten was asked for either.
    std::size_t sibling_index = 0;
    std::size_t of_type_index = 0;
    const std::string_view third_tag = detail::tag_name(third_child);
    glintfx::gfui::gltfx_node_view cursor = detail::previous_sibling(third_child);
    while (!detail::is_null(cursor)) {
        ++sibling_index;
        if (detail::tag_name(cursor) == third_tag) {
            ++of_type_index;
        }
        cursor = detail::previous_sibling(cursor);
    }
    GLINTFX_CHECK_EQ(sibling_index, static_cast<std::size_t>(2));
    GLINTFX_CHECK_EQ(of_type_index, static_cast<std::size_t>(1));

    std::printf("gfui_node_view_test: fatia C scenario '%.*s' - 10 facts + has_class + derivability "
                "checked (%zu entries answered)\n",
                static_cast<int>(label.size()), label.data(), glintfx::gfui::gltfx_node_facts_entry_count);
}

} // namespace

GLINTFX_TEST(gfui_node_query_forwarders_and_derivability_over_fake_arena_tree) {
    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::view;

    using glintfx::test::fake_arena::k_no_index;

    arena tree;
    const std::size_t root_index = tree.add(entry{
        .tag = "html",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 4,
        .first_child = k_no_index,
    });
    const std::size_t first_index = tree.add(entry{
        .tag = "a",
        .id = "one",
        .classes = {"alpha", "beta", "gamma"},
        .attributes = {{"disabled", ""}, {"data-x", "42"}},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = root_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const std::size_t second_index = tree.add(entry{
        .tag = "b",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = static_cast<glintfx::gfui::gltfx_node_state>(
            static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::hover) |
            static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::focus)),
        .parent = root_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const std::size_t third_index = tree.add(entry{
        .tag = "a",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = root_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const std::size_t fourth_index = tree.add(entry{
        .tag = "c",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = root_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 0,
        .first_child = k_no_index,
    });
    const std::size_t text_only_index = tree.add(entry{
        .tag = "p",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = k_no_index,
        .previous_sibling = k_no_index,
        .next_sibling = k_no_index,
        .child_count = 1,
        .first_child = k_no_index,
    });

    tree.entries[root_index].first_child = first_index;
    tree.entries[first_index].next_sibling = second_index;
    tree.entries[second_index].previous_sibling = first_index;
    tree.entries[second_index].next_sibling = third_index;
    tree.entries[third_index].previous_sibling = second_index;
    tree.entries[third_index].next_sibling = fourth_index;
    tree.entries[fourth_index].previous_sibling = third_index;

    check_scenario("arena", view(tree, root_index), view(tree, first_index), view(tree, second_index),
                   view(tree, third_index), view(tree, fourth_index), view(tree, text_only_index));
}

GLINTFX_TEST(gfui_node_query_forwarders_and_derivability_over_fake_linked_tree) {
    using glintfx::test::fake_linked::node;
    using glintfx::test::fake_linked::view;

    node root{
        .tag = "html",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = nullptr,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 4,
        .first_child = nullptr,
    };
    node first{
        .tag = "a",
        .id = "one",
        .classes = {"alpha", "beta", "gamma"},
        .attributes = {{"disabled", ""}, {"data-x", "42"}},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = &root,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 0,
        .first_child = nullptr,
    };
    node second{
        .tag = "b",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = static_cast<glintfx::gfui::gltfx_node_state>(
            static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::hover) |
            static_cast<std::uint8_t>(glintfx::gfui::gltfx_node_state::focus)),
        .parent = &root,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 0,
        .first_child = nullptr,
    };
    node third{
        .tag = "a",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = &root,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 0,
        .first_child = nullptr,
    };
    node fourth{
        .tag = "c",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = &root,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 0,
        .first_child = nullptr,
    };
    node text_only{
        .tag = "p",
        .id = "",
        .classes = {},
        .attributes = {},
        .state = glintfx::gfui::gltfx_node_state::none,
        .parent = nullptr,
        .previous_sibling = nullptr,
        .next_sibling = nullptr,
        .child_count = 1,
        .first_child = nullptr,
    };

    root.first_child = &first;
    first.next_sibling = &second;
    second.previous_sibling = &first;
    second.next_sibling = &third;
    third.previous_sibling = &second;
    third.next_sibling = &fourth;
    fourth.previous_sibling = &third;

    check_scenario("linked", view(root), view(first), view(second), view(third), view(fourth), view(text_only));
}
