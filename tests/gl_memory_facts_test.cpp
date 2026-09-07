// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>

#include "platform/gl/gl_memory_facts.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_memory_facts_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5b-
// revisao.md sec. 1.5, D-W6b-38, CONSERTO 06/09/2026): the TDD red/
// green witness for the achado a real container executor produced -
// glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, ...) raised
// NO error (the extension answered "supported") but wrote back
// UNINITIALIZED memory, a different arbitrary number on every process
// run (measured: -21578144, 892646176, 237507360, same driver, same
// token, three separate runs inside glintfx-wltest's own Mesa 26.1.8
// llvmpipe). classify_by_memory_separation() was already correct
// given whatever it was handed; read_gl_memory_facts() was the one
// handing it garbage as if it were a real capacity.
//
// Each fake driver below is a PLAIN FUNCTION POINTER (gl_get_
// integerv_fn/gl_get_error_fn have no captured state, the same
// constraint every real GL entry point already has) - a static
// call-count variable inside the function body is what lets the
// SECOND test case simulate "this token is garbage: two consecutive
// reads disagree", the exact defect this file's own header names.
//
// RED, SEEN: before read_gl_memory_facts() double-read the capacity
// tokens, TWO of the three cases below failed (garbage-driver and
// half-garbage-driver both used to report nvx_present=true).

namespace {

// CONSERTO (07/09/2026, achado do team-lead sobre o trabalho orfao
// adotado neste arquivo, reprovado por mutacao real - "trocar a
// segunda chamada de read_gl_memory_facts() para o token da primeira,
// e o mutante sobrevive"): a versao anterior desta funcao IGNORAVA
// `pname` e devolvia o mesmo numero pros dois tokens - primeiro
// 4194304 pros dois (achado do clang-tidy, ternario redundante),
// depois 4194304/3145728 mas o segundo era um numero SINTETICO, so
// escolhido pra ser diferente. As duas versoes tinham o MESMO buraco
// de fundo: nada aqui provava que read_gl_memory_facts() pede o TOKEN
// CERTO pro CAMPO certo - trocar 0x9047 (dedicada) por 0x9048 (total
// disponivel) dentro da funcao real, ou ler o mesmo token duas vezes,
// nao reprovava nada quando os dois valores esperados eram iguais OU
// inventados.
//
// NUNCA INVENTAR NUMERO (ordem do team-lead): os dois valores abaixo
// sao os MEDIDOS DE VERDADE do renderizador por software desta
// maquina (llvmpipe), a MESMA dupla que tests/memory_separation_kind_
// test.cpp ja documenta e usa ("this machine actually measured...
// the llvmpipe's own 'total == system RAM'") - 0 KB dedicado (nao tem
// placa propria) e 32548140 KB disponivel total (a RAM inteira do
// sistema). Sao DIFERENTES um do outro por serem REAIS, nao por
// escolha - e e essa diferenca real que agora acusa um token trocado
// ou duplicado, numa asercao especifica, nunca em silencio.
extern "C" void real_driver_get_integerv(unsigned int pname, int *params) noexcept {
    if (params != nullptr) {
        *params = pname == 0x9047 ? 0 : 32548140;
    }
}
extern "C" unsigned int real_driver_get_error() noexcept { return 0; }

// GL_INVALID_ENUM only AFTER the real query (the initial drain in
// read_gl_memory_facts() must see a clean 0, or it never terminates -
// a real glGetError() never returns nonzero forever either).
int absent_call_count = 0;
extern "C" void absent_driver_get_integerv(unsigned int /*pname*/, int * /*params*/) noexcept {}
extern "C" unsigned int absent_driver_get_error() noexcept {
    return absent_call_count++ == 0 ? 0 : 0x0500; // GL_INVALID_ENUM
}

// The measured defect, reproduced exactly: no error, but a DIFFERENT
// arbitrary value on every call - a static counter is what makes a
// plain, stateless function pointer act like the uninitialized memory
// this project's own container executor actually returned.
int garbage_call_count = 0;
extern "C" void garbage_driver_get_integerv(unsigned int /*pname*/, int *params) noexcept {
    if (params != nullptr) {
        *params = garbage_call_count == 0 ? -21578144 : 892646176;
    }
    ++garbage_call_count;
}
extern "C" unsigned int garbage_driver_get_error() noexcept { return 0; }

} // namespace

GLINTFX_TEST(read_gl_memory_facts_trusts_a_consistent_capacity) {
    const gl_memory_facts facts =
        read_gl_memory_facts(real_driver_get_integerv, real_driver_get_error);
    GLINTFX_CHECK(facts.nvx_present);
    // Dois valores REAIS e DIFERENTES de proposito (achado do
    // team-lead, 07/09/2026, reprovado por mutacao antes deste
    // conserto): a versao anterior asseverava os dois campos iguais
    // (primeiro 4194304 pros dois, depois um segundo numero
    // inventado), o que nunca provava que cada campo veio do TOKEN
    // certo - ver o comentario de real_driver_get_integerv() acima
    // para a proveniencia medida dos dois numeros.
    GLINTFX_CHECK_EQ(facts.dedicated_kb, std::int64_t{0});
    GLINTFX_CHECK_EQ(facts.total_available_kb, std::int64_t{32548140});
}

GLINTFX_TEST(read_gl_memory_facts_absent_extension_reports_not_present) {
    absent_call_count = 0;
    const gl_memory_facts facts =
        read_gl_memory_facts(absent_driver_get_integerv, absent_driver_get_error);
    GLINTFX_CHECK(!facts.nvx_present);
}

GLINTFX_TEST(read_gl_memory_facts_never_trusts_a_garbage_capacity_reading) {
    garbage_call_count = 0;
    const gl_memory_facts facts =
        read_gl_memory_facts(garbage_driver_get_integerv, garbage_driver_get_error);
    // The container executor's own measured defect (this file's own
    // header comment): the driver never raises GL_INVALID_ENUM, so the
    // ONLY thing that can catch it is the double-read disagreeing -
    // nvx_present MUST come back false, never a value built from
    // uninitialized memory.
    GLINTFX_CHECK(!facts.nvx_present);
}

GLINTFX_TEST(read_gl_memory_facts_null_function_pointers_report_not_present) {
    const gl_memory_facts facts = read_gl_memory_facts(nullptr, nullptr);
    GLINTFX_CHECK(!facts.nvx_present);
    std::println("gl_memory_facts_test: 4 cenario(s) conferido(s)");
}
