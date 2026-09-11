// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>

#include "container/alloc_counter_classify.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// alloc_counter_classify_test.cpp - CONTAINER-LEAK-COUNTER sub-fatia
// S1 (/var/tmp/glintfx-plan/leak-counter.md sec. 5), classificador
// puro: nenhum destes casos toca backtrace()/dl_iterate_phdr - so
// aritmetica de intervalo sobre enderecos fabricados, por isso este
// arquivo compila e roda nas cinco plataformas sem guarda de sistema
// (nenhum #include de cabecalho de SO aqui).
//
// Enderecos fabricados: classify_allocation_frames() so COMPARA os
// enderecos que recebe (glintfx_leak_counter::address_range::
// contains(), definido com std::less<const void *> - um total order
// valido mesmo entre ponteiros nao relacionados, ao contrario de
// `<`/`>=` crus sobre ponteiros de objetos diferentes) - nunca os
// desreferencia, entao numeros de fantasia via reinterpret_cast sao
// seguros aqui, do mesmo jeito que o gancho real (S2) vai comparar
// enderecos de retorno genuinos de backtrace() sem nunca ler por eles.

namespace {

using glintfx_leak_counter::address_range;
using glintfx_leak_counter::alloc_classify_ranges;
using glintfx_leak_counter::alloc_classify_result;
using glintfx_leak_counter::alloc_frame_class;
using glintfx_leak_counter::classify_allocation_frames;

[[nodiscard]] const void *fake_addr(std::uintptr_t value) {
    // Endereco de fantasia, nunca desreferenciado - ver o comentario
    // do topo do arquivo. Mesmo idioma ja usado em src/platform/
    // win32/display_adapter.cpp e tests/fake/fake_arena_tree.hpp.
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: ver comentario acima
    return reinterpret_cast<const void *>(value);
}

// Quatro faixas fabricadas, NAO sobrepostas, com folga entre elas, e
// um endereco bem fora de todas para representar biblioteca
// desconhecida (third-party). Os valores nao precisam ser realistas -
// so precisam estar em ordem e sem sobreposicao.
constexpr std::uintptr_t k_exec_begin = 0x1000;
constexpr std::uintptr_t k_exec_end = 0x2000;
constexpr std::uintptr_t k_libstdcxx_begin = 0x3000;
constexpr std::uintptr_t k_libstdcxx_end = 0x4000;
constexpr std::uintptr_t k_libc_begin = 0x5000;
constexpr std::uintptr_t k_libc_end = 0x6000;
constexpr std::uintptr_t k_hook_begin = 0x7000;
constexpr std::uintptr_t k_hook_end = 0x8000;
constexpr std::uintptr_t k_third_party_addr = 0x90000;

[[nodiscard]] alloc_classify_ranges fixture_ranges() {
    alloc_classify_ranges ranges;
    ranges.executable = address_range{fake_addr(k_exec_begin), fake_addr(k_exec_end)};
    ranges.libstdcxx = address_range{fake_addr(k_libstdcxx_begin), fake_addr(k_libstdcxx_end)};
    ranges.libc = address_range{fake_addr(k_libc_begin), fake_addr(k_libc_end)};
    ranges.hook = address_range{fake_addr(k_hook_begin), fake_addr(k_hook_end)};
    return ranges;
}

} // namespace

// Caso (a) do plano: primeiro quadro fora do gancho ja esta dentro do
// executavel - nada a pular, decide de cara: ours.
GLINTFX_TEST(classify_first_frame_in_executable_is_ours) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_exec_begin + 0x10)};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::ours);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso (b) do plano: gancho, depois libstdc++ (o std::string::
// _M_create que NAO decide), depois executavel - os dois primeiros
// sao pulados, o terceiro decide: ours.
GLINTFX_TEST(classify_hook_then_libstdcxx_then_executable_is_ours) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {
        fake_addr(k_hook_begin + 0x10),
        fake_addr(k_libstdcxx_begin + 0x10),
        fake_addr(k_exec_begin + 0x20),
    };
    const alloc_classify_result result = classify_allocation_frames(frames, 3, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::ours);
    GLINTFX_CHECK(result.decisive_frame == frames[2]);
}

