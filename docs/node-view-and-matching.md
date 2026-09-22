# node-view-and-matching.md

glintfx's style engine (`gfui`) will eventually decide, for every node in a consumer's own UI tree, which style rules apply to it. To do that without owning that tree - a consumer's document, widget hierarchy, or scene graph is theirs, not glintfx's - the engine asks the consumer eight questions about one node at a time, and nothing else. This document explains that contract, and how the engine currently uses the answers to decide whether one style rule matches one node.

**Only one thing described here is a public contract: the eight-fact table below (`<glintfx/gfui/node_view.hpp>`).** Everything past "How a compound selector gets judged" is internal engine code - not installed, not exported, and free to change shape at any time without a version bump. It is documented here for transparency about how matching works today, not as a promise about how it will keep working.

## The eight facts, and why exactly these

A consumer implements this contract once, by filling in a table of plain function pointers glintfx calls back into. The governing rule, in the project owner's own words: **ask the consumer only what nobody but the consumer can answer.** Anything the engine can work out for itself by walking the tree with what these eight already give it - a node's position among its siblings, how many siblings it has, its position among same-tagged siblings - is deliberately left out. That is also why there is no "give me your whole tree" call: the engine never owns or walks the consumer's data structure directly, it always asks, node by node.

| # | The engine asks... | Answered by |
|---|---|---|
| 1 | What is this node's tag name? | `tag_name` |
| 2 | Does it have an id, and if so, what? | `id` |
| 3 | What classes does it belong to? | `for_each_class` |
| 4 | Does it have this specific attribute, and what's its value? | `attribute` |
| 5 | Is it hovered / being clicked / focused / keyboard-focused / checked? | `state` (one call answers all five) |
| 6 | Who is its parent? | `parent` |
| 7 | Who are its previous and next siblings? | `previous_sibling`, `next_sibling` |
| 8 | How many children does it have, and which is the first? | `child_count`, `first_child` |

Facts 7 and 8 are each answered by two callbacks (ten entries in the table for eight facts) because there was no single-value shape that could answer "who is my previous sibling AND my next sibling" or "how many children AND which is the first" at once without inventing a compound return type just for this. One deliberate omission, named so nobody re-derives it: whatever a style query anchors "starting from here" to (a `:scope` selector, for instance) belongs to the *query*, not to the node - it is never part of this contract.

Once a field is added to this table, every consumer that already implemented it is required to add the new callback the moment they upgrade to a version of glintfx that has it - this is the one contract in this delivery that a consumer, not glintfx, fills in, and it was shown to the project owner in full before being frozen for exactly that reason.

## How the answers read: what empty and null mean

