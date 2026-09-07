// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/dxcore_gpu_kind.hpp"

namespace glintfx::platform {

gltfx_gpu_kind classify_dxcore_gpu(std::span<const dxcore_adapter_facts> facts,
                                   std::optional<std::size_t> matched) noexcept {
    if (!matched.has_value() || *matched >= facts.size()) {
        return gltfx_gpu_kind::unknown;
    }

    const dxcore_adapter_facts &adapter = facts[*matched];

    if (!adapter.hardware_supported) {
        return gltfx_gpu_kind::unknown;
    }

    if (!adapter.is_hardware) {
        return gltfx_gpu_kind::software; // software vence, mesmo se "integrated" também respondeu
    }

    if (!adapter.integrated_supported) {
        return gltfx_gpu_kind::unknown;
    }

    return adapter.is_integrated ? gltfx_gpu_kind::shared : gltfx_gpu_kind::dedicated;
}

} // namespace glintfx::platform
