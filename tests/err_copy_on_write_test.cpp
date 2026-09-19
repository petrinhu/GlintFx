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

// err_copy_on_write_test.cpp - T4 e T5 da onda W-ERRCOPY (TODO.md
// ERR-COPY-FIX, ESCOPO.md Decisao 17, GODS_LAWS.md L-17/L-20/L-22/L-35
// do projeto): guardas de REGRESSAO da semantica nova (contexto
// compartilhado + copia-na-escrita, src/core/err.cpp), nao TDD -
// declarado com todas as letras porque chamar isto de "vermelho
// primeiro" seria mentira (plano da onda W-ERRCOPY secao 7, linha T4:
// "nao e' teste vermelho-primeiro, e' guarda de regressao da semantica
// nova").
//
// T4 - "escrever numa copia nao muda o original": prova a metade que
// tests/err_context_test.cpp::copy_owns_an_independent_context (CE-3,
// ja existente) NAO cobre - aquele caso escreve no ORIGINAL depois de
// copiar e confere que a COPIA fica intacta; T4 e' o espelho exato,
// escreve na COPIA e confere que o ORIGINAL fica intacto. As duas
// direcoes importam porque a copia-na-escrita destaca (clona) o lado
// que ESCREVE, nao um lado fixo - um regressao que so' destacasse a
// copia (nunca o original) passaria no caso antigo e falharia neste.
//
// T5 - "destacar sob falta de memoria degrada sem corromper o outro
// dono": prova o CAMINHO DE FALHA que so' passou a existir com esta
// fatia (TODO.md ERR-COPY-FIX: "ensure_context() ganha destacar-antes-
// de-escrever, degradando em silencio... o mesmo contrato que with_*()
// ja tem hoje"). Reusa a MESMA tecnica de alocador armado que T3 (err_
// copy_terminates_window_validation_test.cpp) e tests/fixation_write_
// and_err_copy_oom_test.cpp ja' estabelecem - GODS_LAWS.md L-17 do
// projeto (nao reinventar).

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

// clang-tidy cert-dcl54-cpp/misc-new-delete-overloads: a TU declaring
// `operator delete[]` (below) needs a matching `operator new[]` at the
// same scope - same idiom tests/err_no_alloc_test.cpp already uses.
void *operator new[](std::size_t size) { return ::operator new(size); }

// Mesmo achado ja documentado em tests/err_copy_terminates_window_
// validation_test.cpp e tests/fixation_write_and_err_copy_oom_test.cpp:
// com a cadeia inteira new -> string -> delete inlineada nesta TU, GCC
// reclama que std::free() nao e' "o desalocador emparelhado" de
// operator new() plantado aqui, mesmo sendo o mesmo par malloc/free por
// baixo em glibc. Suprimido localmente, razao nomeada (GODS_LAWS.md
// L-32: nunca afrouxar warning fora do escopo cirurgico do pedido).
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

// T4: escrever atraves de uma copia COMPARTILHADA nao vaza pro
// original, e o original continua legivel e correto depois.
GLINTFX_TEST(write_through_copy_does_not_mutate_shared_original) {
    glintfx::gltfx_err original(glintfx::gltfx_err_code::io_failure);
    original.with_path("original/path.txt").with_position(1, 1);

    // Neste ponto, `copy` e `original` COMPARTILHAM o mesmo err_context
    // (ESCOPO.md Decisao 17) - a copia acima nao alocou nada (T2 ja
    // prova isso por contagem; aqui o que importa e' a OBSERVACAO).
    const glintfx::gltfx_err copy(original);
    GLINTFX_CHECK(copy.path() == std::string_view{"original/path.txt"});
    GLINTFX_CHECK(copy.line() == 1);

    // ESCREVER NA COPIA e' o que este caso testa - a direcao que
    // err_context_test.cpp::copy_owns_an_independent_context NAO
    // cobre (aquele escreve no original). with_*() sobre um `const
    // gltfx_err` nao compilaria, entao a copia aqui precisa ser
    // mutavel - deliberado, e' exatamente o lado que se quer mutar.
    glintfx::gltfx_err mutable_copy(original);
    mutable_copy.with_path("copy/path.txt").with_position(7, 7);

    // O ORIGINAL nao pode ter mudado - se a copia-na-escrita destacar
    // o lado ERRADO (ou nao destacar nenhum), esta e' a asserção que
    // reprova.
    GLINTFX_CHECK(original.path() == std::string_view{"original/path.txt"});
    GLINTFX_CHECK(original.line() == 1);
    GLINTFX_CHECK(original.column() == 1);

    // E a copia mutavel de fato mudou.
    GLINTFX_CHECK(mutable_copy.path() == std::string_view{"copy/path.txt"});
    GLINTFX_CHECK(mutable_copy.line() == 7);
    GLINTFX_CHECK(mutable_copy.column() == 7);

    // A OUTRA copia (const, nunca escrita) tambem nao pode ter mudado -
    // prova que o destacar de `mutable_copy` nao afeta um TERCEIRO
    // dono do mesmo contexto compartilhado, so' o proprio.
    GLINTFX_CHECK(copy.path() == std::string_view{"original/path.txt"});
    GLINTFX_CHECK(copy.line() == 1);
}

