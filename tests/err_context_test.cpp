// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "harness/win_dll_alloc_hook.hpp"

// err_context_test.cpp - CE-3 of CORE-ERROR (TODO.md, GODS_LAWS.md
// L-20): proves the six diagnostic accessors round-trip through their
// matching with_*() attach method, that an absent field reads back
// empty/zero rather than undefined behavior, that a COPY of a gltfx_err
// owns an INDEPENDENT context (mutating the original never leaks into
// the copy), and - the control that catches the real defect, not the
// happy path - that attaching a diagnostic degrades to code-only,
// never crashes and never propagates a second failure, when the
// allocation behind it is forced to fail.
//
// ALLOCATOR TOGGLE: this translation unit replaces the global
// operator new/delete (throwing AND nothrow forms - ensure_context()
// in err.cpp uses `new (std::nothrow)`, std::string's own internal
// growth uses the throwing form) with a pass-through to malloc/free
// that can be armed to fail on demand via g_force_alloc_failure. Only
// the two OOM-degradation cases below arm it; every other case in this
// file runs with a normal, working allocator - safe because
// glintfx_add_test() (cmake/GlintfxTest.cmake) gives every test case
// its OWN executable, so there is no ODR collision with any other TU.
//
// DECLARED COVERAGE, honestly, not discovered by a future reader the
// hard way (CORE-ERROR CI finding, 25/08/2026 - the same class of risk
// tests/err_no_alloc_test.cpp already declares for its OWN use of this
// technique, applied here to the FORCING half instead of the counting
// half): on Windows in SHARED mode, err.cpp's ensure_context() is
// compiled INSIDE glintfx.dll and its `new (std::nothrow)` call
// resolves, at the DLL's OWN link time, against whatever CRT import
// table that DLL was linked with - NOT against this executable's
// replacement operators, because PE/COFF has no ELF-style default
// global symbol interposition (a DLL does not defer symbol resolution
// to whatever process eventually loads it). g_force_alloc_failure may
// therefore have ZERO EFFECT on ensure_context()'s allocation on that
// leg specifically - the two OOM-degradation cases below could pass
// their "override was reached" check as false there while the library
// still degrades correctly, or (the real risk) never actually force
// the failure at all, making the "path stays empty" assertion measure
// nothing. g_override_new_call_count below exists to tell the two
// apart: it counts every call to THIS TU's own operator new/new(nothrow)
// overrides, so a degradation case can assert the override was
// actually invoked BEFORE trusting what the library did in response.
//
// THE WINDOWS/SHARED GAP ITSELF, closed by harness/win_dll_alloc_hook.
// hpp (CORE-ERROR, GODS_LAWS.md L-07/L-27/L-40 - read that header's own
// comment before touching allocator_reach_probe below): this TU's own
// operator-new override cannot reach glintfx.dll's internal calls, so
// on Windows/SHARED a SECOND, DLL-side mechanism patches glintfx.dll's
// OWN import table for the CRT allocation primitive its (statically
// linked, per Microsoft's own current documentation) operator new
// implementation calls - a small, closed candidate list, not one
// guessed name, because this project cannot run the toolchain that
// would confirm the exact symbol. allocator_reach_probe below sums
// BOTH counters (this TU's own, and the DLL hook's) into one
// reach-proof: on every platform except Windows/SHARED the DLL-side
// counter is always zero-delta (the hook itself found no separate
// glintfx.dll module, or is compiled out entirely - see that header's
// own guard), so the sum degenerates to exactly the ORIGINAL bare
// counter comparison this file had before this mechanism existed.

