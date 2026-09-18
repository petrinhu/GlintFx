// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include <glintfx/platform/gl/gpu.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/gl/gpu_kind_seam.hpp"

// gpu_kind_exclusion_no_alloc_test.cpp - NOEXCEPT-ALLOC-B8 fatia F3
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F3",
// ESCOPO.md Decisao 10, GODS_LAWS.md L-04/L-09/L-20/L-22): proves the
// REAL SITE reached from BOTH platforms - resolve_kernel_and_
// exclusion() (gpu_kind_seam.cpp), called by classify_current_gpu()
// in egl_context_adapter.cpp (Linux) AND wgl_context_adapter.cpp
// (Windows), the SAME compiled definition on both sides (gpu_kind_
// seam.hpp's own header comment: "paridade estrutural garantida pelo
// compilador, nunca por dois arquivos lidos lado a lado") - never
// just the atom (gpu_kind_after_exclusion() on its own would only
// prove the plan's own sec. 10 table, form 1: "o teste do atomo e
// honesto e verde; o sitio continua alocando").
//
// BEFORE THIS FATIA'S OWN FIX: apply_gpu_kind_exclusion() (the atom
// resolve_kernel_and_exclusion() called) materialized a whole
// std::vector<gltfx_gpu_kind> copy of the enumeration inside a
// `noexcept` function - an unlucky std::bad_alloc there escapes a
// `noexcept` boundary and calls std::terminate(), killing the WHOLE
// consumer process (ESCOPO.md, Decisao 8, "a biblioteca nunca mata o
// processo do consumidor"). ESCOPO.md Decisao 10 REVOKES the risk
// this same file used to accept in writing (gpu_kind_exclusion.cpp's
// own header comment, apagado nesta mesma fatia, GODS_LAWS.md L-67).
//
// TWO ASSERTIONS PER CASE, NOT ONE, closing the plan's own "caso
// preguicoso" mutation (sec. F3, m3): every case below checks BOTH the
// allocation count/survival AND that the returned value is the
// GENUINELY PROMOTED one (`dedicated`) - a test that called
// resolve_kernel_and_exclusion() with `enumeration_index ==
// k_gltfx_gpu_index_unknown` (the early-return path, BEFORE via 1
// ever runs) would still show zero allocations, but its result would
// read back `unknown`, not `dedicated`, and the check below would
// catch it red-handed. Zero allocations alone never proves the code
// that "counts as covered" was actually reached.
//
// FORCED, NOT INSPECTED (same technique tests/err_context_test.cpp,
// tests/gfui_complex_match_resource_exhausted_test.cpp and tests/
// gl_proc_address_oom_test.cpp already use, cited in this fatia's own
// briefing so as not to invent a second mechanism): this translation
// unit replaces the global throwing AND nothrow operator new/delete
// with a pass-through to malloc/free that can be armed to fail on
// demand.
//
// SECOND COMPILE, SAME TECHNIQUE AS gpu_kind_seam_test.cpp (tests/
// CMakeLists.txt's own "the whole .cpp is one link unit" comment):
// gpu_kind_seam.cpp and gpu_kind_exclusion.cpp carry no GLINTFX_API
// (GODS_LAWS.md L-19 - neither lives under include/) and glintfx.so/
// .dll hides them by default anyway (CXX_VISIBILITY_PRESET hidden,
// cmake/GlintfxLibrary.cmake) - both are compiled a SECOND time
// directly into this test's own executable, so this file's own global
// operator new/delete override reaches every allocation production
// code performs, with no DSO boundary to cross (the exact reasoning
// tests/gl_proc_address_oom_test.cpp's own header comment already
// gives for the identical shape).
//
// PLATFORM-AGNOSTIC ON PURPOSE, LIKE THE SITE ITSELF: unlike gl_proc_
// address_oom_test.cpp (Linux-only, guarded by if(UNIX) in tests/
// CMakeLists.txt - the Wayland site it proves does not exist on
// Windows), this file builds and runs on BOTH platforms, exactly like
// gpu_kind_exclusion_test.cpp/gpu_kind_seam_test.cpp already do - the
// MSVC-ASan valve below is therefore the WIN32-aware form tests/
// gfui_complex_match_resource_exhausted_test.cpp's own oom_forcing_
// declared_not_applicable() uses, not the GCC/Clang-only
// __SANITIZE_ADDRESS__ form gl_proc_address_oom_test.cpp's own Linux-
// only copy uses.

namespace {

using glintfx::gltfx_gpu_kind;
using glintfx::platform::resolve_kernel_and_exclusion;

bool g_force_alloc_failure = false;
std::size_t g_calls_to_allow_before_failure = 0;
std::size_t g_override_new_call_count = 0;

[[nodiscard]] bool should_fail_this_allocation() noexcept {
    if (!g_force_alloc_failure) {
        return false;
    }
    if (g_calls_to_allow_before_failure > 0) {
        --g_calls_to_allow_before_failure;
        return false;
    }
    return true;
}

// Same MSVC-ASan-only declare-and-skip valve tests/gfui_complex_match_
// resource_exhausted_test.cpp's own oom_forcing_declared_not_
// applicable() uses (learn.microsoft.com/cpp/sanitizers/asan-known-
// issues, "Overriding operator new and delete": ASan's own operator
// new/delete wins by default over any user override linked into the
// same MSVC binary) - this file has no DLL-crossing gap of its own
// (the "SECOND COMPILE" paragraph above), the same reason that file's
// own header comment gives for still needing this valve despite that.
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(_WIN32)
    return false;
#else
    return std::getenv("GLINTFX_GPU_KIND_EXCLUSION_TEST_FORCE_OOM_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "gpu_kind_exclusion_no_alloc_test: {} declared NOT APPLICABLE under MSVC "
                 "AddressSanitizer (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "
                 "\"Overriding operator new and delete\": ASan's own operator new/delete wins by "
                 "default over any user override linked into the same binary - this file's "
                 "forced-failure override never gets a chance to run, so the assertion this case "
                 "exists to prove would measure nothing)",
                 case_name);
}

