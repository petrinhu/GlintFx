// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <print>
#include <vector>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_exclusion.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gpu_kind_exclusion_test.cpp - GL-GPU-KIND, via 1 (docs/plano-w6b-
// fatias-5b-revisao.md sec. 1.5/4.3, D-W6b-38): the closed 8-cell
// enumeration for gpu_kind_after_exclusion() - every shape the plan's
// own table names, including the two/three-placa and the "shared
// never rebaixa nem promove" boundaries (GODS_LAWS.md L-40).
//
// REESCRITO (NOEXCEPT-ALLOC-B8 fatia F3, /var/tmp/glintfx-plan/plano-
// conserto-noexcept.md sec. F3): apply_gpu_kind_exclusion(span) virou
// gpu_kind_after_exclusion(span, index), pura, sem alocar - as MESMAS
// 8 celulas continuam aqui, uma a uma, contra a funcao nova (perder
// uma celula aqui e perder cobertura que hoje existe, GODS_LAWS.md
// L-40). all_after_exclusion() abaixo e um HELPER SO DE TESTE (aloca
// livremente - nunca roda atras da fronteira noexcept da biblioteca)
// que chama gpu_kind_after_exclusion() uma vez por indice e monta o
// vetor que a suite antiga comparava direto - a celula 8 (lista
// vazia) fecha sozinha: o laco abaixo simplesmente nao itera, entao
// nenhum indice invalido e passado a funcao nova para ela.
//
// RED, SEEN: before gpu_kind_exclusion.{hpp,cpp} existed, this file's
// own #include line failed to compile.

using glintfx::gltfx_gpu_kind;
using glintfx::platform::gpu_kind_after_exclusion;

namespace {
constexpr gltfx_gpu_kind unk = gltfx_gpu_kind::unknown;
constexpr gltfx_gpu_kind shr = gltfx_gpu_kind::shared;
constexpr gltfx_gpu_kind ded = gltfx_gpu_kind::dedicated;
constexpr gltfx_gpu_kind sw = gltfx_gpu_kind::software;

// Test-only helper (allocates - never the production noexcept
// boundary): calls gpu_kind_after_exclusion() once per valid index in
// `kinds`, so every index it ever passes is in bounds by construction
// (an empty `kinds` makes the loop not run at all, never a call with
// an out-of-range index).
[[nodiscard]] std::vector<gltfx_gpu_kind>
all_after_exclusion(std::span<const gltfx_gpu_kind> kinds) {
    std::vector<gltfx_gpu_kind> result;
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        result.push_back(gpu_kind_after_exclusion(kinds, static_cast<std::uint32_t>(i)));
    }
    return result;
}
} // namespace

GLINTFX_TEST(gpu_kind_after_exclusion_closed_8_cell_enumeration) {
    int analyzed = 0;

    {
        const std::array<gltfx_gpu_kind, 2> input{unk, shr};
        const std::vector<gltfx_gpu_kind> expected{ded, shr};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, ded};
        const std::vector<gltfx_gpu_kind> expected{unk, ded};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, unk};
        const std::vector<gltfx_gpu_kind> expected{unk, unk};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> input{unk, sw};
        const std::vector<gltfx_gpu_kind> expected{unk, sw};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 1> input{unk};
        const std::vector<gltfx_gpu_kind> expected{unk};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        // Three placas: the two unknown entries both promote, because
        // the premise is "at most one integrated", not "exactly two
        // entries" (revisao.md sec. 1.5).
        const std::array<gltfx_gpu_kind, 3> input{unk, shr, unk};
        const std::vector<gltfx_gpu_kind> expected{ded, shr, ded};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        // shared is never rebaixado nem promovido by this atom.
        const std::array<gltfx_gpu_kind, 2> input{shr, shr};
        const std::vector<gltfx_gpu_kind> expected{shr, shr};
        GLINTFX_CHECK(all_after_exclusion(input) == expected);
        ++analyzed;
    }
    {
        // Lista vazia: o laco de all_after_exclusion() nao itera, e
        // NENHUM indice e passado a gpu_kind_after_exclusion() - a
        // mesma garantia de contrato que o helper documenta no seu
        // proprio comentario.
        const std::vector<gltfx_gpu_kind> input;
        GLINTFX_CHECK(all_after_exclusion(input).empty());
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("gpu_kind_exclusion_test: {} celula(s) conferida(s)", analyzed);
}