namespace {

bool g_force_alloc_failure = false;

// Counts every call to THIS TU's own operator new/new(nothrow)
// overrides below, regardless of whether g_force_alloc_failure is
// armed - see the "DECLARED COVERAGE" paragraph above. A degradation
// case reads the delta across its own armed window to prove the
// override was actually the allocator ensure_context()/with_path()
// reached, before trusting what happened as a result.
std::size_t g_override_new_call_count = 0;

// Long enough (well past libstdc++/libc++'s small-string-optimization
// capacity, typically 15-22 bytes) to force std::string::assign() to
// actually call the THROWING global operator new below, not just
// reuse its inline buffer - otherwise the "string growth fails mid-
// attach" case would never exercise the code path it exists to prove.
constexpr std::string_view k_long_value =
    "a value long enough to defeat small-string-optimization and force a real heap allocation";

// ASAN-OOM-FORCE-GAP, ORDEM DE SERVICO (GODS_LAWS.md L-04/L-40),
// 03/09/2026: the two OOM-degradation cases below rely on forcing an
// allocation to fail (see "THE WINDOWS/SHARED GAP ITSELF" above for
// the mechanism). CI found live (Windows - Sanitizer (ASan), run
// 33785315004, job 100748542369) that BOTH cases fail under MSVC
// AddressSanitizer: `err.path().empty()` is false, meaning the forced
// failure never actually blocked the allocation.
//
// NOT "any sanitizer breaks this technique" - REFUTED, not assumed:
// under a real -DGLINTFX_SANITIZE=address build on Linux (GCC), this
// exact file passes 5/5 with 0 failures (proven the day this comment
// was written; this project's own CI job `sanitizer` already runs
// this file under ASan/UBSan on Fedora, unguarded, and it has never
// been the source of a red run). On Linux, ASan's operator new/delete
// interceptors are WEAK symbols - this TU's own STRONG operator new
// override above still wins by ordinary linker precedence, same as
// without ASan at all.
//
// MSVC-SPECIFIC ROOT CAUSE, FACT from Microsoft's own current
// documentation (learn.microsoft.com/cpp/sanitizers/asan-known-issues,
// "Overriding operator new and delete", fetched 03/09/2026): "ASan
// uses a custom version of operator new and operator delete... Run the
// linker with /INFERASANLIBS to ensure that ASan's new/delete override
// has LOWER precedence, so that the linker chooses operator new or
// operator delete overrides in OTHER libraries over ASan's custom
// versions" - i.e. WITHOUT that flag (neither this project's CMake nor
// the CI job passes it), ASan's OWN operator new/delete wins by
// DEFAULT, the opposite default from GCC/Clang's weak-symbol behavior
// proven above. This starves BOTH forcing mechanisms at once: this
// TU's own override (g_force_alloc_failure is armed, but ASan's
// operator new runs instead and never consults it) and, separately,
// harness/win_dll_alloc_hook.hpp's malloc-family IAT patch inside
// glintfx.dll (learn.microsoft.com/cpp/sanitizers/asan-runtime,
// "Function interception": ASan intercepts CRT allocation functions by
// hotpatching them directly, not by resolving through the importing
// module's own IAT this hook patches).
//
// SCOPE OF THE DECLARATION, and why it is NOT narrowed to
// GLINTFX_STATIC_DEFINE like win_dll_alloc_hook.hpp's own guard is:
// the operator-new-precedence fact above is a property of linking the
// ASan runtime into an MSVC binary at all, static or shared alike - it
// is not specific to the DLL-crossing problem win_dll_alloc_hook.hpp
// exists for. CI's own windows-sanitizer job configures
// -DGLINTFX_SANITIZE=address with BUILD_SHARED_LIBS at its default
// (ON) only - its own comment in .github/workflows/ci.yml says
// plainly "nenhum dos dois testa o modo estatico sob sanitizer" - so
// Windows/STATIC under ASan has never been exercised here, and nothing
// observed narrows this declaration to the shared leg specifically
// (GODS_LAWS.md L-27: a scope this project cannot test is not silently
// assumed safe).

// GODS_LAWS.md L-40: a guard that can only ever evaluate true on a
// platform this project has no machine for (MSVC ASan - this codebase
// builds and runs only on Linux) is not a PROVEN mechanism until its
// own consequence - the "declare and return before the real checks
// run" path itself, not just the preprocessor condition - has actually
// executed somewhere. GLINTFX_ERR_CONTEXT_TEST_FORCE_OOM_NOT_APPLICABLE
// is an opt-in, developer-only escape hatch that ORs a runtime check
// into the real one, so the exact declare-and-return code path can be
// forced and verified on Linux too - same shape as check_spdx.py's own
// `GLINTFX_SPDX_SELFTEST_FORCE_WINDOWS_HOSTILE_SKIP` precedent for the
// identical problem (a platform-conditional branch this project cannot
// natively reach). Never set by CI, and never read by the real
// `#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)` condition
// itself - only by this OR - so it cannot mask the real MSVC leg
// failing to detect its own sanitizer.
//
// The std::getenv() call itself is compiled out on Windows without ASan:
// this override valve exists only to reach the MSVC-ASan declare-and-return
// path from a Linux machine that has no MSVC ASan to run, so a plain
// Windows build has no use for it. MSVC's own <cstdlib> flags std::getenv()
// as C4996 ("may be unsafe") under /W4, which -DGLINTFX_WERROR=ON escalates
// to a build failure, so this branch would break plain Windows builds for
// a code path they never take. Splitting the Windows case in two keeps
// that call scoped to where it can ever matter.
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(_WIN32)
    return false;
#else
    return std::getenv("GLINTFX_ERR_CONTEXT_TEST_FORCE_OOM_NOT_APPLICABLE") != nullptr;
#endif
}

