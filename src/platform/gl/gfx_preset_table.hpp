// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_option_values.hpp"

// platform/gl/gfx_preset_table.hpp - GFX-PRESET, fatia P1
// (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-34,
// /var/tmp/cto-w7d/PLANO-errata.md D-P1-2): the ONE table of what each concrete
// preset sets, and the two accessors the public
// gltfx_gfx_preset_row_count()/gltfx_gfx_preset_row_at() (P3) build on. PURE
// and header-light: no allocation anywhere (R3, degrau 1: a fixed std::array
// read through a std::span), no system call.
//
// THE NUMBERS are the ones of gfx_option_values.hpp (D-P1-5): one internal
// header, which P3 deletes when the public k_gltfx_* constants exist. They are
// a DATA contract (a consumer's saved settings file stores them), append-only,
// never renumbered.
//
// THE ROWS (D-W6b-34): power_saving = {vsync on, frame_rate_cap 30}; balanced =
// {vsync on, frame_rate_cap 0}; performance = {vsync on, frame_rate_cap 0}.
// balanced and performance hold the SAME rows today, on purpose: the only rows
// a preset touches now are vsync and frame_rate_cap, and turning vsync off
// trades tearing for speed, a separate choice from "performance". They start to
// differ when the library grows options that can be spent on quality.
// gpu_preference is NOT in any row (only no_preference is honored, D-W6b-14).

namespace glintfx::platform {

struct gfx_preset_row {
    std::int64_t preset;
    gltfx_gfx_option id;
    std::int64_t value;
};

// Rows of one preset are CONTIGUOUS, and in the order they are applied.
inline constexpr std::array<gfx_preset_row, 6> k_gfx_preset_table{{
    {k_preset_power_saving, gltfx_gfx_option::vsync, k_vsync_on},
    {k_preset_power_saving, gltfx_gfx_option::frame_rate_cap, 30},
    {k_preset_balanced, gltfx_gfx_option::vsync, k_vsync_on},
    {k_preset_balanced, gltfx_gfx_option::frame_rate_cap, 0},
    {k_preset_performance, gltfx_gfx_option::vsync, k_vsync_on},
    {k_preset_performance, gltfx_gfx_option::frame_rate_cap, 0},
}};

// "Contiguous" is what gfx_preset_rows() relies on (it takes ONE run of rows), so it is proved at
// compile time, not left to a test that has to run: a preset whose rows were split by another
// preset's rows would silently lose the second run.
[[nodiscard]] constexpr bool gfx_preset_rows_are_contiguous(
    const std::array<gfx_preset_row, k_gfx_preset_table.size()> &table) noexcept {
    for (std::size_t i = 1; i < table.size(); ++i) {
        if (table[i].preset == table[i - 1].preset) {
            continue;
        }
        for (std::size_t earlier = 0; earlier < i; ++earlier) {
            if (table[earlier].preset == table[i].preset) {
                return false;
            }
        }
    }
    return true;
}
static_assert(gfx_preset_rows_are_contiguous(k_gfx_preset_table),
              "the rows of one preset must be contiguous in k_gfx_preset_table");

// The rows of `preset`. Empty for manual, for automatic (it resolves to a
// concrete preset first) and for any number outside the vocabulary.
[[nodiscard]] std::span<const gfx_preset_row> gfx_preset_rows(std::int64_t preset) noexcept;

// How many rows `preset` sets, in THIS build (zero exactly when
// gfx_preset_rows() is empty).
[[nodiscard]] std::size_t gfx_preset_row_count(std::int64_t preset) noexcept;

// The row at `row_index`, as the {option, value} entry it applies. Out of range
// - the preset outside the vocabulary or the index past the count - is ABSENCE:
// std::nullopt. This atom does not degrade: the public
// gltfx_gfx_preset_row_at() (P3) owns that policy (docs/api-conventions.md R4,
// PLANO-errata.md E1: {suggested_preset, manual}, an id that is read_only, so
// handed to set_option() by mistake it is refused and changes nothing), where
// the public names exist.
[[nodiscard]] std::optional<gltfx_gfx_option_entry>
gfx_preset_row_at(std::int64_t preset, std::size_t row_index) noexcept;

} // namespace glintfx::platform
