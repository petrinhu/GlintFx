// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_option_registry.hpp"
#include "platform/gl/gfx_preset_table.hpp"

// platform/gl/preset_expansion.hpp - GFX-PRESET, fatia P1
// (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-35 rule 5,
// /var/tmp/cto-w7d/PLANO-errata.md D-P1-3): what a concrete preset means for
// ONE opening list or ONE set_option() call. PURE, and WITHOUT ALLOCATION AND
// WITHOUT A FAILURE PATH (GODS_LAWS.md L-22, docs/api-conventions.md R3, degrau
// 1): the result is a value with a FIXED capacity - one entry per option of the
// registry - so nothing can be too small. The draft returned a std::vector from
// a `noexcept` function, which would let std::bad_alloc kill the consumer's
// process; noexcept_alloc_test is the proof, with no line added to its
// exceptions file.
//
// THE RESULT: the rows of `preset` whose option the consumer did NOT set
// explicitly, in table order, THEN every explicit entry, in the order given.
// The explicit entry wins: a row the consumer named is never applied from the
// table. `manual`, `automatic` and any number outside the vocabulary have no
// rows, so only the explicit entries come out (the caller resolves `automatic`
// to a concrete preset before calling).
//
// IMMUNE TO THE CUT BY CONSTRUCTION (D-P1-4): the result holds AT MOST ONE
// entry per option, so the fixed capacity - one entry per option of the
// registry - can never overflow for any option the registry knows, and the
// atom's correctness does not depend on another unit validating the list first
// (the facade still calls validate_gl_context_desc() before expanding and
// proves the refusal, P3; that is a second line, not this one). When the
// explicit list names the SAME option more than once, the FIRST occurrence wins
// - the rule gl_context_desc_validation.cpp already applies. The static_assert
// below ties the capacity to the rows of the table.

namespace glintfx::platform {

inline constexpr std::size_t k_preset_expansion_capacity = k_gfx_option_table.size();

struct preset_expansion {
    std::array<gltfx_gfx_option_entry, k_preset_expansion_capacity> entries{};
    std::size_t count = 0;
};

static_assert(
    [] {
        std::size_t most = 0;
        std::size_t run = 0;
        std::int64_t previous = -1;
        for (const gfx_preset_row &row : k_gfx_preset_table) {
            run = (row.preset == previous) ? run + 1 : 1;
            previous = row.preset;
            most = (run > most) ? run : most;
        }
        return most;
    }() <= k_preset_expansion_capacity,
    "a preset with more rows than the registry has options cannot be expanded "
    "into the fixed "
    "capacity of preset_expansion");

[[nodiscard]] preset_expansion
expand_preset(std::int64_t preset,
              std::span<const gltfx_gfx_option_entry> explicit_entries) noexcept;

} // namespace glintfx::platform
