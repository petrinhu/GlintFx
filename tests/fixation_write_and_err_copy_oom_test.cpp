// SPDX-License-Identifier: AGPL-3.0-or-later
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <optional>
#include <print>
#include <span>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "harness/win_dll_alloc_hook.hpp"
#include "platform/gl/gfx_open_only_fixation.hpp"

// fixation_write_and_err_copy_oom_test.cpp - FIX-OOM-B9 (conserto
// 19/09/2026, GODS_LAWS.md L-17/L-20/L-22/L-27 do projeto): prova, sob
// alocador armado, os DOIS mecanismos que os dois sitios reais de
// FAMILIA B consertados nesta fatia dependem - gl_context_facade.cpp
// (`gltfx_gl_context::open()`, o `fix_now` branch) e gpu_enumeration_
// facade.cpp (`gltfx_gpu_enumeration::query()`, ramo Windows).
//
// ESCOPO DECLARADO, HONESTO (L-17/api-conventions.md R3 - resultado
// negativo honesto vale mais que teste inventado): este arquivo NAO
// chama `gltfx_gl_context::open()` nem `gltfx_gpu_enumeration::query()`
// diretamente.
//   - `open()` so' alcanca o ramo `fix_now` DEPOIS de um `adapter.
//     open()` genuinamente bem-sucedido - exige uma janela Wayland/EGL
//     real, aberta, o que por sua vez exige o compositor aninhado
//     isolado que GODS_LAWS.md L-09 manda pra QUALQUER teste que toque
//     tela (ver tests/container/). Essa infraestrutura de OOM-forcing
//     dentro de um fixture de container NAO EXISTE hoje (tests/
//     container/alloc_counter_hook.cpp so' CONTA alocacao, nunca FORCA
//     falha) - construi-la e' trabalho de infraestrutura proprio,
//     desproporcional a duas correcoes cirurgicas, e fica NOMEADO aqui
//     como lacuna, nao escondido.
//   - `query()`'s own ramo consertado inteiro vive atras de `#if
//     defined(_WIN32)` - nao compila em Linux, ponto final. So' o CI
//     real do Windows prova aquele ramo especifico.
//
// O QUE ESTE ARQUIVO PROVA DE VERDADE, COM OS TIPOS REAIS DO PROJETO
// (nao uma reproducao generica de `optional<vector>`/`gltfx_err`):
//   1. `gfx_open_only_fixation_result::fixed` (o MESMO tipo que
//      window_impl.hpp's own `fixed_open_only_gfx_options` guarda) e'
//      um `std::vector<gltfx_gfx_option_entry>` genuino, produzido pela
//      FUNCAO REAL `resolve_gfx_open_only_fixation()` - e movê-lo pro
//      slot do tipo exato que gl_context_facade.cpp escreve (`std::
//      optional<std::vector<gltfx_gfx_option_entry>>`) nao aloca UMA
//      VEZ SEQUER, mesmo com o alocador armado pra falhar em toda
//      chamada (o `noalloc` positivo, GODS_LAWS.md L-43: a forma que
//      NAO deveria alocar, provada por contagem, nao so' por nao ter
//      crashado).
//   2. `glintfx::gltfx_err` (o mesmo tipo dos dois sitios) tem copia
//      PROFUNDA quando carrega contexto (include/glintfx/core/err.hpp,
//      "LIFECYCLE" - copy ctor exportado, nao noexcept) - sob alocador
//      armado essa copia lanca `std::bad_alloc` CAPTURAVEL de verdade
//      (ao contrario da familia A/c6, onde nem o `catch` salva - ver o
//      comentario corrigido em gl_context_facade.cpp), e o construtor
//      trivial de codigo-so' (`gltfx_err(gltfx_err_code)`) nunca aloca,
//      nem sob o mesmo alocador armado - a prova de que o degrau 2
//      (capturar e converter) usado em gpu_enumeration_facade.cpp
//      genuinamente funciona para ESTE tipo.
//
// MUTACAO (GODS_LAWS.md L-27): reverter QUALQUER um dos dois sitios
// reais para a forma antiga (copia em vez de mover; sem o try/catch)
// nao derruba este arquivo automaticamente - ele testa o MECANISMO,
// nao o sitio de producao (ver o paragrafo de escopo acima). A mutacao
// que ESTE arquivo pega e' no MECANISMO em si: se `gltfx_err`'s own
// copy ctor deixasse de alocar quando carrega contexto, ou se o
// construtor trivial passasse a alocar, os casos abaixo reprovariam.
//
// POR QUE OS CONTADORES SAO std::atomic, NAO std::size_t/bool PLANOS
// (achado medido, nao copiado de outro arquivo deste diretorio):
// drm_device_facts_oom_test.cpp usa contadores planos com seguranca
// porque o codigo sob teste mora numa .cpp SEPARADA (compilada de
// novo, mas ainda assim uma chamada de funcao opaca pro otimizador,
// sem LTO). ESTE arquivo e' diferente - `std::optional<std::vector<T>>`
// e' inteiramente header-only, entao a copia/o move que os casos
// abaixo exercitam fica TODO INLINE, na MESMA unidade de traducao que
// define `operator new`. Medido ao vivo (script isolado, -O2, mesmo
// toolchain deste projeto): com contadores `std::size_t` planos, o GCC
// reordena a leitura "antes"/"depois" em torno da chamada real a
// `operator new` (o compilador reconhece a ASSINATURA como a `new`
// builtin e aplica as suas proprias suposicoes de efeito colateral,
// mesmo com o simbolo substituido) - a chamada ACONTECE de verdade
// (visivel por um `fprintf` dentro dela), mas o delta lido pelo teste
// dava ZERO mesmo assim. Com `-fno-builtin` ou em `-O0` o delta correto
// aparece. `std::atomic` (a mesma tecnica que tests/container/
// alloc_counter_hook.cpp ja usa, por motivo distinto - concorrencia)
// tambem fecha ESTE buraco: a leitura/escrita atomica e' uma barreira
// de otimizacao que o compilador nao pode reordenar em torno da
// chamada, comprovado da mesma forma (script isolado, delta correto
// com atomic mesmo sem `-fno-builtin`).

