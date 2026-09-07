// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gpu_kind_seam.hpp"

#include "platform/gl/gpu_kind_exclusion.hpp"

namespace glintfx::platform {

gltfx_gpu_kind
resolve_kernel_and_exclusion(gltfx_gpu_kind kernel_kind, std::uint32_t enumeration_index,
                             std::span<const gltfx_gpu_kind> enumeration_kinds) noexcept {
    if (kernel_kind != gltfx_gpu_kind::unknown) {
        return kernel_kind;
    }

    if (enumeration_index == k_gltfx_gpu_index_unknown) {
        return gltfx_gpu_kind::unknown;
    }

    const std::vector<gltfx_gpu_kind> excluded = apply_gpu_kind_exclusion(enumeration_kinds);
    return excluded[enumeration_index];
}

gltfx_gpu_kind
resolve_memory_separation(gltfx_gpu_kind kind_so_far, bool definitely_not_software,
                          std::optional<gltfx_gpu_kind> memory_separation_kind) noexcept {
    // CONSERTO (07/09/2026, revisão adversarial da fatia 5b): até
    // aqui, classify_current_gpu() (nos dois arquivos) só olhava
    // `kind_so_far == unknown` - o portão de certeza abaixo é o que
    // fecha o achado ("a consulta ao dispositivo falhou, e o código
    // nunca sabe se está em software, e vai direto à via da
    // memória"). RED, VISTO em 07/09/2026 contra a extração fiel do
    // código de hoje (`tools/preci.sh` não rodado ainda - compile
    // standalone, GCC 16.2.1, `-std=c++23`):
    //
    //   tests/gpu_kind_seam_test.cpp:108: failed:
    //   resolve_memory_separation(unk, false, std::optional{ded}) == unk
    //   [FAIL] resolve_memory_separation_gate_de_certeza
    //   --- 2 case(s), 1 failure(s) ---
    //
    // "onde o sistema não responde, a resposta é 'não sei'" (ordem do
    // líder, 06/09/2026, ESCOPO.md) - `definitely_not_software=false`
    // cobre TANTO "o sistema disse que é software" (irrelevante aqui,
    // porque `kind_so_far` já teria saído `software` antes de chegar
    // nesta função) QUANTO "o sistema nunca respondeu" - as duas
    // nunca autorizam usar o token de memória.
    if (kind_so_far != gltfx_gpu_kind::unknown) {
        return kind_so_far;
    }
    if (!definitely_not_software || !memory_separation_kind.has_value()) {
        return gltfx_gpu_kind::unknown;
    }
    return *memory_separation_kind;
}

} // namespace glintfx::platform