- **`id`**: an empty result means the node has no id. There is no separate "has an id" flag - empty *is* absent, by convention (the same rule this project's public error type already uses for an unattached diagnostic field).
- **`attribute`**: this one is different on purpose. It returns two things, not one: `present` and `value`. `present == false` means the node has no such attribute at all. `present == true` with an *empty* `value` means the attribute exists and its value happens to be empty - the distinction a selector like `[disabled]` (present, any value) needs against `[disabled=""]` (present, specifically empty). Collapsing the two into "empty means absent" here would silently break that distinction.
- **`for_each_class`**: not a list handed back by value - a visitor callback the engine passes in, which the implementation calls once per class the node belongs to. Returning `false` from the visitor is a request to stop early (the engine does this the moment it has seen enough, for instance while checking whether one specific class is present); an implementation that keeps calling after being told to stop violates its own side of the contract, not the engine's.
- **`state`**: one call returns a single value carrying up to five independent flags at once (hovered, being clicked/"active", focused, keyboard-focused/"focus-visible", and checked) - a selector asking about two of them at once (`:hover:focus`) still costs the consumer exactly one call, not two.
- **`parent` / `previous_sibling` / `next_sibling` / `first_child`**: a null result is meaningful, not an error - it means "there is no such node" (the root has no parent, the last sibling has no next one, a leaf has no first child).

## Filling the table

`gltfx_node_facts` is a plain struct of ten function pointers, all `noexcept`, taking a `const void *tree` and a `const void *node` the engine never dereferences itself - it only ever hands them back to the same callback that produced them, unchanged. This shape (rather than a template or a virtual base) exists because the consumer's own tree type is picked at runtime, by code this library never sees at the point it was compiled - a template can't cross that boundary, and a virtual base would put a vtable on a type that has to stay ABI-stable. `gltfx_node_view` is the three-pointer bundle the engine actually carries around: the facts table, an opaque `tree` context, and the current `node`.

A minimal, self-contained example - a toy tree of exactly one node - showing the shape a real implementation follows (compiled and run against this project's own headers before this document was written):

```cpp
#include <glintfx/gfui/node_view.hpp>
#include <string_view>

struct toy_node {
    std::string_view tag;
    std::string_view id;
    std::string_view class_name;
};

namespace {

std::string_view toy_tag_name(const void *, const void *node) noexcept {
    return static_cast<const toy_node *>(node)->tag;
}
std::string_view toy_id(const void *, const void *node) noexcept {
    return static_cast<const toy_node *>(node)->id;
}
void toy_for_each_class(const void *, const void *node,
                         glintfx::gfui::gltfx_node_class_visitor_fn visit,
                         void *visitor_context) noexcept {
    const auto *self = static_cast<const toy_node *>(node);
    if (!self->class_name.empty()) {
        visit(visitor_context, self->class_name);
    }
}
glintfx::gfui::gltfx_node_attribute toy_attribute(const void *, const void *,
                                                  std::string_view) noexcept {
    return glintfx::gfui::gltfx_node_attribute{.present = false, .value = {}};
}
glintfx::gfui::gltfx_node_state toy_state(const void *, const void *) noexcept {
    return glintfx::gfui::gltfx_node_state::none;
}
const void *toy_parent(const void *, const void *) noexcept { return nullptr; }
const void *toy_previous_sibling(const void *, const void *) noexcept { return nullptr; }
const void *toy_next_sibling(const void *, const void *) noexcept { return nullptr; }
std::size_t toy_child_count(const void *, const void *) noexcept { return 0; }
const void *toy_first_child(const void *, const void *) noexcept { return nullptr; }

constexpr glintfx::gfui::gltfx_node_facts kToyFacts{
    .tag_name = &toy_tag_name,
    .id = &toy_id,
    .for_each_class = &toy_for_each_class,
    .attribute = &toy_attribute,
    .state = &toy_state,
    .parent = &toy_parent,
    .previous_sibling = &toy_previous_sibling,
    .next_sibling = &toy_next_sibling,
    .child_count = &toy_child_count,
    .first_child = &toy_first_child,
};

} // namespace

int main() {
    toy_node root{.tag = "button", .id = "ok", .class_name = "primary"};
    glintfx::gfui::gltfx_node_view view{.facts = &kToyFacts, .tree = nullptr, .node = &root};
    return view.node == nullptr ? 1 : 0;
}
```

A table left incomplete during development (a callback still `nullptr`) can be diagnosed with `gltfx_node_facts_first_missing()`, which returns the name of the first missing entry (for example `"previous_sibling"`) or an empty result once every entry is filled - useful while building a consumer's implementation incrementally, not something a finished one should ever need to call.

## How a compound selector gets judged

*(Internal from here on - none of the types or functions below are installed or exported. `GFSS-API`, a dedicated future review, is what decides whether any of this is ever promoted to a public header.)*

A "compound selector" is one glued-together unit with no combinator inside it - `button.primary#ok`, for instance, as opposed to `div > button.primary`, which has two compounds joined by a combinator. The internal `match_compound()` function decides whether one compound matches one node, using only the eight facts above - never looking at any other node, never following a combinator.

It answers with one of three outcomes, not a plain yes/no:

- **matched** - every part of the compound this piece of code knows how to judge held true, and there was nothing in the compound it didn't recognize.
- **rejected** - at least one part it does know how to judge failed. This is decided before anything unrecognized in the compound is even considered: a compound that is going to be rejected anyway is rejected as cheaply as possible.
- **deferred** - nothing it checked failed, but the compound also contains at least one kind of requirement this piece of code does not judge (see below) - so the true answer is not yet known, and belongs to a later part of the engine.

A plain boolean could not tell this story honestly. Answering "true" for a compound like `a:first-child` when `:first-child` was never actually looked at would be a lie one way; answering "false" would be a lie the other way, since the compound might well match once the unjudged part is evaluated. "Deferred" says exactly what it means: what this code owns is settled, what it doesn't is still open.

**What this code currently judges:** the element's tag name, its id, its classes, and the five state pseudo-classes (`:hover`, `:active`, `:focus`, `:focus-visible`, `:checked`) - checked in that order, cheapest and most narrowing first, stopping at the first thing that fails.

**The case-sensitivity rule, and the reasoning behind it:** a tag name and a state pseudo-class name are compared ignoring letter case - they are vocabulary the *language* defines, the same way an HTML tag name is. A class name and an id are compared exactly, byte for byte - they are identifiers the *document's author* chose, and changing their case changes their meaning, the same distinction HTML itself draws between an element name and a `class` attribute's value.

## What is not judged yet - everything deferred, and why

Every selector kind below is real in glintfx's own selector grammar today (the parser already accepts it) but has no code yet deciding whether it holds. A compound carrying any of the following comes back `deferred`, never `matched` or `rejected`, from `match_compound()`:

- **Structural pseudo-classes** - `:first-child`, `:last-child`, `:nth-child()`, `:nth-of-type()`, `:only-child`, `:empty`, and their siblings. These require walking to other nodes (counting siblings, checking types), which is outside what a single-node judgment can answer.
- **Attribute selectors** - `[attr=value]` and its seven operator variants (substring, prefix, suffix, whitespace-list, and so on).
- **Combinators** - `div > button`, `a ~ b`, `a + b`: judging these means navigating between nodes, not judging one node in isolation. `match_compound()` itself still never looks at one - that work has its own owner now, described in "How a complex selector gets judged" below.

None of this is a bug in what exists today - it is work that has a different owner in this project's own plan, not yet built. A style sheet exercising any of the above will not silently do the wrong thing; it will defer, honestly, until the piece that owns that judgment exists.

`:not(...)` and `:scope` anchoring used to be listed here too, as both depending on the combinator work above. That work now exists - see the next section - so a compound carrying either one still comes back `deferred` from `match_compound()` alone (judging either still needs more than one node's own facts), but the `deferred` answer is no longer a dead end: it is resolved one level up, by the piece described next.

Two gaps beyond that division of labor were found while building this piece; one is now resolved, the other still is not:

1. **`:placeholder-shown` has no answer anywhere - RESOLVED by GFSS-SEL-REJECT-ORPHAN (TODO.md, GODS_LAWS.md L-20/L-40), 22/09/2026.** None of the eight facts above speaks to it, and no future matching slice was ever assigned to it either - reporting whether a placeholder is currently shown would need a sixth piece of state this contract does not currently ask the consumer for. The leader's own decision (ESCOPO.md's own "As seis decisoes da manha de 02/09" SS1, verbatim "Recusar na leitura da folha") settled this the OTHER way than the two remaining gaps above will eventually be settled: not by giving `:placeholder-shown` a matching-layer owner, but by having `selector_parse.cpp` refuse a selector that carries it, with a diagnostic (`k_expected_answerable_pseudo_class`), at parse time - before a compound or complex selector carrying it can ever reach `match_compound()`/`match_complex()` at all. **A selector using it no longer parses, full stop; it never reaches "deferred, forever."** The matching-layer code that used to defer it (`compound_match.cpp`'s own `note_pseudo_class_selector()`, `deferred_simple_match.hpp`'s own header comment, this file's own two mentions below) is unchanged and still behaves that way for a `gfss_compound_selector`/`gfss_complex_selector` a caller builds BY HAND rather than through `parse_selector_list()` - defensive code for a value shape the public parser itself no longer produces, not a live code path a style sheet can still reach.
2. **A backslash-escaped identifier in a selector does not match today.** A class written as `.a\:b` in a style sheet is meant to name the class `a:b` (the backslash escaping the colon so the parser doesn't read it as a pseudo-class marker), but the raw escape sequence is not currently decoded before the name reaches the matcher above - and since class names are compared exactly, byte for byte, the escaped selector will never match a node whose class is genuinely `a:b`. This needs a decode step in the tokenizer or the parser before the public selector API is frozen.

## How a complex selector gets judged

*(Internal, same as the section above - nothing here is installed or exported either.)*

A "complex selector" is one or more compounds joined by combinators - `button.primary #ok > a`, for instance, as opposed to a single compound like `button.primary`. The internal `match_complex()` function decides whether a whole complex selector matches one *subject* node, navigating other nodes as its own combinators demand. It answers with the same three-value outcome `match_compound()` does (matched / rejected / deferred), for the same reason: a chain that still carries a pseudo-element anywhere along it is honestly `deferred`, never a guessed answer (a chain carrying `:placeholder-shown` can no longer reach this function at all, gap 1 above - `parse_selector_list()` refuses it first).

**Direction: right to left, with backtrack.** `match_complex()` starts by judging the *subject* - the rightmost compound, the one a style query is actually asking about - and only then walks backward toward the selector's own start. This matters for cost: most nodes in a real tree do not match most rules, so judging the subject first means a selector with no chance of matching a given node never even looks at that node's ancestors. `>` and `+` (child, next sibling) try exactly one candidate - the parent, or the previous sibling - and reject outright if there is none, or if that one candidate's own chain does not work out. The whitespace combinator (descendant) and `~` (subsequent sibling) instead try every candidate along their own axis, nearest first, and only move on to a farther one when the nearer one leads to a dead end - `.a > .b .c` matching a node shaped like `.a > .b > .b > .c` is exactly this: the nearest `.b` is a dead end (its own parent is another `.b`, not `.a`), so the search has to retry with the farther one.

**`:not(...)` is resolved here, recursively.** A compound carrying `:not(s1, s2, ...)` holds at a node when *none* of `s1`, `s2`, ... match that node as their own subject - each argument is itself a full complex selector, run back through `match_complex()` (so `:not()` can itself carry a combinator, e.g. `div:not(.a > .b)`). One counterintuitive case worth naming plainly, because a naive reading gets it wrong: `body :not(table) a` still matches an `a` that is a descendant of a `table`, because `:not(table)` only needs *one* ancestor, at *some* position, that is not itself a table - it does not mean "no ancestor anywhere is a table". Rejecting the whole chain just because a table ancestor exists somewhere would be the bug, not the fix.

**`:scope` anchors to an explicit scope root, or to the tree root.** A query that supplies a scope node makes `:scope` match *only* that exact node - never a different one, even a parent or a sibling. A query with no scope falls back to CSS Selectors 4's own rule: `:scope` is then equivalent to `:root`, holding only for a node with no parent. `:scope` composes normally as the subject of a working chain (`a + :scope`, asking "is this node the scope, and does its previous sibling match `a`") - this is a different question from using `:scope` as the *head* of a selector followed by a sibling combinator (`:scope + a`), which a later piece of this project (the folha-de-estilo dead-selector check) treats as a selector that can never match and warns about; `match_complex()` itself does not special-case that shape.

**What still defers through here too, on purpose:** every pseudo-element (`::before`, `::after` - fabricating the box they denote is layout's own future job). A chain carrying one, anywhere, still comes back `deferred` from `match_complex()`, never a guessed `matched`. `:placeholder-shown` used to defer through here too, until gap 1 above closed by having the parser refuse it instead - a chain carrying it can no longer be built through `parse_selector_list()` at all.

## Where to look for the code itself

The public contract: `include/glintfx/gfui/node_view.hpp`. The internal pieces referenced above: `src/gfui/node_query.hpp` (one-line forwarders from a `gltfx_node_view` to the fact it answers, plus class-membership as a derived query), `src/gfui/compound_match.hpp`/`.cpp` (the algorithm behind `match_compound()`), `src/gfui/state_pseudo_class_table.hpp` (the table mapping a pseudo-class's spelling in a style sheet, such as `"focus-visible"`, to the state bit it asks about), `src/gfui/match_verdict.hpp` (the shared matched/rejected/deferred answer both match levels speak), `src/gfui/combinator_step.hpp`/`.cpp` (given a combinator and a node, the next candidate along its own axis), `src/gfui/deferred_simple_match.hpp`/`.cpp` (`:not()` and `:scope`, the two deferred simple selectors this level owns), and `src/gfui/complex_match.hpp`/`.cpp` (the algorithm behind `match_complex()` itself).