// ARMADILHA MEDIDA, NAO PRESUMIDA (achado do time-lead, servidor run que
// reprovou o pedido de juncao numero 10, tres legs Windows: compartilhado,
// Debug e Sanitizer - GODS_LAWS.md L-44/L-27): substituir `operator new`
// NESTA TRADUCTION UNIT so alcanca alocacao que acontece DENTRO DELA. No
// Linux, interposicao de simbolo ELF faz esse override valer tambem para
// o que a biblioteca (glintfx.so) aloca - MAS NO WINDOWS/SHARED NAO: PE/
// COFF nao tem interposicao global de simbolo, e cada modulo (glintfx.dll
// e este executavel) resolve `operator new` contra o proprio CRT no
// PROPRIO link-time do modulo. CASO 3 abaixo e' o UNICO dos quatro casos
// deste arquivo que cruza essa fronteira: `gltfx_err`'s own copy ctor e'
// DECLARADO em err.hpp mas DEFINIDO em err.cpp, exportado (GLINTFX_API) -
// ver a "LIFECYCLE" paragraph la (mesma razao de ABI, alocacao/
// desalocacao no MESMO lado do boundary). Os outros tres casos (mover/
// copiar um std::optional<std::vector<T>>, e o construtor trivial de
// gltfx_err) sao inteiramente header-only/inline - nunca cruzam para
// dentro da .dll, entao o override desta TU sempre alcanca.
//
// A MESMA armadilha ja mordeu err_context_test.cpp (CORE-ERROR, CI real
// em 25/08/2026, corrigida em harness/win_dll_alloc_hook.hpp - LEIA o
// cabecalho daquele arquivo antes de tocar o que segue). CASO 3 abaixo
// reusa exatamente aquele mecanismo (IAT patch de dentro de glintfx.dll,
// nao um segundo override desta TU) em vez de inventar um outro - este
// arquivo E' o segundo ponto de uso do dll_alloc_hook (win_dll_alloc_
// hook.hpp's own "CONFINED TO TEST" paragraph foi corrigido no mesmo
// commit que este comentario para nomear os dois arquivos).
//
// ESTE PROJETO TEM SEIS testes que substituem operator new
// (gfss_anb_parse_no_alloc_test, alloc_counter_classify_test, gfui_
// complex_match_resource_exhausted_test, drm_device_facts_oom_test,
// err_context_test, e este) - so' os dois que chamam simbolo exportado
// da biblioteca (err_context_test e este) podiam morder esta armadilha;
// os outros quatro nunca cruzam o boundary (contagem de chamada = zero
// na auditoria que motivou este comentario), entao nunca precisaram do
// dll_alloc_hook.

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

