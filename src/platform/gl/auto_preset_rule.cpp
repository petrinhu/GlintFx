// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/auto_preset_rule.hpp"

namespace glintfx::platform {

auto_preset_choice choose_preset_automatically(gltfx_gpu_kind kind,
                                               gltfx_power_source power) noexcept {
    if (power == gltfx_power_source::battery) {
        return {k_gltfx_preset_power_saving, k_gltfx_auto_choice_reason_on_battery};
    }
    switch (kind) {
    case gltfx_gpu_kind::dedicated:
        return {k_gltfx_preset_performance, k_gltfx_auto_choice_reason_dedicated_gpu};
    case gltfx_gpu_kind::software:
        return {k_gltfx_preset_balanced, k_gltfx_auto_choice_reason_software_renderer};
    case gltfx_gpu_kind::shared:
        return {k_gltfx_preset_balanced, k_gltfx_auto_choice_reason_shared_gpu};
    case gltfx_gpu_kind::unknown:
        break;
    }
    return {k_gltfx_preset_balanced, k_gltfx_auto_choice_reason_unknown_gpu};
}

} // namespace glintfx::platform
