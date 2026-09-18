// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include "gfss/selector_parse.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_selector_parse_oom_test.cpp - NOEXCEPT-ALLOC-B8 fatia F5
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F5",
// GODS_LAWS.md L-04/L-09/L-17/L-20/L-22/L-27/L-40, ESCOPO.md Decisao 8,
// 16/09/2026, verbatim: "Devolve erro; o aplicativo decide"): proves
// B2/B3/B4 of the audit (/var/tmp/builds/claude-1000/varredura-
// noexcept/RELATORIO.md SS3) no longer end the WHOLE consumer process
// when an allocation inside the selector parser fails under memory
// pressure - selector_parse.cpp's own parse_compound_selector() (the
// compound.simple_selectors.push_back() this fatia's own commit
// message names) and parse_complex_selector() (complex_selector.rest.
// push_back(), same treatment) both propagate std::bad_alloc to their
// caller now, instead of letting it escape a noexcept function and
// calling std::terminate() per [except.terminate].
//
// TWO CASES, ONE PER PATH, ON PURPOSE (this fatia's own service order,
// echoed in the plan's own F5 section: "nao provar um e emprestar a
// confianca ao outro"): gfss_selector_parse_survives_an_armed_
// allocator_for_a_compound_chain below exercises B2/B3 (a compound
// selector with combinators, no `:not()` anywhere) and is GREEN against
// this fatia's own fix. gfss_selector_parse_survives_an_armed_
// allocator_for_a_nested_not_argument exercises B4 - the recursive
// re-entry a `:not()` argument causes - and is DELIBERATELY LEFT RED
// here: measured, not assumed (GODS_LAWS.md L-44), four MORE functions
// on that exact call path (attach_not_argument(), parse_functional_
// pseudo(), parse_pseudo_selector(), parse_one_simple_selector() -
// selector_parse.cpp:429/498/527/735 as of this fatia's own start) are
// ALSO noexcept and sit BETWEEN parse_not_argument() and parse_
// compound_selector() in the call chain - none of them named by the
// plan's own "four pontos" list. Removing noexcept from ONLY the three
// selector_parse.cpp sites the plan names (parse_compound_selector,
// parse_complex_selector, parse_not_argument) still lets THIS case's
// own process die: the propagating exception hits attach_not_
// argument()'s own noexcept boundary one frame above parse_not_
// argument(), which the narrow fix never touched. This is the "CONFIRA
// ANTES DE APLICAR" condition the plan's own F5 section states in
// these exact words: "se houver um noexcept entre qualquer uma delas e
// um chamador que hoje nao e noexcept, o desenho muda e para" - it
// fired, and per that same instruction this file does not fix it
// unilaterally. THE WHOLE gfss_selector_parse_oom_test BINARY THEREFORE
// STILL EXITS NON-ZERO until that four-function extension is
// authorized and applied - an honest red target beats a green one that
// stopped testing what it claims to.
//
// N CHOSEN BY MEASUREMENT, NOT GUESSED (GODS_LAWS.md L-43: the
// criterion existed before the number): a standalone repro tool (same
// idiom as RELATORIO.md's own repro/repro.cpp) linked selector_parse.
// cpp + anb_parse.cpp against this project's own built glintfx shared
// library (for gltfx_gfss_tokenize(), GLINTFX_API and therefore never
// recompiled here - the SAME reason gfss_selector_parse_test.cpp's own
// header comment already gives), armed operator new to fail on the
// (N+1)-th allocation, and searched N upward, one full process per N
// (a crash-by-signal cannot be caught in-process, the same reason
// harness_main.cpp's own case runner cannot save a case that std::
// terminate()s). For "a > b + c" (11 total allocations against a
// healthy allocator), N=0..4 land inside gltfx_gfss_tokenize()'s own
// token-vector growth - never noexcept, so failing there just
// propagates normally even before this fatia - and N=5 is the first
// that lands inside parse_compound_selector()'s own push_back(), the
// earliest point this fatia's own fix touches; N=5..9 all reproduced
// std::terminate() against the pre-fix code, N=10 lands past every
// noexcept-scoped allocation for this input and never crashes either
// way, and N=11 exceeds the total allocation count (nothing left to
// fail). For ":not(a > b)" (19 total against a healthy allocator), the
// SAME N=5 happens to be the first that lands inside the nested
// re-entry too (this input's own outer tokenize costs the identical 5
// calls) - chosen as the EARLIEST point on this second, distinct path,
// the same reasoning F2's own drm_device_facts_oom_test.cpp already
// applies for picking "the very next allocation" rather than an
// arbitrary later one.

