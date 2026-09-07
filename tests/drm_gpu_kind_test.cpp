// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <print>
#include <string>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/wayland/drm_gpu_kind.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// drm_gpu_kind_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec.
// 4.4; docs/plano-w6b-fatias-5b-revisao.md sec. 3/4.3, D-W6b-30/38):
// the closed, 27-cell enumeration for classify_drm_gpu() - every
// driver name this fatia knows a kernel question for, every answer
// that question can give, and every name it does not (GODS_LAWS.md
// L-40: a non-empty scan, printed, never a sample).
//
// RED, SEEN: before drm_gpu_kind.{hpp,cpp} existed, this file's own
// #include line failed to compile.

using glintfx::gltfx_gpu_kind;
using glintfx::platform::classify_drm_gpu;

namespace {

struct drm_cell {
    std::string_view label;
    drm_device_facts facts;
    gltfx_gpu_kind expected = gltfx_gpu_kind::unknown;
};

drm_device_facts make_facts(bool opened, bool query_ok, std::string driver,
                            bool has_device_local_memory = false, bool amdgpu_fusion = false,
                            int nouveau_platform = -1, int nouveau_bus_type = -1) {
    drm_device_facts facts;
    facts.opened = opened;
    facts.query_ok = query_ok;
    facts.driver = std::move(driver);
    facts.has_device_local_memory = has_device_local_memory;
    facts.amdgpu_fusion = amdgpu_fusion;
    facts.nouveau_platform = nouveau_platform;
    facts.nouveau_bus_type = nouveau_bus_type;
    return facts;
}

} // namespace

GLINTFX_TEST(classify_drm_gpu_closed_27_cell_enumeration) {
    const std::array<drm_cell, 27> cells{{
        {"not_opened", make_facts(false, false, ""), gltfx_gpu_kind::unknown},
        {"opened_query_failed", make_facts(true, false, "i915"), gltfx_gpu_kind::unknown},
        {"amdgpu_fusion", make_facts(true, true, "amdgpu", false, true), gltfx_gpu_kind::shared},
        {"amdgpu_discrete", make_facts(true, true, "amdgpu", false, false),
         gltfx_gpu_kind::dedicated},
        {"i915_no_local_memory", make_facts(true, true, "i915", false), gltfx_gpu_kind::shared},
        {"i915_local_memory", make_facts(true, true, "i915", true), gltfx_gpu_kind::dedicated},
        {"xe_no_local_memory", make_facts(true, true, "xe", false), gltfx_gpu_kind::shared},
        {"xe_local_memory", make_facts(true, true, "xe", true), gltfx_gpu_kind::dedicated},
        {"nvidia_drm_proprietary", make_facts(true, true, "nvidia-drm"), gltfx_gpu_kind::unknown},
        {"nvidia_pci_sysfs_name_wrong_table", make_facts(true, true, "nvidia"),
         gltfx_gpu_kind::unknown},
        {"nouveau_platform_igp", make_facts(true, true, "nouveau", false, false, 0x00),
         gltfx_gpu_kind::shared},
        {"nouveau_platform_pci", make_facts(true, true, "nouveau", false, false, 0x01),
         gltfx_gpu_kind::dedicated},
        {"nouveau_platform_agp", make_facts(true, true, "nouveau", false, false, 0x02),
         gltfx_gpu_kind::dedicated},
        {"nouveau_platform_pcie", make_facts(true, true, "nouveau", false, false, 0x03),
         gltfx_gpu_kind::dedicated},
        {"nouveau_platform_soc", make_facts(true, true, "nouveau", false, false, 0x04),
         gltfx_gpu_kind::shared},
        {"nouveau_platform_out_of_table_bus_type_unconsulted",
         make_facts(true, true, "nouveau", false, false, 9, -1), gltfx_gpu_kind::unknown},
        {"nouveau_platform_failed_bus_type_agp",
         make_facts(true, true, "nouveau", false, false, -1, 0), gltfx_gpu_kind::dedicated},
        {"nouveau_platform_failed_bus_type_pci",
         make_facts(true, true, "nouveau", false, false, -1, 1), gltfx_gpu_kind::dedicated},
        {"nouveau_platform_failed_bus_type_pcie",
         make_facts(true, true, "nouveau", false, false, -1, 2), gltfx_gpu_kind::dedicated},
        {"nouveau_platform_failed_bus_type_soc",
         make_facts(true, true, "nouveau", false, false, -1, 3), gltfx_gpu_kind::shared},
        {"nouveau_platform_failed_bus_type_out_of_table",
         make_facts(true, true, "nouveau", false, false, -1, 7), gltfx_gpu_kind::unknown},
        {"nouveau_both_unconsulted", make_facts(true, true, "nouveau", false, false, -1, -1),
         gltfx_gpu_kind::unknown},
        {"nova_new_rust_driver", make_facts(true, true, "nova"), gltfx_gpu_kind::unknown},
        {"tegra", make_facts(true, true, "tegra"), gltfx_gpu_kind::unknown},
        {"virtio", make_facts(true, true, "virtio"), gltfx_gpu_kind::unknown},
        {"vc4", make_facts(true, true, "vc4"), gltfx_gpu_kind::unknown},
        {"msm", make_facts(true, true, "msm"), gltfx_gpu_kind::unknown},
    }};

    int analyzed = 0;
    for (const drm_cell &cell : cells) {
        const gltfx_gpu_kind result = classify_drm_gpu(cell.facts);
        GLINTFX_CHECK(result == cell.expected);
        ++analyzed;
    }
    GLINTFX_CHECK_EQ(analyzed, static_cast<int>(cells.size()));
    std::println("drm_gpu_kind_test: {} celula(s) conferida(s)", analyzed);
}
