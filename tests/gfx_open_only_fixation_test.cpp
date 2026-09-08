// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <print>
#include <span>
#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_open_only_fixation.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_open_only_fixation_test.cpp - C-OPT (docs/plano-w6b-placa-e-
// laco.md sec. 14.1/14.2, D-W6b-25, GODS_LAWS.md L-20): the TDD red/
// green witness for glintfx::platform::resolve_gfx_open_only_fixation()
// (src/platform/gl/gfx_open_only_fixation.hpp).
//
// RED, SEEN: before gfx_open_only_fixation.{hpp,cpp} existed, this
// file's own #include line failed to compile.
//
// THE 6-CELL ENUMERATION (sec. 14.2's own wording, verbatim): {nada
// fixado, igual, diferente} x {pedida, omitida}. Exercised below over
// the registry's own three `open_only` options (gpu_preference id 2,
// msaa_samples id 3, srgb_framebuffer id 4 - sec. 11.2), all with
// default 0, using msaa_samples as the ONE option each case varies -
// its own [0, 16] range gives room for a value clearly different from
// its own default (0) without touching the shape gfx_option_
// validation_test.cpp already covers.

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_entry;
using glintfx::platform::gfx_open_only_fixation_outcome;
using glintfx::platform::gfx_open_only_fixation_result;
using glintfx::platform::resolve_gfx_open_only_fixation;

