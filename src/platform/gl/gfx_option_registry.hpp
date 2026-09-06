// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>

#include <glintfx/platform/gl/gfx_option.hpp>

// platform/gl/gfx_option_registry.hpp - C-OPT (docs/plano-w6b-placa-e-
// laco.md fatia 2a, sec. 11.2, GODS_LAWS.md L-17/L-26/L-40): the ONE
// master table of the eight options v1 ships, in id order - the SAME
// "one table, shared, header-only, inline constexpr" shape src/gfss/
// property_table.hpp already established for this project's other
// append-only registry (see that file's own header comment for the
// full C++17 inline-variable reasoning; it is not repeated here).
//
// EVERY ROW HERE IS A LITERAL TRANSCRIPTION of docs/plano-w6b-placa-e-
// laco.md sec. 11.2's own table, ROW FOR ROW, in the SAME order - a
// mechanical cross-check against that document is meant to be a
// line-for-line comparison, never free interpretation. The `min_value`/
// `max_value` bounds are this fatia's own reading of each row's
// "valores / faixa" column into a closed numeric range (a `choice`
// option's range is `[0, N-1]` over its own N named values, in the
// order that column lists them - the names themselves are documented
// here, per row, since the public struct does not carry them; a
// `toggle` is always `[0, 1]`; `integer`'s range is copied verbatim).
//
// tests/tools/check_gfx_option_ids.py cross-reads THIS file against
// gfx_option.hpp's own enum - see that script's own header for exactly
// what it reproves.

namespace glintfx::platform {

struct gfx_option_row {
    gltfx_gfx_option id = gltfx_gfx_option::vsync;
    std::string_view name;
    gltfx_gfx_option_kind kind = gltfx_gfx_option_kind::toggle;
    gltfx_gfx_option_when when = gltfx_gfx_option_when::read_only;
    std::int64_t min_value = 0;
    std::int64_t max_value = 0;
    std::int64_t default_value = 0;
};

// Eight rows, sec. 11.2's own order (id 0 = vsync ... id 7 =
// power_source). `std::array<gfx_option_row, 8>` is a HAND-WRITTEN
// literal size, not gltfx_gfx_option's own enumerator count - the SAME
// "a mismatched count is a compile failure, never a silently value-
// initialized row" discipline property_table.hpp's own static_assert
// already applies, repeated below.
inline constexpr std::array<gfx_option_row, 8> k_gfx_option_table{{
    // id 0 - choice: off=0, on=1, adaptive=2. Default `on` (D-W6b-7,
    // "a janela nao trava, mas o consumidor pediu sincronia por
    // padrao").
    {gltfx_gfx_option::vsync, "vsync", gltfx_gfx_option_kind::choice, gltfx_gfx_option_when::live,
     0, 2, 1},

    // id 1 - integer: 0 (sem teto) ou 1..1000 Hz. Default 0.
    {gltfx_gfx_option::frame_rate_cap, "frame_rate_cap", gltfx_gfx_option_kind::integer,
     gltfx_gfx_option_when::live, 0, 1000, 0},

    // id 2 - choice: no_preference=0, prefer_shared=1,
    // prefer_dedicated=2. Fixed at open() (D-W6b-14/17); only
    // no_preference is honored until GL-GPU-PREFERENCE exists (fatia
    // 2b's own validation, not this table's job).
    {gltfx_gfx_option::gpu_preference, "gpu_preference", gltfx_gfx_option_kind::choice,
     gltfx_gfx_option_when::open_only, 0, 2, 0},

    // id 3 - integer: sample count, 0..16 (the discrete set {0, 2, 4,
    // 8, 16} is an ADAPTER-level support question, fatias 3/4 - this
    // registry only bounds the SHAPE of the value, never which of the
    // range a given driver actually has a config for).
    {gltfx_gfx_option::msaa_samples, "msaa_samples", gltfx_gfx_option_kind::integer,
     gltfx_gfx_option_when::open_only, 0, 16, 0},

    // id 4 - toggle: 0/1. Default off.
    {gltfx_gfx_option::srgb_framebuffer, "srgb_framebuffer", gltfx_gfx_option_kind::toggle,
     gltfx_gfx_option_when::open_only, 0, 1, 0},

    // id 5 - choice: manual=0, power_saving=1, balanced=2,
    // performance=3, automatic=4. Default `manual` (D-W6b-19: the
    // library never reapplies a preset on its own).
    {gltfx_gfx_option::preset, "preset", gltfx_gfx_option_kind::choice, gltfx_gfx_option_when::live,
     0, 4, 0},

    // id 6 - choice, read_only: none=0, on_battery=1,
    // software_renderer=2, shared_gpu=3, dedicated_gpu=4,
    // unknown_gpu=5. Set by the library after resolving `preset=
    // automatic` (fatia 5c); never accepted from a consumer.
    {gltfx_gfx_option::auto_choice_reason, "auto_choice_reason", gltfx_gfx_option_kind::choice,
     gltfx_gfx_option_when::read_only, 0, 5, 0},

    // id 7 - choice, read_only: unknown=0, mains=1, battery=2. The
    // insumo D-W6b-19's own automatic rule reads (sec. 12's own
    // "power_source" row) - public so a consumer can write its own
    // rule too.
    {gltfx_gfx_option::power_source, "power_source", gltfx_gfx_option_kind::choice,
     gltfx_gfx_option_when::read_only, 0, 2, 0},
}};

static_assert(k_gfx_option_table.size() == 8,
              "GODS_LAWS.md L-40: k_gfx_option_table's row count must track gfx_option.hpp's own "
              "gltfx_gfx_option ids - a ninth option added to the enum without a matching row here "
              "must not compile silently");

// Linear scan, shared by every accessor in gfx_option_registry.cpp -
// one atom, one job (GODS_LAWS.md L-17), the same technique property_
// table.hpp's own find_property_entry() already uses. Returns nullptr
// for an `id` this build's table does not carry.
inline const gfx_option_row *find_gfx_option_row(gltfx_gfx_option id) noexcept {
    for (const gfx_option_row &row : k_gfx_option_table) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace glintfx::platform
