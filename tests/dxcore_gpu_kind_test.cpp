// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <optional>
#include <print>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/win32/dxcore_gpu_kind.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// dxcore_gpu_kind_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5b-
// revisao.md sec. 3/4.3, D-W6b-37): the closed 8-cell enumeration for
// classify_dxcore_gpu() - not localized, out of range, each
// IsPropertySupported=false, the software executor, the hybrid
// running on the integrated GPU, the single-GPU desktop (the exact
// cell the old DXGI-based D-W6b-31 left as `unknown`), and "software
// vence integrada" (GODS_LAWS.md L-40).
//
// RED, SEEN: before dxcore_gpu_kind.{hpp,cpp} existed, this file's own
// #include line failed to compile.
//
// dxcore_adapter_facts lives at GLOBAL scope - every field spelled out
// below (never partial designated init) so -Wmissing-field-
// initializers stays quiet under GLINTFX_WERROR.

using glintfx::gltfx_gpu_kind;
using glintfx::platform::classify_dxcore_gpu;

namespace {
dxcore_adapter_facts make_facts(bool hardware_supported, bool is_hardware,
                                bool integrated_supported, bool is_integrated) {
    return dxcore_adapter_facts{.description = "",
                                .luid = 0,
                                .hardware_supported = hardware_supported,
                                .is_hardware = is_hardware,
                                .integrated_supported = integrated_supported,
                                .is_integrated = is_integrated};
}
} // namespace

GLINTFX_TEST(classify_dxcore_gpu_closed_8_cell_enumeration) {
    int analyzed = 0;
    const std::array<dxcore_adapter_facts, 1> one_adapter{make_facts(false, false, false, false)};

    GLINTFX_CHECK(classify_dxcore_gpu(one_adapter, std::nullopt) == gltfx_gpu_kind::unknown);
    ++analyzed;

    GLINTFX_CHECK(classify_dxcore_gpu(one_adapter, std::optional<std::size_t>{5}) ==
                  gltfx_gpu_kind::unknown);
    ++analyzed;

    {
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(false, false, false, false)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::unknown);
        ++analyzed;
    }
    {
        // O executor de CI: hardware_supported=true, is_hardware=false.
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(true, false, false, false)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::software);
        ++analyzed;
    }
    {
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(true, true, false, false)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::unknown);
        ++analyzed;
    }
    {
        // O hibrido rodando na Intel (M2 da revisao).
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(true, true, true, true)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::shared);
        ++analyzed;
    }
    {
        // O desktop de uma placa so - a celula que a D-W6b-31 antiga
        // deixava em unknown.
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(true, true, true, false)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::dedicated);
        ++analyzed;
    }
    {
        // software vence, ordem das regras: is_hardware=false decide
        // antes mesmo de is_integrated=true.
        const std::array<dxcore_adapter_facts, 1> facts{make_facts(true, false, true, true)};
        GLINTFX_CHECK(classify_dxcore_gpu(facts, std::optional<std::size_t>{0}) ==
                      gltfx_gpu_kind::software);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("dxcore_gpu_kind_test: {} celula(s) conferida(s)", analyzed);
}