namespace {

// The complete open_only set with every default (gpu_preference=0,
// msaa_samples=0, srgb_framebuffer=0) - what cell 2 (nada fixado,
// omitida) below fixes, and what cell 4 (igual, omitida) reuses as
// "what was already fixed".
const std::vector<gltfx_gfx_option_entry> k_all_default_fixed{
    {gltfx_gfx_option::gpu_preference, 0},
    {gltfx_gfx_option::msaa_samples, 0},
    {gltfx_gfx_option::srgb_framebuffer, 0},
};

// The complete open_only set with msaa_samples fixed at 4 (non-
// default) - what cell 1 (nada fixado, pedida) below fixes, and what
// cells 5/6 (diferente) reuse as "what was already fixed".
const std::vector<gltfx_gfx_option_entry> k_msaa_four_fixed{
    {gltfx_gfx_option::gpu_preference, 0},
    {gltfx_gfx_option::msaa_samples, 4},
    {gltfx_gfx_option::srgb_framebuffer, 0},
};

bool fixed_set_matches(const std::vector<gltfx_gfx_option_entry> &actual,
                       const std::vector<gltfx_gfx_option_entry> &expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (const gltfx_gfx_option_entry &want : expected) {
        bool found = false;
        for (const gltfx_gfx_option_entry &got : actual) {
            if (got.id == want.id && got.value == want.value) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// GODS_LAWS.md L-27/docs/api-conventions.md R3 witness: forces the
// std::vector::push_back() inside resolve_gfx_open_only_fixation()'s
// own `fix_now` branch to fail with std::bad_alloc, and proves the
// noexcept function degrades to `alloc_failed` instead of calling
// std::terminate() (the exact "a lib NUNCA aborta o processo do
// consumidor" the leader's OOM decision forbids).
//
// SEGUNDA REPROVACAO (GODS_LAWS.md L-22, run 34178782241, job "Windows
// - Debug"): a versao anterior deste gancho era um booleano ligado
// pela CHAMADA INTEIRA - fazia TODA alocacao falhar, inclusive a que a
// PROPRIA std::vector faz por dentro ao ser default-construida.
// PESQUISADO, nao suposto: no MSVC em build Debug (_ITERATOR_DEBUG_
// LEVEL == 2, o default de Debug - learn.microsoft.com/cpp/standard-
// library/iterator-debug-level, e learn.microsoft.com/cpp/standard-
// library/debug-iterator-support, ambos consultados 07/09/2026), o
// PROPRIO construtor default de std::vector chama `_Alloc_proxy()`
// pra alocar um `_Container_proxy` de bookkeeping de iterador -
// mesmo para um vetor vazio (github.com/microsoft/STL, stl/inc/
// vector: `vector() noexcept(is_nothrow_default_constructible_v<
// _Alty>) { ...; _Mypair._Myval2._Alloc_proxy(...); }`) - e o
// compilador declara esse construtor noexcept mesmo assim, porque
// `is_nothrow_default_constructible_v<std::allocator<T>>` e
// `true` sem olhar pro que `_Alloc_proxy` faz por dentro. Se essa
// alocacao interna falha enquanto o gancho antigo fazia TUDO falhar,
// a excecao tenta escapar de um construtor que o proprio MSVC marcou
// noexcept - e o runtime chama std::terminate() ALI, dentro do
// construtor do vector, ANTES de a pilha sequer voltar pro try{} de
// resolve_gfx_open_only_fixation(). Por isso a fatia WIN-DEBUG-
// CTORALLOC (mover `result{}` pra dentro do try{}, commit 2d8d12c) nao
// mudou nada: aquele try{} nunca chega a rodar - quem quebra o proprio
// contrato noexcept e o std::vector do MSVC, nao o nosso codigo, e
// nenhum try/catch nosso alcanca uma fronteira noexcept mais funda que
// a nossa propria. O DEFEITO E DESTE GANCHO DE TESTE, nao do produto
// (fixation.cpp): forcar TODA alocacao a falhar simula um OOM mais
// bruto do que o teste precisa provar, e derruba bookkeeping interno
// do STL que o produto nao tem como proteger nem devia precisar
// proteger - so a alocacao de CRESCIMENTO do push_back() e a que
// importa (essa sim propaga normalmente, por push_back() nao ser
// noexcept, e e exatamente o que o catch(const std::bad_alloc&) em
// gfx_open_only_fixation.cpp trata).
//
// CONSERTO: em vez de um booleano ligado pela chamada inteira, um
// contador de alocacao com "falha exatamente na N-esima, depois se
// desarma sozinho". O teste no fim deste arquivo CALIBRA `N` medindo,
// em runtime, quantas alocacoes uma gfx_open_only_fixation_result
// VAZIA custa NESTE compilador (zero no Linux/libstdc++, uma no MSVC
// Debug pelo motivo acima) - e so entao arma a falha forcada na
// alocacao SEGUINTE a essa, que e a de verdade (a que push_back() pede
// pra crescer o vetor). Como a falha dispara uma unica vez e se
// desarma, a PROPRIA construcao do resultado `alloc_failed` no catch
// (que tambem constroi um vector vazio) nunca tropeca na mesma
// armadilha - sem precisar filtrar por tamanho de alocacao (sizeof do
// `_Container_proxy` do MSVC e sizeof(gltfx_gfx_option_entry) colidem
// em 16 bytes no x86-64, medido por leitura das duas structs, entao
// filtrar por tamanho seria fragil por coincidencia). Internal linkage
// (mesma ideia de err_context_test.cpp's own g_force_alloc_failure):
// so o override de operator new abaixo e o teste no fim deste arquivo
// tocam nisso.
//
// PROVADO POR EXECUCAO, nao so por doc (GODS_LAWS.md L-27, tools/msvc-
// container/, cl.exe real 19.51.36256, 07/09/2026): compilar este
// arquivo com `/MDd` e desmontar o `.obj` de gfx_open_only_fixation.cpp
// com `dumpbin /disasm` mostra, literalmente, `std::_Container_base12
// ::_Alloc_proxy<std::allocator<std::_Container_proxy>>` chamando
// `allocator<_Container_proxy>::allocate()` na instanciacao de
// `std::vector<glintfx::gltfx_gfx_option_entry>` - o mecanismo do
// paragrafo acima nao e inferencia, e o codigo que o compilador real
// da Microsoft de fato gera. Rodar esse mesmo binario sob `wine64`
// morre com codigo 53 sem imprimir nada, ANTES desta fatia e DEPOIS
// dela igualmente - limitacao conhecida e ja documentada do runtime de
// depuracao (`/MDd`) sob este Wine (tools/msvc-container/README.md),
// nao algo que esta fatia introduziu nem pode contornar; a unica prova
// de execucao de ponta a ponta continua sendo o job `windows` do CI.
std::size_t g_alloc_count = 0;
std::optional<std::size_t> g_fail_at_alloc_number;

} // namespace

// Global replacement, same shape as err_context_test.cpp's own -
// gfx_open_only_fixation.cpp carries no GLINTFX_API (L-19, "nada e
// exportado") and is recompiled straight into THIS test binary (tests/
// CMakeLists.txt's own target_sources() for gfx_open_only_fixation_
// test), so there is no DLL boundary to cross and no need for that
// file's own win_dll_alloc_hook.hpp companion.
void *operator new(std::size_t size) {
    ++g_alloc_count;
    if (g_fail_at_alloc_number.has_value() && g_alloc_count == *g_fail_at_alloc_number) {
        // Dispara uma unica vez: quem chamou nao precisa desarmar de
        // volta, e a alocacao SEGUINTE (a que o catch de fixation.cpp
        // faz pra construir o `alloc_failed` de retorno) tem que
        // suceder normalmente.
        g_fail_at_alloc_number.reset();
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

// Cell (nada fixado, pedida): no context has opened this window yet,
// and msaa_samples is explicitly requested at 4 - fix_now with the
// COMPLETE set, requested value where given, default everywhere else.
GLINTFX_TEST(gfx_open_only_fixation_nothing_fixed_requested_fixes_now_with_the_requested_value) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(fixed_set_matches(result.fixed, k_msaa_four_fixed));
}

// Cell (nada fixado, omitida): no context has opened this window yet,
// and the opening list is empty - fix_now with every open_only option
// at its own default.
GLINTFX_TEST(gfx_open_only_fixation_nothing_fixed_omitted_fixes_now_with_every_default) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(fixed_set_matches(result.fixed, k_all_default_fixed));
}

// Cell (igual, pedida): a first context already fixed msaa_samples=4,
// and the second open() explicitly asks for the SAME value - accept.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_requested_the_same_value_accepts) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::accept);
}

// Cell (igual, omitida): a first context already fixed every option at
// its own default, and the second open() omits them all (resolving to
// the SAME defaults) - accept.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_omitted_resolving_to_the_same_default_accepts) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_all_default_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::accept);
}

// Cell (diferente, pedida): a first context already fixed
// msaa_samples=4, and the second open() explicitly asks for a
// DIFFERENT value - refuse, naming msaa_samples.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_requested_a_different_value_refuses_by_id) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 8}};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::refuse);
    GLINTFX_CHECK(result.refused_id == gltfx_gfx_option::msaa_samples);
}

