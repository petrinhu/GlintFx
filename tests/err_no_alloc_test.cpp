// SPDX-License-Identifier: AGPL-3.0-or-later
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// err_no_alloc_test.cpp - CE-2 of CORE-ERROR (TODO.md, GODS_LAWS.md
// L-20): proves, by COUNTING, that constructing, copying, moving,
// assigning and destroying a CONTEXT-LESS gltfx_err never touches the
// heap. "Never allocates" in err.hpp's own comment is a claim this
// file exists to check, not to trust.
//
// TECHNIQUE: this translation unit replaces the GLOBAL operator
// new/delete (scalar and array forms) for the whole process it links
// into - a standard heap-profiling idiom, safe here because
// glintfx_add_test() (cmake/GlintfxTest.cmake) gives every test case
// its OWN executable; no other TU in this binary defines these
// symbols, so there is no ODR collision.
//
// DECLARED COVERAGE, honestly, not discovered by a future reader the
// hard way: in a STATIC build (BUILD_SHARED_LIBS=OFF) this genuinely
// proves zero heap allocation for the WHOLE call, because glintfx and
// this test link into ONE binary and share one allocator with no DSO
// boundary at all. In a SHARED build on Linux/ELF, a strong global
// operator new defined in the executable is well-established to
// interpose the .so's own calls too (the same technique heap-profiling
// tools rely on) - expected to hold here, and this test does not
// special-case it. On Windows in SHARED mode, a DLL linking its own
// dynamic CRT import table can resolve operator new INSIDE that CRT
// rather than through this executable's override, which would make a
// genuinely non-allocating call inside the library invisible to the
// counters below (a false negative for THIS test, never a false
// positive - it could only make a real bug harder to see on that one
// leg, not report one that is not there). Declared, not silently
// assumed.

namespace {

// ATOMIC, NAO std::size_t PLANO (achado ERR-COPY-RED, GODS_LAWS.md
// L-40/L-43, mesmo mecanismo ja documentado em fixation_write_and_err_
// copy_oom_test.cpp): com contadores planos e -O2, o GCC reordena a
// leitura "antes"/"depois" em torno da chamada real a operator new
// (reconhece a ASSINATURA como a `new` builtin e aplica suas proprias
// suposicoes de efeito colateral, mesmo com o simbolo substituido) - a
// chamada acontece de verdade, mas o delta lido pelo teste pode dar
// ZERO mesmo assim. Isto nunca mordeu os casos JA' existentes neste
// arquivo porque a verdade de campo deles TAMBEM e' zero (um
// gltfx_err sem contexto genuinamente nao aloca) - um falso-zero e um
// zero verdadeiro sao indistinguiveis ali. O caso novo abaixo
// (context_bearing_error_copy_allocates_zero_times) tem verdade de
// campo DIFERENTE DE ZERO hoje (a copia com contexto aloca sempre,
// ESCOPO.md Decisao 17) - um falso-zero aqui daria um VERDE FALSO
// exatamente onde a fatia F2 precisa do vermelho verdadeiro. std::atomic
// e' a barreira de otimizacao que fecha esse buraco, comprovada nesta
// mesma sessao (script isolado, delta correto com atomic mesmo sem
// -fno-builtin).
std::atomic<std::size_t> g_alloc_count{0};
std::atomic<std::size_t> g_dealloc_count{0};

void reset_counts() {
    g_alloc_count = 0;
    g_dealloc_count = 0;
}

} // namespace