// Caso gemeo do (b), para a libc em vez da libstdc++ (achado da
// revisao de S1, CONTAINER-LEAK-COUNTER: a mutacao "parar de pular
// quadros da libc" sobrevivia porque nenhum caso usava um endereco
// dessa faixa como QUADRO DE PILHA - so aparecia montando o intervalo
// em fixture_ranges(), nunca como entrada de classify_allocation_
// frames()). Gancho, depois libc (ex.: um quadro dentro de malloc()
// que a implementacao de operator new da libstdc++ as vezes atravessa
// antes de retornar - nao decide), depois executavel - os dois
// primeiros sao pulados, o terceiro decide: ours.
GLINTFX_TEST(classify_hook_then_libc_then_executable_is_ours) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {
        fake_addr(k_hook_begin + 0x10),
        fake_addr(k_libc_begin + 0x10),
        fake_addr(k_exec_begin + 0x20),
    };
    const alloc_classify_result result = classify_allocation_frames(frames, 3, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::ours);
    GLINTFX_CHECK(result.decisive_frame == frames[2]);
}

// Caso (c) do plano: gancho, libstdc++, biblioteca desconhecida - o
// terceiro decide, e nao esta no executavel: third_party.
GLINTFX_TEST(classify_hook_then_libstdcxx_then_unknown_library_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {
        fake_addr(k_hook_begin + 0x10),
        fake_addr(k_libstdcxx_begin + 0x10),
        fake_addr(k_third_party_addr),
    };
    const alloc_classify_result result = classify_allocation_frames(frames, 3, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == frames[2]);
}

// Caso (d) do plano: pilha inteira de biblioteca desconhecida (ex.:
// thread do llvmpipe) - o primeiro quadro ja decide: third_party.
GLINTFX_TEST(classify_unknown_library_stack_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_third_party_addr),
                                  fake_addr(k_third_party_addr + 0x100)};
    const alloc_classify_result result = classify_allocation_frames(frames, 2, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso (e1) do plano: pilha vazia - third_party, quadro decisivo
// nulo. A classe que reprova (ours) exige evidencia POSITIVA; a
// ausencia de quadro nunca e essa evidencia.
GLINTFX_TEST(classify_empty_stack_is_third_party_with_null_decisive_frame) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const alloc_classify_result result = classify_allocation_frames(nullptr, 0, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == nullptr);
}

// Caso (e2) do plano: pilha so com quadros do proprio gancho - todos
// pulados, mesmo resultado do (e1): third_party, quadro decisivo nulo.
GLINTFX_TEST(classify_stack_only_hook_frames_is_third_party_with_null_decisive_frame) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_hook_begin + 0x10), fake_addr(k_hook_begin + 0x50)};
    const alloc_classify_result result = classify_allocation_frames(frames, 2, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == nullptr);
}

// Caso (f1) do plano: endereco EXATAMENTE no inicio do intervalo do
// executavel (equivalente a __executable_start) esta DENTRO: ours.
GLINTFX_TEST(classify_address_exactly_at_executable_begin_is_ours) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_exec_begin)};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::ours);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso (f2) do plano: endereco EXATAMENTE no fim do intervalo
// (equivalente a etext) esta FORA - limite superior exclusivo:
// third_party.
GLINTFX_TEST(classify_address_exactly_at_executable_end_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_exec_end)};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso extra (fronteira pedida pelo orquestrador, alem do (f1)/(f2)
// do plano): um endereco IMEDIATAMENTE ANTES do inicio do intervalo
// esta FORA.
GLINTFX_TEST(classify_address_one_before_executable_begin_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_exec_begin - 1)};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso extra: um endereco IMEDIATAMENTE DEPOIS do fim do intervalo
// (etext + 1) tambem esta FORA - distinto do (f2), que testa o limite
// exato em vez de um passo alem dele.
GLINTFX_TEST(classify_address_one_after_executable_end_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {fake_addr(k_exec_end + 1)};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == frames[0]);
}

// Caso extra: endereco de quadro NULO (distinto do (e1), pilha
// genuinamente vazia - aqui a pilha tem um quadro, e o valor dele e
// nullptr). Nenhuma faixa fabricada contem o endereco zero, entao e
// evidencia negativa como qualquer outro endereco fora: third_party.
GLINTFX_TEST(classify_null_frame_address_is_third_party) {
    const alloc_classify_ranges ranges = fixture_ranges();
    const void *const frames[] = {nullptr};
    const alloc_classify_result result = classify_allocation_frames(frames, 1, ranges);
    GLINTFX_CHECK(result.klass == alloc_frame_class::third_party);
    GLINTFX_CHECK(result.decisive_frame == nullptr);
}