// Cell (diferente, omitida): a first context already fixed
// msaa_samples=4 (non-default), and the second open() omits it -
// resolving to the DEFAULT (0), which disagrees with what is fixed -
// refuse, naming msaa_samples.
GLINTFX_TEST(
    gfx_open_only_fixation_already_fixed_omitted_resolving_to_a_different_default_refuses) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::refuse);
    GLINTFX_CHECK(result.refused_id == gltfx_gfx_option::msaa_samples);

    std::println("gfx_open_only_fixation: 6/6 cells of {{nada fixado, igual, diferente}} x "
                 "{{pedida, omitida}} checked");
}

// INBOX (drenagem de 06/09/2026): "uma funcao que promete nunca falhar
// pode derrubar o processo do consumidor por falta de memoria" - the
// `fix_now` branch (nothing fixed yet) is the one that grows `result.
// fixed` with push_back(); forcing operator new to throw mid-loop used
// to escape this noexcept function and call std::terminate(). Armed
// only around the one call under test, so the harness's own printing
// above/below never sees a forced failure.
//
// SEGUNDA REPROVACAO (GODS_LAWS.md L-22, ver o comentario do gancho
// acima pra causa e fontes): calibra `baseline_allocs` medindo quantas
// alocacoes uma gfx_open_only_fixation_result VAZIA custa NESTE
// compilador, e so arma a falha forcada na alocacao SEGUINTE a essa -
// a de verdade, a que push_back() pede - em vez de derrubar TODA
// alocacao da chamada, inclusive bookkeeping interno do STL que nem o
// produto nem este teste tem como (ou devem precisar) proteger.
GLINTFX_TEST(gfx_open_only_fixation_out_of_memory_degrades_instead_of_terminating) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};

    g_alloc_count = 0;
    {
        const gfx_open_only_fixation_result probe{};
    } // calibracao, ver o gancho acima
    const std::size_t baseline_allocs = g_alloc_count;

    g_alloc_count = 0;
    g_fail_at_alloc_number = baseline_allocs + 1;
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    g_fail_at_alloc_number.reset(); // idempotente - ja se desarma sozinho ao disparar

    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::alloc_failed);
    GLINTFX_CHECK(result.fixed.empty());
}