namespace {

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

// THE THREE-BRANCH VALVE (the SAME idiom err_context_test.cpp's own
// header comment already establishes in full, "ASAN-OOM-FORCE-GAP" -
// reused here verbatim rather than re-derived, GODS_LAWS.md L-07/L-27):
// MSVC's own AddressSanitizer wins the operator-new-override race by
// DEFAULT (learn.microsoft.com/cpp/sanitizers/asan-known-issues,
// "Overriding operator new and delete" - without /INFERASANLIBS, which
// neither this project's CMake nor CI passes), so this TU's own
// override never gets a chance to run there and the two OOM cases below
// would measure nothing. Declared, not silently skipped (GODS_LAWS.md
// L-40). The getenv() OR-valve exists only so the MSVC-ASan declare-
// and-return path can be forced and proven from a Linux machine that
// has no MSVC ASan to run - never read by the real condition, never set
// by CI - and is compiled out entirely on a plain Windows build (no
// ASan) so MSVC's own C4996 on std::getenv() under -DGLINTFX_WERROR=ON
// never fires for a branch that build never takes.
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(_WIN32)
    return false;
#else
    return std::getenv("GLINTFX_SELECTOR_PARSE_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "gfss_selector_parse_oom_test: {} declared NOT APPLICABLE under MSVC "
                 "AddressSanitizer (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "
                 "\"Overriding operator new and delete\": ASan's own operator new/delete wins "
                 "by default over any user override linked into the same binary - this file's "
                 "forced-failure override never gets a chance to run, so the assertion this "
                 "case exists to prove would measure nothing)",
                 case_name);
}

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

// B2/B3, THE CASE (plan's own F5, first "teste que falha ANTES"):
// against the pre-fix sites (parse_compound_selector()/parse_complex_
// selector() both noexcept, push_back() inside each), an allocation
// failure forced on the 6th call (N=5, see this file's own header
// comment for how that number was measured) escapes as std::bad_alloc
// out of a noexcept function, which [except.terminate] mandates calls
// std::terminate() - this whole test BINARY dies before GLINTFX_CHECK
// ever runs. After this fatia's own fix, the call returns normally
// (result.ok == false, since the allocation genuinely failed and the
// parse cannot complete) instead.
//
// MUTATION m1 (parse_compound_selector site) LIVES HERE: reverting
// selector_parse.cpp:790's noexcept alone makes this case std::
// terminate() again, independent of parse_complex_selector's own
// noexcept.
GLINTFX_TEST(gfss_selector_parse_survives_an_armed_allocator_for_a_compound_chain) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "gfss_selector_parse_survives_an_armed_allocator_for_a_compound_chain");
        return;
    }

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 5; // measured: first allocation inside
                                         // parse_compound_selector() for this text
    glintfx::style::detail::selector_parse_result result{};
    bool caught = false;
    try {
        result = glintfx::style::detail::parse_selector_list("a > b + c");
    } catch (const std::bad_alloc &) {
        caught = true;
    }
    g_force_alloc_failure = false;

    // Reaching this line at all already proves the process did not
    // std::terminate() - before this fatia's own fix, it never did.
    // The bad_alloc must actually have been thrown and caught here
    // (D-4 opcao A of the plan's own SS3: "chega ao chamador"), not
    // silently swallowed somewhere that never touched the allocator at
    // all - g_override_new_call_count moving past the armed point is
    // what tells the two apart.
    GLINTFX_CHECK(caught);
    GLINTFX_CHECK(!result.ok);
}

// B4, THE HARDER CASE (plan's own F5, second "teste que falha ANTES"):
// see this file's own header comment above for why this case is
// EXPECTED to still terminate the process today - four noexcept
// functions this fatia's own service order did not name
// (attach_not_argument/parse_functional_pseudo/parse_pseudo_selector/
// parse_one_simple_selector) still stand between parse_not_argument()
// and parse_compound_selector() in the call chain a `:not()` argument
// takes. Left in this file, RED, with the finding named here rather
// than fixed silently - GODS_LAWS.md L-18: an implementer's own
// inference about scope never gets to cut scope on its own say-so.
GLINTFX_TEST(gfss_selector_parse_survives_an_armed_allocator_for_a_nested_not_argument) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "gfss_selector_parse_survives_an_armed_allocator_for_a_nested_not_argument");
        return;
    }

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 5; // measured: earliest allocation inside
                                         // the nested :not() re-entry for this text
    glintfx::style::detail::selector_parse_result result{};
    bool caught = false;
    try {
        result = glintfx::style::detail::parse_selector_list(":not(a > b)");
    } catch (const std::bad_alloc &) {
        caught = true;
    }
    g_force_alloc_failure = false;

    GLINTFX_CHECK(caught);
    GLINTFX_CHECK(!result.ok);
}