// CASO 3 UNICAMENTE (mesma decisao ja tomada em err_context_test.cpp's
// own oom_forcing_declared_not_applicable(), mesma citacao): sob MSVC
// AddressSanitizer, o proprio operator new da ASan vence por PRECEDENCIA
// DE LINKER sobre qualquer override de usuario linkado no mesmo binario
// (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "Overriding
// operator new and delete" - sem /INFERASANLIBS, nem este projeto passa)
// - E TAMBEM sobre o IAT patch de dentro da .dll que dll_alloc_hook
// instala (learn.microsoft.com/cpp/sanitizers/asan-runtime, "Function
// interception": ASan intercepta as primitivas de alocacao do CRT por
// hotpatch direto, nao por resolucao atraves da IAT do modulo que a
// importa - o MESMO fato, MESMA fonte, que ja bloqueia err_context_
// test.cpp's own dois casos de degradacao no leg windows-sanitizer).
// Os outros tres casos deste arquivo (1, 2, 4) NAO dependem disto: sao
// inteiramente inline/header-only nesta TU, e o leg Windows-Sanitizer
// real (servidor run que motivou este conserto) mediu os TRES passando
// - so' o caso que cruza pra dentro de glintfx.dll (CASO 3) reprovou nos
// tres legs shared, e so' ASan, entre eles, tem este segundo motivo
// documentado para continuar reprovando mesmo depois do dll_alloc_hook.
[[nodiscard]] bool err_copy_oom_forcing_declared_not_applicable() {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#else
    return false;
#endif
}

void declare_err_copy_oom_forcing_not_applicable() {
    std::println(stderr,
                 "fixation_write_and_err_copy_oom_test: "
                 "err_copy_with_attached_context_throws_a_catchable_bad_alloc_when_forced "
                 "declared NOT APPLICABLE under MSVC AddressSanitizer (learn.microsoft.com/cpp/"
                 "sanitizers/asan-known-issues + asan-runtime, same citation err_context_test.cpp "
                 "already gives in full: ASan's own operator new wins by linker precedence, AND "
                 "hotpatches the CRT allocation primitives directly, so neither this TU's override "
                 "nor win_dll_alloc_hook.hpp's IAT patch inside glintfx.dll ever gets a chance to "
                 "run - the assertion this case exists to prove would measure nothing)");
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
    ++g_override_new_call_count;
    if (should_fail_this_allocation()) {
        return nullptr;
    }
    return std::malloc(size);
}

