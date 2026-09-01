// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <glintfx/export.hpp>

// gfui/node_view.hpp - GFSS-NODE-VIEW (TODO.md, GODS_LAWS.md L-17/
// L-19/L-20/L-22/L-26/L-28/L-40, ESCOPO.md SS2 decision 6, ratified by
// the project leader verbatim "aceito tudo" after asking to see the
// list first): the contract by which the motor asks a consumer's
// alien tree exactly eight facts about one node - no more, no less,
// nothing renamed in meaning (see ESCOPO.md SS2 decision 6's own
// table for the eight, and /var/tmp/glintfx-plan/gfss-node-view-
// plano.md SS1 for the ten-entry breakdown this header implements).
// [PMU] - this is the ONLY contract the CONSUMER implements: a field
// added later breaks every consumer that already filled the table
// (the aggregate gains a new required member) the moment they upgrade
// the .so.
//
// WHY gfui, NOT gfss (D-NV-1 of the plan above): ESCOPO.md SS4 fixes
// the trio - gfss is the leaf FORMAT (data), gfml the markup (data),
// gfui the motor (code, exported symbol). A node's view is not a
// piece of the format; it is the contract by which the motor SEES a
// consumer's tree - motor, not data - so it lives in its own gfui/
// module, never bolted onto gfss/ "because it fits" (GODS_LAWS.md
// L-17).
//
// WHY A TABLE OF FUNCTION POINTERS, NOT A TEMPLATE OR A VIRTUAL BASE
// (plano SS2.3/SS3.5 item 2): ESCOPO.md SS4 decision 1 (21/08/2026)
// bans a template on the public surface ("contrato preenchido pelo
// consumidor, sem template na superficie publica") - the CONSUMER's
// tree type is unknown at the moment this .so is compiled, and is
// picked at RUNTIME by code this library has never seen, so the
// indirection has to be a runtime one. A virtual base is out too
// (GODS_LAWS.md L-19 item 3: no vtable on a public type crosses the
// ABI boundary here). The industry-standard non-owning, non-
// allocating, no-RTTI, no-exception type-erasure shape for exactly
// this problem is `void*` plus a table of function pointers (learned
// from Servo's own `selectors::Element` trait and from P0792/"Type
// erasure: void*" - GODS_LAWS.md L-29: technique learned, nothing
// copied, the outputs below differ from every source read) - THAT is
// gltfx_node_facts below.

