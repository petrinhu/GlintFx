// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <print>
#include <vector>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_exclusion.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gpu_kind_exclusion_test.cpp - GL-GPU-KIND, via 1 (docs/plano-w6b-
// fatias-5b-revisao.md sec. 1.5/4.3, D-W6b-38): the closed 8-cell
// enumeration for apply_gpu_kind_exclusion() - every shape the plan's
// own table names, including the two/three-placa and the "shared
// never rebaixa nem promove" boundaries (GODS_LAWS.md L-40).
//
// RED, SEEN: before gpu_kind_exclusion.{hpp,cpp} existed, this file's
// own #include line failed to compile.

using glintfx::gltfx_gpu_kind;
using glintfx::platform::apply_gpu_kind_exclusion;

namespace {
constexpr gltfx_gpu_kind unk = gltfx_gpu_kind::unknown;
constexpr gltfx_gpu_kind shr = gltfx_gpu_kind::shared;
constexpr gltfx_gpu_kind ded = gltfx_gpu_kind::dedicated;
constexpr gltfx_gpu_kind sw = gltfx_gpu_kind::software;
} // namespace

GLINTFX_TEST(apply_gpu_kind_exclusion_closed_8_cell_enumeration) {
    int analyzed = 0;

    {
        const std::array<gltfx_gpu_kind, 2> input{unk, shr};
        const std::vector<gltfx_gpu_kind> expected{ded, shr};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, ded};
        const std::vector<gltfx_gpu_kind> expected{unk, ded};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, unk};
        const std::vector<gltfx_gpu_kind> expected{unk, unk};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, sw};
        const std::vector<gltfx_gpu_kind> expected{unk, sw};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 1> input{unk};
        const std::vector<gltfx_gpu_kind> expected{unk};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        // Three placas: the two unknown entries both promote, because
        // the premise is "at most one integrated", not "exactly two
        // entries" (revisao.md sec. 1.5).
        const std::array<gltfx_gpu_kind, 3> input{unk, shr, unk};
        const std::vector<gltfx_gpu_kind> expected{ded, shr, ded};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        // shared is never rebaixado nem promovido by this atom.
        const std::array<gltfx_gpu_kind, 2> input{shr, shr};
        const std::vector<gltfx_gpu_kind> expected{shr, shr};
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::vector<gltfx_gpu_kind> input;
        GLINTFX_CHECK(apply_gpu_kind_exclusion(input).empty());
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("gpu_kind_exclusion_test: {} celula(s) conferida(s)", analyzed);
}
