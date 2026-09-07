// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/drm_gpu_kind.hpp"

namespace glintfx::platform {

namespace {

// nouveau's own NVIF `platform` values (revisao.md sec. 1.6 N4,
// cl0080.h): IGP is the integrated case, SOC is Tegra - both share
// system memory; PCI/AGP/PCIE sit in a slot with their own VRAM.
constexpr int k_nouveau_platform_igp = 0x00;
constexpr int k_nouveau_platform_pci = 0x01;
constexpr int k_nouveau_platform_agp = 0x02;
constexpr int k_nouveau_platform_pcie = 0x03;
constexpr int k_nouveau_platform_soc = 0x04;

// The flatter GETPARAM_BUS_TYPE fallback (revisao.md sec. 1.1 L7): the
// SAME four buses, DIFFERENT numbers, and IGP already collapsed by the
// kernel itself into PCI or PCIE depending on pci_is_pcie() - only SOC
// survives distinctly (sec. 1.6 N4's own "perde a distincao integrada
// para chipsets antigos").
constexpr int k_nouveau_bus_type_agp = 0;
constexpr int k_nouveau_bus_type_pci = 1;
constexpr int k_nouveau_bus_type_pcie = 2;
constexpr int k_nouveau_bus_type_soc = 3;

[[nodiscard]] gltfx_gpu_kind classify_nouveau(const drm_device_facts &facts) noexcept {
    switch (facts.nouveau_platform) {
    case k_nouveau_platform_igp:
    case k_nouveau_platform_soc:
        return gltfx_gpu_kind::shared;
    case k_nouveau_platform_pci:
    case k_nouveau_platform_agp:
    case k_nouveau_platform_pcie:
        return gltfx_gpu_kind::dedicated;
    default:
        break; // -1 (not consulted) or a value outside the table
    }

    switch (facts.nouveau_bus_type) {
    case k_nouveau_bus_type_soc:
        return gltfx_gpu_kind::shared;
    case k_nouveau_bus_type_agp:
    case k_nouveau_bus_type_pci:
    case k_nouveau_bus_type_pcie:
        return gltfx_gpu_kind::dedicated;
    default:
        break; // -1 (not consulted) or a value outside the table
    }

    return gltfx_gpu_kind::unknown; // both kernel questions failed - vias take over
}

} // namespace

gltfx_gpu_kind classify_drm_gpu(const drm_device_facts &facts) noexcept {
    if (!facts.opened || !facts.query_ok) {
        return gltfx_gpu_kind::unknown;
    }

    if (facts.driver == "amdgpu") {
        return facts.amdgpu_fusion ? gltfx_gpu_kind::shared : gltfx_gpu_kind::dedicated;
    }

    if (facts.driver == "i915" || facts.driver == "xe") {
        return facts.has_device_local_memory ? gltfx_gpu_kind::dedicated : gltfx_gpu_kind::shared;
    }

    if (facts.driver == "nouveau") {
        return classify_nouveau(facts);
    }

    // "nvidia-drm" (the kernel names the bus/model, never the class -
    // revisao.md sec. 1.1 L6), "nova" (sec. 1.6 N6, no query of its
    // own yet), the PCI sysfs name "nvidia" (wrong table, F3's own
    // correction), and every driver this project has not written a
    // question for - `unknown`, declared, never chuted (D-W6b-30).
    return gltfx_gpu_kind::unknown;
}

} // namespace glintfx::platform