namespace glintfx::gfui {

// GLINTFX_NODE_STATE_LIST(X) - the closed set of the five pseudo-
// class bits ESCOPO.md SS2 decision 6's fact 5 names ("mouse em cima,
// sendo clicado, com foco, com foco por teclado, marcado"): hover,
// active, focus, focus_visible, checked. Each entry carries its own
// bit SHIFT (not a sequential enumerator value like gfss/value.hpp's
// X-macro lists use) because these five have to combine as flags in
// ONE gltfx_node_state answered by a SINGLE `state` callback (plano
// SS3, fact 5's own callback table entry: "um chamado, cinco bits") -
// private to this header, defined and #undef'd right after generating
// the enum, the count and the first_missing() if-chain below (this
// header's own three uses).
#define GLINTFX_NODE_STATE_LIST(X)                                                                 \
    X(hover, 0)                                                                                    \
    X(active, 1)                                                                                   \
    X(focus, 2)                                                                                    \
    X(focus_visible, 3)                                                                            \
    X(checked, 4)

enum class gltfx_node_state : std::uint8_t {
    none = 0,
#define GLINTFX_NODE_STATE_ENUMERATOR(name, shift) name = (1U << (shift)),
    GLINTFX_NODE_STATE_LIST(GLINTFX_NODE_STATE_ENUMERATOR)
#undef GLINTFX_NODE_STATE_ENUMERATOR
};

// THE FIVE BIT VALUES CONGEAL HERE, NOT JUST IN A COMMENT (plano
// tabela SS4, "a mais perigosa": an old binary reading `hover` where a
// new one writes `focus` compiles clean and fails silently - the
// exact same danger class as the color decision 42 the plan's own
// table cites). A reorder of GLINTFX_NODE_STATE_LIST above that kept
// every name but changed a shift number would still compile; these
// five static_asserts are what turns that into a build failure
// instead of a silent ABI break.
static_assert(static_cast<std::uint8_t>(gltfx_node_state::hover) == 1U,
              "GODS_LAWS.md L-26: gltfx_node_state::hover's bit value is frozen ABI, never renumbered");
static_assert(static_cast<std::uint8_t>(gltfx_node_state::active) == 2U,
              "GODS_LAWS.md L-26: gltfx_node_state::active's bit value is frozen ABI, never renumbered");
static_assert(static_cast<std::uint8_t>(gltfx_node_state::focus) == 4U,
              "GODS_LAWS.md L-26: gltfx_node_state::focus's bit value is frozen ABI, never renumbered");
static_assert(static_cast<std::uint8_t>(gltfx_node_state::focus_visible) == 8U,
              "GODS_LAWS.md L-26: gltfx_node_state::focus_visible's bit value is frozen ABI, never renumbered");
static_assert(static_cast<std::uint8_t>(gltfx_node_state::checked) == 16U,
              "GODS_LAWS.md L-26: gltfx_node_state::checked's bit value is frozen ABI, never renumbered");
static_assert(sizeof(gltfx_node_state) == 1U,
              "GODS_LAWS.md L-26: gltfx_node_state has to stay a single byte on the wire, on every "
              "platform this library ships for");

// Mechanically counted from GLINTFX_NODE_STATE_LIST above - never a
// hand-copied literal (same GODS_LAWS.md L-40 discipline gfss/
// value.hpp's own gltfx_gfss_value_kind_count already established): a
// sixth bit added to the list without a matching row in node_state.
// cpp's own table fails the build there, not silently.
inline constexpr std::size_t gltfx_node_state_count = [] {
    std::size_t count = 0;
#define GLINTFX_NODE_STATE_COUNT_ONE(name, shift) ++count;
    GLINTFX_NODE_STATE_LIST(GLINTFX_NODE_STATE_COUNT_ONE)
#undef GLINTFX_NODE_STATE_COUNT_ONE
    return count;
}();

#undef GLINTFX_NODE_STATE_LIST

// Returns the IDENTIFIER of the single bit `one_bit` names (e.g.
// "focus_visible"), never a sentence (docs/api-conventions.md R7).
// Defined in node_state.cpp, alongside a hand-authored, static_assert-
// guarded table - the SAME "add a bit without registering it, watch
// it fail to compile" proof gfss/value_kind.cpp's own tables already
// give this project (GODS_LAWS.md L-40). noexcept, never undefined
// behavior: `none`, a combination of more than one bit, or any value
// outside the five named ones returns "unknown" - the SAME fallback
// gltfx_gfss_token_kind_name()/gltfx_err_code_name() already use.
[[nodiscard]] GLINTFX_API std::string_view gltfx_node_state_name(gltfx_node_state one_bit) noexcept;

// Bit test: true iff every bit set in `bit` is also set in `flags`.
// constexpr and inline (nothing to hide behind the .so boundary for a
// single bitwise-and-and-compare), the SAME shape a consumer would
// write by hand for `flags & bit == bit` - this just names it so
// nobody has to remember the underlying_type cast at every call site.
[[nodiscard]] constexpr bool gltfx_node_state_has(gltfx_node_state flags, gltfx_node_state bit) noexcept {
    const auto flags_bits = static_cast<std::uint8_t>(flags);
    const auto bit_bits = static_cast<std::uint8_t>(bit);
    return (flags_bits & bit_bits) == bit_bits;
}

// --- fatia B: the table of ten answers, and the three-pointer view ---
//
// Response to fact 4 ("consultar um atributo pelo nome"). `present ==
// false` means "the node has no such attribute"; `present == true`
// with an empty `value` means "it has one, and the value is empty"
// (`[disabled]` and `[disabled=""]` both match GFSS-MATCH-SIMPLE's own
// future attribute selector, the same distinction R4 already draws
// for `id` below being empty versus absent).
struct gltfx_node_attribute {
    bool present = false;
    std::string_view value;
};

// Visitor of fact 3 ("a lista de classes"). Returns false to STOP the
// enumeration - the consumer's for_each_class implementation MUST
// honor that (plano SS3.4 item 7): calling it once more after it
// returned false is a violation of the consumer's own contract, not
// this library's. Repeating a class name IS tolerated (pertencimento
// e idempotente); omitting one is not.
using gltfx_node_class_visitor_fn = bool (*)(void *visitor_context, std::string_view class_name) noexcept;

// GLINTFX_NODE_FACTS_LIST(X) - the ten CALLBACK ENTRIES that answer
// ESCOPO.md SS2 decision 6's eight facts (facts 7 and 8 are each two
// entries - "quem e o irmao anterior e o seguinte" and "quantos filhos
// tem e qual e o primeiro"). Used below to derive gltfx_node_facts_
// entry_count and the if-chain inside gltfx_node_facts_first_missing()
// - the struct's own fields are hand-declared right after (their
// types differ per entry, so a single X(name) cannot generate a
// uniform field declaration the way GLINTFX_NODE_STATE_LIST above
// could) - private to this header, #undef'd once both uses are done.
#define GLINTFX_NODE_FACTS_LIST(X)                                                                 \
    X(tag_name)                                                                                    \
    X(id)                                                                                          \
    X(for_each_class)                                                                              \
    X(attribute)                                                                                   \
    X(state)                                                                                       \
    X(parent)                                                                                       \
    X(previous_sibling)                                                                            \
    X(next_sibling)                                                                                \
    X(child_count)                                                                                 \
    X(first_child)

// Mechanically counted from GLINTFX_NODE_FACTS_LIST above - never a
// hand-copied literal (GODS_LAWS.md L-40, same discipline as gltfx_
// node_state_count above).
inline constexpr std::size_t gltfx_node_facts_entry_count = [] {
    std::size_t count = 0;
#define GLINTFX_NODE_FACTS_COUNT_ONE(name) ++count;
    GLINTFX_NODE_FACTS_LIST(GLINTFX_NODE_FACTS_COUNT_ONE)
#undef GLINTFX_NODE_FACTS_COUNT_ONE
    return count;
}();

// Entry i (0-based, SAME order as GLINTFX_NODE_FACTS_LIST above)
// answers WHICH of ESCOPO.md SS2 decision 6's eight facts it belongs
// to - facts 7 and 8 are each two entries, every other fact is one.
// This is the data gltfx_node_facts_fact_count below counts DISTINCT
// values from, instead of a hand-typed literal 8. A raw array, not
// std::array: this header's own layer discipline (SS3.5 item 1 of the
// plan this fatia follows) keeps its include list to <cstddef>/
// <cstdint>/<string_view>/<type_traits>/glintfx/export.hpp only - a
// fixed-size C array needs none of those added.
inline constexpr std::uint8_t k_node_facts_entry_fact_number[10] = {
    1, 2, 3, 4, 5, 6, 7, 7, 8, 8,
};

// Distinct fact count, computed FROM k_node_facts_entry_fact_number
// above, never a hand-typed literal 8 (GODS_LAWS.md L-40): an entry
// added with the wrong fact number, or a ninth fact opened by mistake,
// changes this number instead of silently disagreeing with ESCOPO.md's
// own eight-fact text.
inline constexpr std::size_t gltfx_node_facts_fact_count = [] {
    bool seen[9] = {}; // index 0 unused; facts are numbered 1..8
    for (std::uint8_t fact_number : k_node_facts_entry_fact_number) {
        seen[fact_number] = true;
    }
    std::size_t count = 0;
    for (std::size_t i = 1; i < 9; ++i) {
        if (seen[i]) {
            ++count;
        }
    }
    return count;
}();

// The table: the ten entries above, ONE PER TYPE OF CONSUMER TREE, all
// noexcept (plano SS3.2: since C++17 noexcept is part of a function's
// TYPE, so a consumer callback that can throw fails to compile here -
// GODS_LAWS.md L-22 enforced by the compiler, not by discipline, on
// both sides of the fence). `const void *tree, const void *node` never
// dereferenced by this library - only handed back to the SAME
// callback that produced them (plano SS3.2: "o motor so le").
//
// FIELD ORDER IS FROZEN ABI (plano tabela SS4): reordering shifts
// every subsequent field's offset silently in a consumer that only
// swapped the .so - see the ten offsetof static_asserts right after
// this struct.
struct gltfx_node_facts {
    std::string_view (*tag_name)(const void *tree, const void *node) noexcept = nullptr; // fact 1
    std::string_view (*id)(const void *tree, const void *node) noexcept = nullptr;       // fact 2 (empty = none)
    void (*for_each_class)(const void *tree, const void *node, gltfx_node_class_visitor_fn visit,
                            void *visitor_context) noexcept = nullptr; // fact 3
    gltfx_node_attribute (*attribute)(const void *tree, const void *node,
                                       std::string_view name) noexcept = nullptr; // fact 4
    gltfx_node_state (*state)(const void *tree, const void *node) noexcept = nullptr; // fact 5
    const void *(*parent)(const void *tree, const void *node) noexcept = nullptr;     // fact 6 (nullptr = root)
    const void *(*previous_sibling)(const void *tree, const void *node) noexcept = nullptr; // fact 7
    const void *(*next_sibling)(const void *tree, const void *node) noexcept = nullptr;     // fact 7
    std::size_t (*child_count)(const void *tree, const void *node) noexcept = nullptr;      // fact 8
    const void *(*first_child)(const void *tree, const void *node) noexcept = nullptr; // fact 8 (nullptr = none)
};

// Ten offsetof static_asserts, one per field, in declaration order -
// the mechanical guard against a silent reorder (GODS_LAWS.md L-26,
// same danger class the bit-value static_asserts above already guard
// for gltfx_node_state). Multiplied by sizeof(gltfx_node_facts::tag_
// name) rather than sizeof(void*): the standard does not guarantee
// every function-pointer TYPE has the same size as every other, even
// though it is true in practice on every ABI this library ships for
// (SysV x86-64, Windows x64, aarch64 - all pointer-sized) - using one
// of the struct's own member types as the unit keeps the assertion
// correct even if that practical fact ever stopped holding on some
// future target, instead of silently trusting it.
static_assert(offsetof(gltfx_node_facts, tag_name) == 0 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, id) == 1 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, for_each_class) == 2 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, attribute) == 3 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, state) == 4 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, parent) == 5 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, previous_sibling) == 6 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, next_sibling) == 7 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, child_count) == 8 * sizeof(gltfx_node_facts::tag_name));
static_assert(offsetof(gltfx_node_facts, first_child) == 9 * sizeof(gltfx_node_facts::tag_name));

