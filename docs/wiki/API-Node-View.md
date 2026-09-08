# API reference: Node View

glintfx's future style engine will decide which style rules apply to a
node in *your* UI tree - a document, a widget hierarchy, a scene graph,
whatever shape your own program uses. It never owns or walks that tree
itself; instead, you fill in a small table of plain functions the engine
calls back into, and it asks you eight questions about one node at a
time.

**This table is the only public contract in the style engine today.**
The style engine itself (parsing a stylesheet, matching selectors) has
no public entry point yet - see the main README's ["Status"](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
section. What is here is stable and safe to implement now, ahead of the
rest landing.

## The table you fill in

[`gfui/node_view.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/gfui/node_view.hpp) -
`gltfx_node_facts` is ten function pointers, answering eight questions
about one node. Every callback takes `(tree, node)` - two opaque
pointers you provide and glintfx only ever hands back unchanged, never
dereferences itself.

| Question | Callback(s) |
|---|---|
| What is this node's tag name? | `tag_name` |
| Does it have an id? | `id` (empty means "no id") |
| What classes does it belong to? | `for_each_class` (a visitor: called once per class; return `false` from your visitor to stop early) |
| Does it have this attribute? | `attribute` (returns `present` and `value` separately - `[disabled]` and `[disabled=""]` are different things) |
| Is it hovered / active / focused / focus-visible / checked? | `state` (one call answers all five, as bits of `gltfx_node_state`) |
| Who is its parent? | `parent` (`nullptr` means "this is the root") |
| Who are its siblings? | `previous_sibling`, `next_sibling` |
| How many children does it have, and which is the first? | `child_count`, `first_child` |

`gltfx_node_facts_first_missing(facts)` returns the name of the first
callback you forgot to fill in (e.g. `"parent"`), or an empty string if
the table is complete - check this once, right after building your
table, instead of discovering a missing callback at a crash later.

`gltfx_node_view{facts, tree, node}` is the three-pointer bundle glintfx
actually passes around: your table, plus which tree and which node.

## Minimal working example

A single node with no parent, no children, no attributes and no
classes - enough to prove the table is wired correctly before building a
real tree adapter:

```cpp
#include <glintfx/gfui/node_view.hpp>

namespace {

std::string_view single_node_tag_name(const void *, const void *) noexcept {
    return "div";
}
std::string_view single_node_id(const void *, const void *) noexcept {
    return {};
}
void single_node_for_each_class(const void *, const void *,
                                 glintfx::gfui::gltfx_node_class_visitor_fn, void *) noexcept {
    // no classes: never call the visitor
}
glintfx::gfui::gltfx_node_attribute single_node_attribute(const void *, const void *,
                                                           std::string_view) noexcept {
    return {.present = false, .value = {}};
}
glintfx::gfui::gltfx_node_state single_node_state(const void *, const void *) noexcept {
    return glintfx::gfui::gltfx_node_state::none;
}
const void *single_node_parent(const void *, const void *) noexcept {
    return nullptr; // this node is the root
}
const void *single_node_sibling(const void *, const void *) noexcept {
    return nullptr; // no siblings
}
std::size_t single_node_child_count(const void *, const void *) noexcept {
    return 0;
}
const void *single_node_first_child(const void *, const void *) noexcept {
    return nullptr;
}

} // namespace

int main() {
    glintfx::gfui::gltfx_node_facts facts{
        .tag_name = single_node_tag_name,
        .id = single_node_id,
        .for_each_class = single_node_for_each_class,
        .attribute = single_node_attribute,
        .state = single_node_state,
        .parent = single_node_parent,
        .previous_sibling = single_node_sibling,
        .next_sibling = single_node_sibling,
        .child_count = single_node_child_count,
        .first_child = single_node_first_child,
    };

    // Confirm every callback was filled in before using the table.
    if (!glintfx::gfui::gltfx_node_facts_first_missing(facts).empty()) {
        return 1;
    }

    glintfx::gfui::gltfx_node_view view{.facts = &facts, .tree = nullptr, .node = nullptr};
    return view.facts->tag_name(view.tree, view.node) == "div" ? 0 : 1;
}
```

## See also

- [API Reference: Core](API-Core) - the error-handling pattern used
  throughout the rest of glintfx (this contract itself has no fallible
  call, since it never fails - it either compiles with a complete table
  or does not compile at all).
