// SPDX-License-Identifier: AGPL-3.0-or-later
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/window_desc_validation.hpp"

// err_copy_terminates_window_validation_test.cpp - T3 da onda
// W-ERRCOPY (TODO.md ERR-COPY-RED, ESCOPO.md Decisao 17, GODS_LAWS.md
// L-09/L-20/L-35): prova, com uma funcao noexcept REAL da arvore
// (validate_window_text_field, src/platform/window/window_desc_
// validation.cpp:10-24 - um dos 129 sitios classificados no oraculo
// congelado do planejador, /var/tmp/glintfx-errcopia/sitios_
// classificados.json), que o defeito descrito no plano da onda (secao
// 1) mata o PROCESSO inteiro, nao so' reprova uma asserção.
//
// A CADEIA: `::err(gltfx_err(codigo).with_rejected_value(valor))`
// (window_desc_validation.cpp:16-17/20-21) e' a forma
// CADEIA_with_lvalue do oraculo - with_rejected_value() devolve
// `gltfx_err&` (include/glintfx/core/err.hpp:257), uma referencia de
// lvalue, entao `gltfx_rslt<void>::err(gltfx_err failure) noexcept`
// (err.hpp:299) precisa COPIAR esse lvalue pro parametro por valor.
// Essa copia (err.cpp:33-37) aloca sempre que ha' contexto, com a
// forma QUE LANCA (`new err_context(...)`, nao a nothrow). Sob
// alocador armado pra falhar exatamente nessa alocacao, ela lanca
// std::bad_alloc de dentro da construcao do parametro de uma funcao
// noexcept sem try cobrindo - [except.terminate] chama
// std::terminate() antes que qualquer GLINTFX_CHECK abaixo rode.
//
// CANDIDATO SEM JANELA (GODS_LAWS.md L-09 do projeto): window_desc_
// validation.cpp e' validacao pura (nenhum tipo Wayland/EGL/Win32
// alcancavel dali), um dos quatro candidatos que o plano da onda nomeia
// para T3 (window_desc_validation.cpp, gfx_option_validation.cpp,
// gl_context_desc_validation.cpp, loop_callbacks_validation.cpp).
//
// FORMA DE ARMAR, PRECISA (nao "falha na proxima alocacao qualquer"):
// with_rejected_value("title") passa primeiro por ensure_context()
// (err.cpp:41-46) - `new (std::nothrow) err_context()`, 1a chamada
// real a operator new, FORMA NOTHROW. Em seguida
// `m_context->rejected_value.assign("title")` - "title" (5 bytes) cabe
// folgado no buffer de small-string-optimization de libstdc++
// (tipicamente 15-22 bytes), entao NAO aloca uma segunda vez ali.
// g_calls_to_allow_before_failure=1 deixa essa 1a chamada passar e
// forca a 2a chamada real a operator new - exatamente a do construtor
// de copia (`new err_context(*other.m_context)`, FORMA QUE LANCA) - a
// falhar. Sem essa contagem precisa, armar "falha na proxima
// alocacao" faria ensure_context() degradar silenciosamente (ela usa
// `new(std::nothrow)`, best-effort, GODS_LAWS.md L-22) e o teste
// mediria o mecanismo errado.
//
// TECNICA DE OVERRIDE (mesmo idioma ja em tests/fixation_write_and_
// err_copy_oom_test.cpp, reusado aqui, nao reinventado): contadores
// std::atomic, nao std::size_t/bool planos - GODS_LAWS.md L-40/L-43,
// mesma razao ja documentada naquele arquivo (contador plano na MESMA
// TU que o override, sob -O2, pode reordenar em torno da chamada real
// a operator new e ler zero mesmo com a chamada tendo acontecido).
// Interposicao valida no Linux/ELF (glintfx::glintfx e' consumido como
// biblioteca compartilhada por padrao, GLINTFX_API cruza esse
// boundary) - o mesmo mecanismo que tests/err_context_test.cpp e
// tests/fixation_write_and_err_copy_oom_test.cpp ja' declaram e usam;
// o gap Windows/SHARED que aqueles dois arquivos fecham com
// harness/win_dll_alloc_hook.hpp NAO se aplica a esta fatia - GODS_
// LAWS.md L-09 do projeto restringe este teste a Linux, sem janela, e
// a ordem de servico desta fatia nomeia window_desc_validation.cpp
// exatamente por ser alcancavel assim.
//
// VERMELHO ESPERADO, E POR QUE NAO HA' UM "TESTE DE MORTE" AQUI: este
// binario de teste morre (SIGABRT via std::terminate()) ANTES de
// qualquer GLINTFX_CHECK rodar. Nao e' preciso capturar o sinal - o
// ctest ja reprova um caso cujo processo nao sobrevive (plano da onda,
// secao 7, T3: "hoje mata, o ctest reprova por morte de processo").
// Este e' o vermelho legitimo que a fatia F2 pede, nao um erro de
// sintaxe: a prova de que o motivo certo esta' em jogo e' que os
// contadores abaixo, se este arquivo compilar e RODAR sem crashar, se
// comportariam exatamente como os de tests/fixation_write_and_err_
// copy_oom_test.cpp - o defeito e' o unico jeito de o processo nao
// chegar ao fim da funcao.