static_assert(sizeof(gltfx_node_facts) == gltfx_node_facts_entry_count * sizeof(gltfx_node_facts::tag_name),
              "GODS_LAWS.md L-19 item 3: gltfx_node_facts is exactly ten function pointers, no padding, "
              "no hidden field");
static_assert(std::is_trivially_copyable_v<gltfx_node_facts>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_facts is a value type, safe to copy across the ABI "
              "boundary");
static_assert(std::is_standard_layout_v<gltfx_node_facts>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_facts's layout is the contract itself");

// The view of ONE node: what the motor receives and passes onward.
// Three pointers, by value - `node == nullptr` means "no node" (what
// navigation returns on hitting the root or the end of the sibling
// row). `tree` carries whatever context the consumer's own callbacks
// need (an arena, a document handle) - it may itself be nullptr for a
// consumer whose node pointers are already self-sufficient (plano
// D-NV-4: the alternative, a two-pointer view, would force an arena-
// backed consumer to store a back-pointer to the arena in every one of
// its own nodes - a requirement outside the eight facts).
struct gltfx_node_view {
    const gltfx_node_facts *facts = nullptr;
    const void *tree = nullptr;
    const void *node = nullptr;
};

static_assert(sizeof(gltfx_node_view) == 3 * sizeof(void *),
              "GODS_LAWS.md L-19 item 3: gltfx_node_view is exactly three pointers, no padding");
static_assert(std::is_trivially_copyable_v<gltfx_node_view>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_view is a value type, safe to copy across the ABI "
              "boundary");
static_assert(std::is_standard_layout_v<gltfx_node_view>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_view's layout is the contract itself");
static_assert(std::is_trivially_copyable_v<gltfx_node_attribute>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_attribute is a value type, safe to copy across the "
              "ABI boundary");
static_assert(std::is_standard_layout_v<gltfx_node_attribute>,
              "GODS_LAWS.md L-19 item 3: gltfx_node_attribute's layout is the contract itself");

// Diagnostic of an incomplete table: returns the NAME of the first
// null entry ("previous_sibling"), or empty if the table is complete -
// a token, never a sentence (docs/api-conventions.md R7). Inline:
// nothing crosses the .so/.dll boundary for this. The ten checks below
// are generated from GLINTFX_NODE_FACTS_LIST above, one `if` per
// entry, in declaration order (GODS_LAWS.md L-17: the whole function
// body is exactly ten one-line ifs, none of them exceeding the 40-line
// budget by a wide margin).
#define GLINTFX_NODE_FACTS_FIRST_MISSING_CHECK(name)                                               \
    if (facts.name == nullptr) {                                                                  \
        return #name;                                                                              \
    }

[[nodiscard]] inline std::string_view gltfx_node_facts_first_missing(const gltfx_node_facts &facts) noexcept {
    GLINTFX_NODE_FACTS_LIST(GLINTFX_NODE_FACTS_FIRST_MISSING_CHECK)
    return {};
}

#undef GLINTFX_NODE_FACTS_FIRST_MISSING_CHECK
#undef GLINTFX_NODE_FACTS_LIST

} // namespace glintfx::gfui