// T5: se o DESTACAR (clonar o contexto compartilhado antes de
// escrever) falhar por falta de memoria, o with_*() degrada em
// silencio - devolve *this SEM MUDAR NADA, nunca lanca, nunca
// corrompe o outro dono. Mesmo contrato que "ATTACH IS BEST-EFFORT"
// (err.hpp) ja documentava para o caso "contexto ainda nao existe";
// esta fatia estende o MESMO contrato pro caso novo "contexto existe
// mas esta' compartilhado".
//
// with_position() DE PROPOSITO, NAO with_path()/with_rejected_value():
// with_position() so' escreve dois campos escalares depois de
// ensure_context() - "Plain scalar writes: cannot throw, no try/catch
// needed" (err.cpp). Isola a medicao no PRORIO destacar, sem uma
// segunda alocacao possivel (std::string::assign() crescendo alem do
// buffer de small-string-optimization) confundir qual chamada de
// operator new falhou.
//
// FORMA DE ARMAR, PRECISA (mesmo idioma de err_copy_terminates_window_
// validation_test.cpp): `original.with_rejected_value("seed")` faz 1
// chamada real a operator new (ensure_context() - forma NOTHROW,
// `new(std::nothrow) err_context()`; "seed" cabe em SSO, sem segunda
// alocacao). `glintfx::gltfx_err copy(original)` NAO aloca nada (T2).
// A PROXIMA chamada real a operator new e', portanto, a 1a APOS ARMAR:
// `ensure_context()` de `copy.with_position(...)` destacando o
// contexto compartilhado - `new err_context(*m_context)`, a forma QUE
// LANCA (nao a nothrow). g_calls_to_allow_before_failure=0 forca ESSA
// primeira chamada pos-armamento a falhar direto.
GLINTFX_TEST(detach_under_forced_oom_degrades_without_corrupting_shared_owner) {
    glintfx::gltfx_err original(glintfx::gltfx_err_code::io_failure);
    original.with_rejected_value("seed");

    glintfx::gltfx_err copy(original); // compartilha, zero alocacao

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0;

    glintfx::gltfx_err &result_ref = copy.with_position(9, 9);

    g_force_alloc_failure = false;

    const std::size_t override_calls = g_override_new_call_count;
    std::println("err_copy_on_write_test: sobreviveu ao alocador armado durante o destacar, "
                 "{} chamada(s) reais a operator new observadas",
                 override_calls);

    // Best-effort: with_position() devolve *this, sempre (mesmo
    // objeto, mesmo endereco) - a assinatura publica (err.hpp) promete
    // encadeamento incondicional.
    GLINTFX_CHECK(&result_ref == &copy);

    // NADA foi escrito na copia: o destacar falhou DENTRO de
    // ensure_context(), ANTES de qualquer escrita escalar, entao a
    // copia continua lendo o valor COMPARTILHADO original (linha e
    // coluna ainda 0, "seed" ainda o rejected_value) - nao 9/9, o que
    // indicaria que a escrita vazou pro contexto AINDA compartilhado
    // com o original (corrupcao, nao degradacao).
    GLINTFX_CHECK(copy.line() == 0);
    GLINTFX_CHECK(copy.column() == 0);
    GLINTFX_CHECK(copy.rejected_value() == std::string_view{"seed"});

    // O OUTRO DONO (original) nao pode ter sido afetado pelo destacar
    // que falhou - continua legivel, com o mesmo valor de sempre. Esta
    // e' a asserção que provaria corrupcao (ex.: contador de
    // referencia deixado inconsistente, ponteiro pendurado, escrita
    // vazada pro contexto compartilhado) se o caminho de falha do
    // destacar estivesse errado.
    GLINTFX_CHECK(original.line() == 0);
    GLINTFX_CHECK(original.column() == 0);
    GLINTFX_CHECK(original.rejected_value() == std::string_view{"seed"});

    // Depois da falha forcada, um SEGUNDO with_position() (alocador
    // normal outra vez) precisa funcionar - prova que o estado
    // compartilhado sobreviveu intacto ao destacar que falhou, sem
    // deixar `copy` presa num estado que nunca mais aceita escrita.
    copy.with_position(9, 9);
    GLINTFX_CHECK(copy.line() == 9);
    GLINTFX_CHECK(copy.column() == 9);
    GLINTFX_CHECK(copy.rejected_value() == std::string_view{"seed"});

    // E o original CONTINUA intocado depois desse segundo
    // with_position() bem-sucedido - a prova final de que o destacar,
    // quando funciona, de fato isola os dois donos.
    GLINTFX_CHECK(original.line() == 0);
    GLINTFX_CHECK(original.column() == 0);
    GLINTFX_CHECK(original.rejected_value() == std::string_view{"seed"});
}