namespace {

std::atomic<bool> g_force_alloc_failure{false};
std::atomic<std::size_t> g_calls_to_allow_before_failure{0};
std::atomic<std::size_t> g_override_new_call_count{0};

[[nodiscard]] bool should_fail_this_allocation() noexcept {
    if (!g_force_alloc_failure.load(std::memory_order_relaxed)) {
        return false;
    }
    if (g_calls_to_allow_before_failure.load(std::memory_order_relaxed) > 0) {
        g_calls_to_allow_before_failure.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

} // namespace

void *operator new(std::size_t size) {
    g_override_new_call_count.fetch_add(1, std::memory_order_relaxed);
    if (should_fail_this_allocation()) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    g_override_new_call_count.fetch_add(1, std::memory_order_relaxed);
    if (should_fail_this_allocation()) {
        return nullptr;
    }
    return std::malloc(size);
}

// GCC's -Wmismatched-new-delete (mesmo achado ja documentado em
// tests/fixation_write_and_err_copy_oom_test.cpp): quando o
// otimizador consegue inlinear a cadeia inteira new -> string/vector
// -> delete dentro de uma unica funcao de teste, ele rastreia a
// proveniencia do ponteiro e reclama de std::free() nao ser "o
// desalocador emparelhado" de operator new(), mesmo sendo o mesmo par
// malloc/free por baixo em glibc. Suprimido localmente, com a razao
// nomeada (GODS_LAWS.md L-32: nunca afrouxar warning fora do escopo
// cirurgico do pedido).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t /*size*/) noexcept { std::free(p); }
void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

GLINTFX_TEST(noexcept_validation_survives_context_copy_under_forced_oom) {
    g_force_alloc_failure = true;
    // Deixa a 1a chamada real a operator new (ensure_context(), forma
    // nothrow) passar; forca a 2a (o construtor de copia do
    // gltfx_err(...).with_rejected_value(...) lvalue, forma que
    // lanca) a falhar - ver o comentario "FORMA DE ARMAR" acima.
    g_calls_to_allow_before_failure = 1;

    // A CHAMADA REAL, sem indireção nenhuma: window_desc_validation.
    // cpp:10-24, sitio classificado no oraculo do planejador. Se este
    // processo sobreviver ate' a linha seguinte, T3 deixou de ser
    // vermelho - o que so' deve acontecer depois que a fatia F4
    // (ESCOPO.md Decisao 17, copia-na-escrita) tornar a copia de
    // gltfx_err noexcept e livre de alocacao.
    const glintfx::gltfx_rslt<void> result =
        glintfx::platform::validate_window_text_field("title", std::string_view{"a\0b", 3});

    g_force_alloc_failure = false;

    // So' alcancavel depois do conserto (F4) - ver o comentario da
    // fatia acima. Nao e' o que este caso mede hoje, mas fica pronto
    // para F4 herdar.
    const std::size_t override_calls = g_override_new_call_count;
    std::println("err_copy_terminates_window_validation_test: sobreviveu ao alocador armado, "
                 "{} chamada(s) reais a operator new observadas",
                 override_calls);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::invalid_argument);
}
