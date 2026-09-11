// SPDX-License-Identifier: AGPL-3.0-or-later
#include "alloc_counter_classify.hpp"

#include <functional>

// alloc_counter_classify.cpp - ver o cabecalho para o contrato
// completo (CONTAINER-LEAK-COUNTER sub-fatia S1). A implementacao
// inteira e aritmetica de intervalo: nenhuma chamada de sistema,
// nenhuma alocacao, nenhum estado - propositalmente, para poder ser
// exercitada como caso de teste puro nas cinco plataformas.

namespace glintfx_leak_counter {

bool address_range::contains(const void *addr) const noexcept {
    // addr >= begin && addr < end, mas via std::less<const void *>
    // (ver o comentario do cabecalho): da ordem total mesmo quando
    // addr, begin e end nao apontam para o mesmo objeto.
    const std::less<const void *> less{};
    return !less(addr, begin) && less(addr, end);
}

alloc_classify_result classify_allocation_frames(const void *const *frames, std::size_t frame_count,
                                                 const alloc_classify_ranges &ranges) noexcept {
    for (std::size_t i = 0; i < frame_count; ++i) {
        const void *frame = frames[i];
        if (ranges.hook.contains(frame) || ranges.libstdcxx.contains(frame) ||
            ranges.libc.contains(frame)) {
            // Pulado: gancho ou runtime da propria linguagem nunca
            // decide quem pediu a alocacao (leak-counter.md sec. 4).
            continue;
        }
        if (ranges.executable.contains(frame)) {
            return {.klass = alloc_frame_class::ours, .decisive_frame = frame};
        }
        return {.klass = alloc_frame_class::third_party, .decisive_frame = frame};
    }
    // Nenhum quadro restante deu evidencia: nunca `ours` por falta de
    // informacao (leak-counter.md sec. 5, caso e).
    return {.klass = alloc_frame_class::third_party, .decisive_frame = nullptr};
}

} // namespace glintfx_leak_counter