// Prints the one declaration line GODS_LAWS.md L-40 requires (ausencia
// declarada, nunca pulo silencioso) - used by BOTH OOM-degradation
// cases below, the same "second real use crosses the extraction bar"
// precedent allocator_reach_probe below already sets in this file.
// Returning immediately after this call, before allocator_reach_probe
// is even constructed, also means win_dll_alloc_hook.hpp's own
// IAT-patching constructor never runs under MSVC ASan - avoiding a
// second, unrelated risk this file cannot test either (Microsoft's own
// "Function interception" doc: a hotpatch that cannot write a jmp into
// too-short a prologue makes ASan itself __debugbreak() the process).
void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "err_context_test: {} declared NOT APPLICABLE under MSVC AddressSanitizer "
                 "(learn.microsoft.com/cpp/sanitizers/asan-known-issues, \"Overriding operator "
                 "new and delete\": ASan's own operator new/delete wins by default over any "
                 "user override linked into the same binary - this file's forced-failure "
                 "override never gets a chance to run, so the assertion this case exists to "
                 "prove would measure nothing)",
                 case_name);
}

} // namespace

void *operator new(std::size_t size) {
    ++g_override_new_call_count;
    if (g_force_alloc_failure) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    ++g_override_new_call_count;
    if (g_force_alloc_failure) {
        return nullptr;
    }
    return std::malloc(size);
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

namespace {

// Arms both forcing mechanisms for the duration of one with_*() call,
// and proves at least one of them was reached - see the "THE
// WINDOWS/SHARED GAP ITSELF" header comment above and
// harness/win_dll_alloc_hook.hpp's own comment for the full reasoning.
// Used by BOTH OOM-degradation cases below (CE-3): the second real use
// is what crosses CONTRACT.md §6's three-occurrence bar for pulling
// this out of each test body.
class allocator_reach_probe {
  public:
    allocator_reach_probe() noexcept
        : m_tu_calls_before(g_override_new_call_count)
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
          ,
          m_dll_hook(L"glintfx.dll"), m_dll_calls_before(glintfx_test::hooked_call_count())
#endif
    {
        g_force_alloc_failure = true;
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
        glintfx_test::arm_forced_failure();
#endif
    }

    ~allocator_reach_probe() {
        g_force_alloc_failure = false;
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
        glintfx_test::disarm_forced_failure();
#endif
    }

    allocator_reach_probe(const allocator_reach_probe &) = delete;
    allocator_reach_probe &operator=(const allocator_reach_probe &) = delete;

    // True once at least one of the two mechanisms observed a call
    // since construction. On every platform except Windows/SHARED the
    // DLL-side term is always zero (see the class comment above), so
    // this degenerates to the ORIGINAL bare
    // "calls_after > calls_before" comparison this file had before the
    // DLL hook existed.
    [[nodiscard]] bool reached() const noexcept {
        const std::size_t tu_delta = g_override_new_call_count - m_tu_calls_before;
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
        const std::size_t dll_delta = glintfx_test::hooked_call_count() - m_dll_calls_before;
        return (tu_delta + dll_delta) > 0;
#else
        return tu_delta > 0;
#endif
    }

#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    // GODS_LAWS.md L-40: zero patched is a FACT this test surfaces
    // (this toolset's operator new does not route through anything on
    // win_dll_alloc_hook.hpp's candidate list), never a silently
    // accepted no-op.
    [[nodiscard]] std::size_t dll_patched_count() const noexcept {
        return m_dll_hook.patched_count();
    }
#endif

  private:
    std::size_t m_tu_calls_before;
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    glintfx_test::dll_alloc_hook m_dll_hook;
    std::size_t m_dll_calls_before;
#endif
};

} // namespace

GLINTFX_TEST(each_field_round_trips_through_its_attach_method) {
    glintfx::gltfx_err err(glintfx::gltfx_err_code::parse_failure);
    err.with_path("assets/scene.rcss")
        .with_position(12, 5)
        .with_byte_offset(4096)
        .with_rejected_value("#ffgg00")
        .with_os_error_code(-2);

    GLINTFX_CHECK(err.code() == glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(err.path() == std::string_view{"assets/scene.rcss"});
    GLINTFX_CHECK(err.line() == 12);
    GLINTFX_CHECK(err.column() == 5);
    GLINTFX_CHECK(err.byte_offset() == 4096);
    GLINTFX_CHECK(err.rejected_value() == std::string_view{"#ffgg00"});
    GLINTFX_CHECK(err.os_error_code() == -2);
}

GLINTFX_TEST(absent_fields_read_back_empty_or_zero) {
    const glintfx::gltfx_err err(glintfx::gltfx_err_code::not_found);

    GLINTFX_CHECK(err.path().empty());
    GLINTFX_CHECK(err.line() == 0);
    GLINTFX_CHECK(err.column() == 0);
    GLINTFX_CHECK(err.byte_offset() == 0);
    GLINTFX_CHECK(err.rejected_value().empty());
    GLINTFX_CHECK(err.os_error_code() == 0);
}

GLINTFX_TEST(copy_owns_an_independent_context) {
    glintfx::gltfx_err original(glintfx::gltfx_err_code::io_failure);
    original.with_path("original/path.txt").with_position(1, 1);

    const glintfx::gltfx_err copy(original);

    // Mutate the ORIGINAL after the copy was taken. If the copy shared
    // the original's context pointer instead of owning a deep copy,
    // this would leak into `copy` too - the exact bug the copy
    // constructor's deep-copy branch (err.cpp) exists to prevent,
    // and the exact bug a shallow-copy regression would reintroduce.
    original.with_path("mutated/path.txt").with_position(99, 99);

    GLINTFX_CHECK(copy.path() == std::string_view{"original/path.txt"});
    GLINTFX_CHECK(copy.line() == 1);
    GLINTFX_CHECK(copy.column() == 1);

    GLINTFX_CHECK(original.path() == std::string_view{"mutated/path.txt"});
    GLINTFX_CHECK(original.line() == 99);
}

GLINTFX_TEST(attach_degrades_to_code_only_when_context_allocation_fails) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "attach_degrades_to_code_only_when_context_allocation_fails");
        return;
    }

    const allocator_reach_probe probe;
    glintfx::gltfx_err err(glintfx::gltfx_err_code::parse_failure);
    // ensure_context()'s `new (std::nothrow) err_context()` is the
    // VERY FIRST allocation with_path() would need (err has no
    // context yet) - forcing it to fail exercises the "context itself
    // cannot be allocated at all" branch.
    err.with_path("does/not/matter");

    // Proves the FORCING MECHANISM actually reached the allocator this
    // call needed - see the "DECLARED COVERAGE" / "THE WINDOWS/SHARED
    // GAP ITSELF" header comments. A failure HERE, not below, means
    // this platform/configuration could not force the failure at all,
    // not that the library mishandled a real one.
    GLINTFX_CHECK(probe.reached());
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    // GODS_LAWS.md L-40: distinguishes "the DLL hook found nothing to
    // patch" (this check) from "it patched something but the forced
    // value made no difference" (the check above) - two different
    // facts, two different assertions.
    GLINTFX_CHECK(probe.dll_patched_count() > 0);
