// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

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

} // namespace glintfx::gfui
