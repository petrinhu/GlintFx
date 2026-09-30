// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/port/power_source.hpp"

// platform/gl/auto_preset_rule.hpp - GFX-PRESET, fatia P1
// (docs/plano-w6b-fatias-5.md sec. 5.1, D-W6b-34, D-W6b-44): the automatic rule
// that turns the two facts the system reports - the kind of GPU this context
// runs on and where the power comes from - into ONE suggestion: a concrete
// preset and the reason. PURE: no allocation, no system call, the same answer
// for the same two inputs. It only ever SUGGESTS (the leader's order of
// 06/09/2026: the automatic choice hands over the best values for the consumer
// to use on their own side; it locks nothing) - applying is always the
// consumer's own request.
//
// THE RULE, exactly (12 cells: 4 GPU kinds x 3 power sources), in this order:
//   1. `battery`                        -> power_saving / on_battery (whatever
//   the GPU)
//   2. else `dedicated`                 -> performance  / dedicated_gpu
//   3. else `software`                  -> balanced     / software_renderer
//   4. else `shared`                    -> balanced     / shared_gpu
//   5. else `unknown`                   -> balanced     / unknown_gpu
// The reason is NEVER `none`: every suggestion has one. `none` stays in the
// vocabulary only because the vocabulary is append-only.
//
// The preset and reason numbers are the public constants k_gltfx_preset_* and
// k_gltfx_auto_choice_reason_* of gfx_option.hpp: a DATA contract, append-only.

namespace glintfx::platform {

struct auto_preset_choice {
    std::int64_t preset;
    std::int64_t reason;
};

[[nodiscard]] auto_preset_choice choose_preset_automatically(gltfx_gpu_kind kind,
                                                             gltfx_power_source power) noexcept;

} // namespace glintfx::platform