#endif

    // Degrades to code-only: the ORIGINAL code survives unchanged (it
    // does NOT become out_of_memory - that translation is a SEPARATE
    // mechanism for call sites that already return a gltfx_rslt<T>,
    // see err.hpp's own "ATTACH IS BEST-EFFORT" comment), and no
    // exception escaped this noexcept call (proven simply by reaching
    // this line at all - an escaped exception from a noexcept function
    // calls std::terminate(), which would have ended the process
    // before this check ever ran).
    GLINTFX_CHECK(err.code() == glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(err.path().empty());
}

GLINTFX_TEST(attach_degrades_to_code_only_when_a_field_allocation_fails_mid_attach) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "attach_degrades_to_code_only_when_a_field_allocation_fails_mid_attach");
        return;
    }

    glintfx::gltfx_err err(glintfx::gltfx_err_code::unsupported);
    // Succeeds with the allocator healthy: ensure_context() runs here,
    // so the SECOND case below exercises a DIFFERENT failure point (a
    // field growing inside an ALREADY-allocated context), not context
    // allocation itself.
    err.with_position(3, 4);

    const allocator_reach_probe probe;
    // k_long_value is long enough to force std::string::assign() to
    // call the THROWING global operator new above; ensure_context()
    // itself is a no-op here (m_context is already non-null), so this
    // isolates the "string buffer growth throws mid-attach" branch.
    err.with_path(k_long_value);

    // Same proof as the case above, isolating the SAME risk for the
    // "string buffer growth" allocation point specifically.
    GLINTFX_CHECK(probe.reached());
#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
    GLINTFX_CHECK(probe.dll_patched_count() > 0);
#endif

    GLINTFX_CHECK(err.code() == glintfx::gltfx_err_code::unsupported);
    // The field this specific attach call was trying to set: the
    // context already existed (from with_position above), so
    // ensure_context() is not what failed - path() reads back empty
    // because the assign() that WOULD have set it threw and was
    // caught, not because there was no context to check.
    GLINTFX_CHECK(err.path().empty());
    // Unaffected by the failed attach: a field set BEFORE the forced
    // failure, in the SAME context, survives untouched.
    GLINTFX_CHECK(err.line() == 3);
    GLINTFX_CHECK(err.column() == 4);
}
