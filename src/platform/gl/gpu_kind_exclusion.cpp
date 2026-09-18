// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gpu_kind_exclusion.hpp"

// CONSERTO (NOEXCEPT-ALLOC-B8 fatia F3, ESCOPO.md Decisao 10,
// 17/09/2026): esta funcao costumava materializar um std::vector
// inteiro (copia de `kinds`) so para o seu unico chamador (gpu_kind_
// seam.cpp's own resolve_kernel_and_exclusion()) indexar UM valor por
// `enumeration_index` - alocar dentro desta funcao `noexcept` podia
// lancar std::bad_alloc e terminar o processo do consumidor
// (GODS_LAWS.md L-22; ESCOPO.md, Decisao 8). Cada celula de saida
// depende so da propria celula de entrada mais a bandeira unica
// `any_kernel_shared` sobre a enumeracao inteira - a lista nunca foi
// necessaria. gpu_kind_after_exclusion() abaixo devolve o MESMO valor
// para o indice pedido, sem alocar nada (degrau 1 da escada de R3,
// docs/api-conventions.md).
namespace glintfx::platform {

gltfx_gpu_kind gpu_kind_after_exclusion(std::span<const gltfx_gpu_kind> kinds,
                                        std::uint32_t index) noexcept {
    // O mesmo contrato implicito que a forma antiga (`excluded[
    // enumeration_index]`) ja tinha: quem chama garante `index` dentro
    // dos limites de `kinds` - gpu_kind_seam.cpp's own resolve_kernel_
    // and_exclusion() so chama esta funcao quando `enumeration_index
    // != k_gltfx_gpu_index_unknown`, e esse indice e sempre a posicao
    // real do dispositivo atual dentro de `kinds` (egl_context_
    // adapter.cpp/wgl_context_adapter.cpp's own callers constroem os
    // dois juntos, na mesma enumeracao). Sem checagem de limite aqui:
    // o mesmo risco de acesso indevido que `std::vector::operator[]`
    // ja tinha antes, nunca pior, nunca melhor.
    const gltfx_gpu_kind at_index = kinds[index];
    if (at_index != gltfx_gpu_kind::unknown) {
        return at_index;
    }

    for (const gltfx_gpu_kind kind : kinds) {
        if (kind == gltfx_gpu_kind::shared) {
            return gltfx_gpu_kind::dedicated;
        }
    }

    return gltfx_gpu_kind::unknown;
}

} // namespace glintfx::platform
