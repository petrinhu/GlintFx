// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gpu_kind_exclusion.hpp"

// ACHADO, NAO CONSERTADO (varredura de 07/09/2026, mesma familia de
// bugs de gfx_open_only_fixation.cpp - GODS_LAWS.md L-22): a construcao
// de `result` abaixo pode lancar std::bad_alloc dentro de uma funcao
// noexcept. Diferente dos demais achados desta varredura, este atomo
// NAO tem como degradar com seguranca: seu unico chamador (gpu_kind_
// seam.cpp's own resolve_kernel_and_exclusion()) indexa o vetor
// devolvido por `enumeration_index` SEM reconferir o tamanho -
// devolver um vetor vazio ou menor que `kinds` trocaria um terminate()
// por um acesso fora dos limites, um defeito PIOR, nao um conserto.
// Consertar de verdade exigiria dar a esta funcao (e a resolve_kernel_
// and_exclusion() acima dela, e a classify_current_gpu() dos DOIS
// lados - wayland e win32, este ultimo fora do mandato desta tarefa)
// um canal de erro que hoje nenhuma das duas tem - fora do escopo
// cirurgico desta fatia (GODS_LAWS.md L-32). Aceito como risco
// residual, nomeado e nao escondido: a entrada e sempre pequena e
// fechada (a contagem real de placas de video de uma maquina, nunca
// uma lista vinda de rede ou de um compositor hostil), o mesmo perfil
// de risco que a suite de dependencia zero ja aceita para outras
// alocacoes de tamanho fixo pequeno em todo este projeto.
namespace glintfx::platform {

std::vector<gltfx_gpu_kind>
apply_gpu_kind_exclusion(std::span<const gltfx_gpu_kind> kinds) noexcept {
    std::vector<gltfx_gpu_kind> result(kinds.begin(), kinds.end());

    bool any_kernel_shared = false;
    for (const gltfx_gpu_kind kind : kinds) {
        if (kind == gltfx_gpu_kind::shared) {
            any_kernel_shared = true;
            break;
        }
    }

    if (!any_kernel_shared) {
        return result; // nothing to exclude by - via 2 or unknown decide next
    }

    for (gltfx_gpu_kind &kind : result) {
        if (kind == gltfx_gpu_kind::unknown) {
            kind = gltfx_gpu_kind::dedicated;
        }
    }

    return result;
}

} // namespace glintfx::platform