void *operator new(std::size_t size) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void *p) noexcept {
    g_dealloc_count.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete(void *p, std::size_t /*size*/) noexcept { ::operator delete(p); }

void operator delete[](void *p) noexcept { ::operator delete(p); }

void operator delete[](void *p, std::size_t /*size*/) noexcept { ::operator delete(p); }

GLINTFX_TEST(context_less_error_lifecycle_never_allocates) {
    reset_counts();

    {
        const glintfx::gltfx_err err(glintfx::gltfx_err_code::not_found);
        glintfx::gltfx_err copy_constructed(err);
        const glintfx::gltfx_err move_constructed(std::move(copy_constructed));

        glintfx::gltfx_err copy_assigned(glintfx::gltfx_err_code::unknown);
        copy_assigned = err;

        glintfx::gltfx_err move_assigned(glintfx::gltfx_err_code::unknown);
        move_assigned = std::move(copy_assigned);

        GLINTFX_CHECK(move_constructed.code() == glintfx::gltfx_err_code::not_found);
        GLINTFX_CHECK(move_assigned.code() == glintfx::gltfx_err_code::not_found);
    }

    // SNAPSHOT IMMEDIATELY (CI finding, CORE-ERROR, 25/08/2026): the
    // measured region above is closed, its destructors already ran -
    // this is the FIRST line of code afterward, before ANYTHING else,
    // including std::println itself, gets a chance to run. Freezing
    // the counts here, into local consts, is what makes the PRINTED
    // numbers and the CHECKED numbers the SAME numbers, guaranteed,
    // not just usually the same.
    //
    // WHY THIS MATTERS, measured live: Ubuntu's CI leg failed with
    // exactly this shape - the println line reported "0 allocation(s),
    // 0 deallocation(s)" while the checks on the SAME two counters,
    // two lines later, failed claiming they were NOT zero. That is not
    // a contradiction in the type under test; it is std::println's OWN
    // arguments being evaluated (captured) BEFORE the call, while
    // std::println's internal implementation (format/write a buffer)
    // can itself allocate and free AFTER that capture, on a libstdc++
    // version whose <print>/<format> backend does so (a throwaway probe
    // on this session's build machine, GCC 16, showed ZERO allocations
    // for the identical call shape - this failure needs an OLDER or
    // DIFFERENT <print>/<format> backend to manifest, exactly what
    // Ubuntu's CI leg ships and this machine does not). The GLOBAL
    // counter the OLD code re-read for the checks was therefore reading
    // a value already contaminated by println's own overhead, not by
    // gltfx_err's lifecycle. Reproduced live in THIS file, on THIS
    // machine, by temporarily injecting an equivalent alloc+free right
    // after println() and observing the exact same failure text - see
    // the commit that introduced this fix for the captured red/green
    // pair.
    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;

    // L-40: the counts that decide PASS/FAIL are printed even when
    // they pass - a portal that scans nothing and prints green is the
    // defect this project's gates exist to never ship.
    std::println("err_no_alloc_test: {} allocation(s), {} deallocation(s) across construct, "
                 "copy, move, copy-assign, move-assign, destroy of a context-less gltfx_err",
                 final_alloc_count, final_dealloc_count);

    GLINTFX_CHECK(final_alloc_count == 0);
    GLINTFX_CHECK(final_dealloc_count == 0);
}

// context_bearing_error_copy_allocates_zero_times - T2 da onda
// W-ERRCOPY (TODO.md ERR-COPY-RED, ESCOPO.md Decisao 17,
// GODS_LAWS.md L-20/L-35): o caso ACIMA so' prova que um gltfx_err
// SEM contexto nunca aloca - o proprio nome dele diz "of a
// context-less gltfx_err". Nenhum caso deste arquivo, ate' esta
// fatia, contou uma copia de um erro QUE CARREGA contexto. A Decisao
// 17 fixa que essa copia tambem deve ficar livre de alocacao
// (contagem de referencia intrusiva + copia-na-escrita); hoje ela
// aloca SEMPRE que ha' contexto (src/core/err.cpp:33-37, `new
// err_context(*other.m_context)`), entao este caso e' vermelho por
// construcao, nao por acidente.
//
// ISOLAMENTO DA MEDICAO: attach (with_rejected_value) aloca de
// verdade (a criacao do err_context em si) - isso e' esperado e NAO
// e' o que este caso mede. reset_counts() roda DEPOIS do attach,
// ANTES da copia, para que o unico delta contado seja o da copia em
// si, o unico ponto que a Decisao 17 promete consertar.
GLINTFX_TEST(context_bearing_error_copy_allocates_zero_times) {
    glintfx::gltfx_err original(glintfx::gltfx_err_code::not_found);
    // "adapter" cabe folgado no buffer de small-string-optimization de
    // libstdc++/MSVC (tipicamente 15-22 bytes) - a unica alocacao real
    // do attach e' a do proprio err_context, nao uma segunda para o
    // conteudo da string.
    original.with_rejected_value("adapter");

    reset_counts(); // a partir daqui, so' a copia abaixo e' medida

    const glintfx::gltfx_err copy(original);

    // Mesma disciplina SNAPSHOT-IMEDIATAMENTE do caso acima: congela os
    // dois contadores antes de qualquer outra coisa (inclusive
    // std::println) ter chance de rodar.
    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;

    // L-40: contagem impressa mesmo quando (ainda) nao e' zero - o
    // ponto desta fatia e' justamente que ela NAO e' zero hoje.
    std::println("err_no_alloc_test: {} allocation(s), {} deallocation(s) copying a gltfx_err "
                 "WITH an attached context (ESCOPO.md Decisao 17: deveria ser zero e zero)",
                 final_alloc_count, final_dealloc_count);

    // Sanidade: a copia e' de verdade uma copia funcional, nao um erro
    // vazio - se este check falhar, o vermelho seria pelo motivo
    // errado (a copia nao aconteceu como esperado), nao pelo defeito
    // de alocacao que este caso existe para provar.
    GLINTFX_CHECK(copy.rejected_value() == std::string_view{"adapter"});

    // O CHECK QUE HOJE REPROVA (ESCOPO.md Decisao 17): a copia com
    // contexto anexado aloca >=1 vez hoje; deve virar 0 na fatia F4.
    GLINTFX_CHECK(final_alloc_count == 0);
}