// GCC 16's own -Wmismatched-new-delete (medido ao vivo compilando este
// arquivo, nao presumido): o mesmo par malloc()/free() que drm_device_
// facts_oom_test.cpp/err_no_alloc_test.cpp ja' usam sem aviso algum
// dispara AQUI porque, neste arquivo, o otimizador consegue inlinear a
// cadeia INTEIRA new -> std::vector -> ~vector -> delete dentro de UMA
// unica funcao de teste (a copia/o move exercitados ficam totalmente
// header-only, na MESMA TU do override) - o compilador entao rastreia
// a proveniencia do ponteiro e reclama que `std::free()` "nao e' o
// desalocador emparelhado" de `operator new()`, mesmo sendo o MESMO
// par malloc/free por baixo em glibc. Delegar a forma sized/nothrow pra
// esta (em vez de cada uma chamar std::free direto) nao bastou - o
// aviso ainda pega a raiz. Suprimido localmente, apenas para estas
// quatro funcoes, com a razao nomeada em vez de `-Wno-mismatched-new-
// delete` global (GODS_LAWS.md L-32: nunca afrouxar warning fora do
// escopo cirurgico do pedido).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// CASO 1 (gl_context_facade.cpp, `open()`'s own `fix_now` branch, POS-
// CONSERTO): a mesma forma exata `window_impl_ptr->fixed_open_only_
// gfx_options = std::move(fixation.fixed);` - alocador armado pra
// falhar em TODA chamada, e o move ainda assim nao faz uma unica
// alocacao (move-construtor de std::vector rouba o ponteiro, nunca
// chama new/malloc).
GLINTFX_TEST(fixation_fixed_moves_into_window_style_optional_without_allocating) {
    const std::vector<glintfx::gltfx_gfx_option_entry> empty_requested;
    glintfx::platform::gfx_open_only_fixation_result fixation =
        glintfx::platform::resolve_gfx_open_only_fixation(
            std::nullopt, std::span<const glintfx::gltfx_gfx_option_entry>(empty_requested));

    GLINTFX_CHECK(fixation.outcome == glintfx::platform::gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(!fixation.fixed.empty());

    // Mirrors window_impl.hpp's own field type exactly - see that
    // header's own struct window_impl.
    std::optional<std::vector<glintfx::gltfx_gfx_option_entry>> window_slot;

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // fail the VERY NEXT allocation, if any
    const std::size_t calls_before = g_override_new_call_count;
    window_slot = std::move(fixation.fixed); // the exact, post-fix production shape
    const std::size_t calls_after = g_override_new_call_count;
    g_force_alloc_failure = false;

    GLINTFX_CHECK_EQ(calls_after, calls_before);
    GLINTFX_CHECK(window_slot.has_value());
    GLINTFX_CHECK(!window_slot->empty());
}

// CASO 2, o CONTRASTE que prova o caso 1 nao e' trivial: a mesma
// escrita, mas por COPIA (a forma pre-conserto) - sob o MESMO alocador
// armado, aloca de verdade (o teste so' passa porque o alocador aqui
// deixa a copia por completar antes de armar a falha na proxima
// chamada; sob falha imediata ela lancaria std::bad_alloc, exatamente
// o [except.terminate] que matava o consumidor dentro de open()).
GLINTFX_TEST(fixation_fixed_copy_into_window_style_optional_does_allocate) {
    const std::vector<glintfx::gltfx_gfx_option_entry> empty_requested;
    const glintfx::platform::gfx_open_only_fixation_result fixation =
        glintfx::platform::resolve_gfx_open_only_fixation(
            std::nullopt, std::span<const glintfx::gltfx_gfx_option_entry>(empty_requested));
    GLINTFX_CHECK(fixation.outcome == glintfx::platform::gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(!fixation.fixed.empty());

    std::optional<std::vector<glintfx::gltfx_gfx_option_entry>> window_slot;

    const std::size_t calls_before = g_override_new_call_count;
    window_slot = fixation.fixed; // the PRE-fix shape, deliberately reproduced here only
    const std::size_t calls_after = g_override_new_call_count;

    GLINTFX_CHECK(calls_after > calls_before);
    GLINTFX_CHECK(window_slot.has_value());
    GLINTFX_CHECK(!window_slot->empty());
    // fixation.fixed itself is untouched by a copy - still readable,
    // still equal (unlike the move case above).
    GLINTFX_CHECK_EQ(fixation.fixed.size(), window_slot->size());
}

// CASO 3 (gpu_enumeration_facade.cpp, `query()`'s own Windows branch,
// PRIMEIRA METADE do conserto): copiar um gltfx_err QUE CARREGA
// CONTEXTO aloca de verdade, e sob alocador armado lanca std::bad_
// alloc CAPTURAVEL - ao contrario da familia A (c6, RELATORIO.md,
// citado no comentario corrigido de gl_context_facade.cpp), onde nem o
// catch salva. Isto e' o que torna o degrau 2 (capturar e converter)
// uma escolha honesta para ESTE sitio e nao para o outro.
//
// UNICO caso deste arquivo que cruza para dentro de glintfx.dll (ver o
// comentario de armadilha no topo do arquivo): `gltfx_err`'s own copy
// ctor e' GLINTFX_API, DEFINIDO em err.cpp. No Windows/SHARED, este
// TU's own operator new override (acima) nunca alcanca essa alocacao -
// dll_alloc_hook (harness/win_dll_alloc_hook.hpp) fecha o buraco
// patcheando a IAT de glintfx.dll de fora, o MESMO mecanismo que
// err_context_test.cpp's own allocator_reach_probe ja usa e prova
// funcionar nos legs Windows compartilhado/Debug (servidor run que
// motivou este conserto: so' este caso, dos quatro, reprovava, e nos
// tres legs shared - a assinatura exata de um site que cruza o boundary
// contra tres que nao cruzam).
GLINTFX_TEST(err_copy_with_attached_context_throws_a_catchable_bad_alloc_when_forced) {
    if (err_copy_oom_forcing_declared_not_applicable()) {
        declare_err_copy_oom_forcing_not_applicable();
        return;
    }

    glintfx::gltfx_err original(glintfx::gltfx_err_code::not_found);
    original.with_rejected_value("adapter"); // attaches context -> copy ctor below allocates

#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    // Patches glintfx.dll's OWN import table for the CRT allocation
    // primitive its (statically-linked) operator new calls internally -
    // see win_dll_alloc_hook.hpp's own header comment for the full
    // mechanism and what is FACT versus INFERENCE in it. On Windows/
    // STATIC (GLINTFX_STATIC_DEFINE defined) there is no separate .dll
    // module to patch - err.cpp is compiled directly into this
    // executable, so this TU's own operator new override below already
    // reaches it, same as Linux; this whole block compiles out there.
    glintfx_test::dll_alloc_hook dll_hook(L"glintfx.dll");
    const std::size_t dll_calls_before = glintfx_test::hooked_call_count();
    glintfx_test::arm_forced_failure();
#endif
    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0;
    bool threw_bad_alloc = false;
    try {
        // The copy itself is THE MECHANISM under test (the exact
        // expression the try{} in gpu_enumeration_facade.cpp now
        // guards) - a reference would defeat the point of this case.
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization) reason: see above
        const glintfx::gltfx_err copy(original);
        GLINTFX_CHECK_EQ(std::string_view(copy.rejected_value()), std::string_view("adapter"));
    } catch (const std::bad_alloc &) {
        threw_bad_alloc = true;
    }
    g_force_alloc_failure = false;
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    glintfx_test::disarm_forced_failure();
    // Proves the forcing mechanism actually reached the allocator this
    // copy needed, BEFORE trusting what the library did in response -
    // same discipline err_context_test.cpp's own allocator_reach_probe
    // already applies (GODS_LAWS.md L-44: not declaring a mechanism
    // worked without measuring it). A failure HERE, not below, means
    // this platform/configuration could not force the failure at all -
    // see win_dll_alloc_hook.hpp's own "achado 1" for why patched_count()
    // and the call-count delta are two DIFFERENT facts, checked
    // separately.
    const std::size_t dll_calls_after = glintfx_test::hooked_call_count();
    GLINTFX_CHECK(dll_hook.patched_count() > 0);
    GLINTFX_CHECK(dll_calls_after > dll_calls_before);
#endif

    GLINTFX_CHECK(threw_bad_alloc);
}

// CASO 4, o outro lado do degrau 2: o fallback que o `catch` escreve
// (`gltfx_err(gltfx_err_code::out_of_memory)`) usa o construtor
// trivial - err.hpp's own comment: "Inline, e NUNCA aloca". Provado
// aqui por contagem, sob o MESMO alocador armado, nao so' por
// confiança no comentario.
GLINTFX_TEST(err_trivial_code_only_fallback_never_allocates_even_when_forced) {
    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0;
    const std::size_t calls_before = g_override_new_call_count;
    const glintfx::gltfx_err fallback(glintfx::gltfx_err_code::out_of_memory);
    const std::size_t calls_after = g_override_new_call_count;
    g_force_alloc_failure = false;

    GLINTFX_CHECK_EQ(calls_after, calls_before);
    GLINTFX_CHECK(fallback.code() == glintfx::gltfx_err_code::out_of_memory);
}