// A enumeracao mais simples que entra MESMO na via da exclusao (F3's
// own "teste que falha ANTES"): o dispositivo atual (indice 0) e
// `unknown`, e existe um `shared` em outra posicao - a unica forma de
// resolve_kernel_and_exclusion() chegar ao codigo que este teste
// existe para cobrir (kernel_kind == unknown, enumeration_index !=
// k_gltfx_gpu_index_unknown).
constexpr std::array<gltfx_gpu_kind, 2> k_kinds_with_shared{gltfx_gpu_kind::unknown,
                                                            gltfx_gpu_kind::shared};

} // namespace

void *operator new(std::size_t size) {
    ++g_override_new_call_count;
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

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

// THE POSITIVE FORM OF DEGRAU 1 (plan sec. 2, "nao alocar"): with a
// HEALTHY allocator (never armed), the via-1 site allocates exactly
// zero times AND returns the genuinely promoted value - the same
// "no_alloc" shape gl_proc_address_oom_test.cpp's own proc_address_
// allocates_nothing_resolving_a_long_name uses.
//
// MUTATION m1 OF THE PLAN'S OWN F3 SECTION LIVES HERE: reverting
// gpu_kind_after_exclusion() to rebuild a std::vector copy inside
// resolve_kernel_and_exclusion() makes g_override_new_call_count's
// delta go from zero to at least one, failing the count check.
GLINTFX_TEST(resolve_kernel_and_exclusion_via_exclusion_allocates_nothing) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "resolve_kernel_and_exclusion_via_exclusion_allocates_nothing");
        return;
    }

    const std::size_t calls_before = g_override_new_call_count;
    const gltfx_gpu_kind result =
        resolve_kernel_and_exclusion(gltfx_gpu_kind::unknown, 0, k_kinds_with_shared);
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);

    // A checagem de VALOR, nao so de contagem: fecha o "caso
    // preguicoso" da mutacao m3 (plan sec. F3) - um teste que tivesse
    // chamado com k_gltfx_gpu_index_unknown (saindo ANTES da via 1)
    // tambem mostraria zero alocacoes, mas devolveria `unknown`, nao
    // `dedicated`.
    GLINTFX_CHECK(result == gltfx_gpu_kind::dedicated);
}

// THE CASE (plan's own F3, case 2 of "teste que falha ANTES"): against
// the pre-fix site (apply_gpu_kind_exclusion() rebuilding a
// std::vector copy), an allocation failure forced on the VERY NEXT
// call escapes as std::bad_alloc out of a `noexcept` function, which
// [except.terminate] mandates calls std::terminate() - this whole test
// BINARY dies before GLINTFX_CHECK ever runs. After the fix, the call
// returns normally, WITHOUT the override ever firing (the "THE
// POSITIVE FORM" case above already measures that), and with the SAME
// value the healthy case returns.
//
// MUTATION m2 OF THE PLAN'S OWN F3 SECTION LIVES HERE: inverting the
// promotion (`shared` becoming `dedicated` instead of `unknown`
// becoming `dedicated`) does not change the ALLOCATION shape, so it is
// caught by gpu_kind_exclusion_test.cpp's own 8-cell enumeration
// instead, never by this file.
GLINTFX_TEST(resolve_kernel_and_exclusion_does_not_terminate_under_an_armed_allocator) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "resolve_kernel_and_exclusion_does_not_terminate_under_an_armed_allocator");
        return;
    }

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // the very next allocation, if any, fails
    const gltfx_gpu_kind result =
        resolve_kernel_and_exclusion(gltfx_gpu_kind::unknown, 0, k_kinds_with_shared);
    g_force_alloc_failure = false;

    // Reaching this line at all already proves the process did not
    // std::terminate() - before this fatia's own fix, it never did.
    // Same value as the healthy case: "conserta sem alocar" means the
    // armed allocator never even runs on this path anymore, and the
    // library's own behaviour is unchanged either way.
    GLINTFX_CHECK(result == gltfx_gpu_kind::dedicated);

    // A second, healthy call (override no longer armed) proves the
    // pure function carries no corrupted state forward from the first
    // - the same "recovers" control tests/gl_proc_address_oom_test.
    // cpp's own second call already gives.
    const gltfx_gpu_kind second =
        resolve_kernel_and_exclusion(gltfx_gpu_kind::unknown, 0, k_kinds_with_shared);
    GLINTFX_CHECK(second == gltfx_gpu_kind::dedicated);
}
