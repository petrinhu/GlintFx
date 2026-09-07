// SPDX-License-Identifier: AGPL-3.0-or-later
#include <optional>
#include <print>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/memory_separation_kind.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// memory_separation_kind_test.cpp - GL-GPU-KIND, via 2 (docs/plano-
// w6b-fatias-5b-revisao.md sec. 1.5/4.3, D-W6b-38): the closed 5-cell
// enumeration for classify_by_memory_separation() - the two values
// this machine actually measured (the NVIDIA's 4 GiB pool, the
// llvmpipe's own "total == system RAM"), plus the boundaries (GODS_
// LAWS.md L-40/L-43: a value one step past the freshly-widened
// fronteira, never just the exact edge).

using glintfx::gltfx_gpu_kind;
using glintfx::platform::classify_by_memory_separation;

GLINTFX_TEST(classify_by_memory_separation_closed_5_cell_enumeration) {
    int analyzed = 0;

    GLINTFX_CHECK(classify_by_memory_separation(
                      {.nvx_present = false, .dedicated_kb = -1, .total_available_kb = -1}) ==
                  std::nullopt);
    ++analyzed;

    GLINTFX_CHECK(
        classify_by_memory_separation(
            {.nvx_present = true, .dedicated_kb = 4194304, .total_available_kb = 4194304}) ==
        std::optional{gltfx_gpu_kind::dedicated});
    ++analyzed;

    GLINTFX_CHECK(classify_by_memory_separation(
                      {.nvx_present = true, .dedicated_kb = 0, .total_available_kb = 32548140}) ==
                  std::optional{gltfx_gpu_kind::shared});
    ++analyzed;

    GLINTFX_CHECK(classify_by_memory_separation(
                      {.nvx_present = true, .dedicated_kb = -1, .total_available_kb = -1}) ==
                  std::nullopt);
    ++analyzed;

    // One step past the shared/dedicated boundary (L-43): 1 KiB of its
    // own is still "separate from system RAM".
    GLINTFX_CHECK(classify_by_memory_separation(
                      {.nvx_present = true, .dedicated_kb = 1, .total_available_kb = 1}) ==
                  std::optional{gltfx_gpu_kind::dedicated});
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 5);
    std::println("memory_separation_kind_test: {} celula(s) conferida(s)", analyzed);
}
