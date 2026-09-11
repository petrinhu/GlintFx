// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

// alloc_counter_classify.hpp - CONTAINER-LEAK-COUNTER sub-fatia S1
// (/var/tmp/glintfx-plan/leak-counter.md sec. 5, GODS_LAWS.md L-04/
// L-07/L-17/L-20/L-34): classificador PURO de quadro de pilha de
// alocacao, o atomo que o gancho de contagem (S2) vai usar dentro do
// container. PURO de proposito: aritmetica de intervalo de enderecos,
// sem backtrace(), sem dl_iterate_phdr, sem estado global - e por
// isso compila e roda, testado, nas cinco plataformas, fora de
// qualquer container (nao inclui nenhum cabecalho de sistema).
//
// A REGRA (leak-counter.md sec. 4): pula-se qualquer quadro dentro do
// proprio gancho, da libstdc++ ou da libc; o PRIMEIRO quadro restante
// decide - dentro do intervalo do executavel e classe `ours`, fora e
// `third_party`. Pilha vazia, ou pilha inteira feita so de quadros
// pulados, e `third_party` com quadro decisivo nulo: a classe que
// reprova (`ours`) exige evidencia POSITIVA, nunca e o resultado por
// falta de informacao (leak-counter.md sec. 5, caso e).

namespace glintfx_leak_counter {

enum class alloc_frame_class {
    ours,
    third_party,
};

// Intervalo meio-aberto [begin, end): `begin` esta DENTRO, `end` esta
// FORA (leak-counter.md sec. 5, caso f - o limite superior e
// exclusivo). contains() usa std::less<const void *> em vez de `<`/
// `>=` crus: a norma so garante ordem total para ponteiros DO MESMO
// array/objeto com relacionais crus, e os enderecos comparados aqui
// (um quadro de pilha genuino, dado por backtrace() em S2) nunca sao
// do mesmo objeto que `begin`/`end` - std::less<T*> e a forma padrao
// de comparar ponteiros nao relacionados sem essa UB.
struct address_range {
    const void *begin = nullptr;
    const void *end = nullptr;

    [[nodiscard]] bool contains(const void *addr) const noexcept;
};

struct alloc_classify_ranges {
    address_range executable;
    address_range libstdcxx;
    address_range libc;
    address_range hook;
};

struct alloc_classify_result {
    alloc_frame_class klass = alloc_frame_class::third_party;
    const void *decisive_frame = nullptr;
};

// `frames[0]` e o quadro mais interno (o que pediu a alocacao
// diretamente), na mesma ordem que a glibc backtrace() devolve - S1
// nao chama backtrace(): quem chama (S2) passa o array ja pronto.
// `frames` pode ser nulo quando `frame_count` e zero (pilha vazia,
// caso e1 do plano).
[[nodiscard]] alloc_classify_result
classify_allocation_frames(const void *const *frames, std::size_t frame_count,
                           const alloc_classify_ranges &ranges) noexcept;

} // namespace glintfx_leak_counter
