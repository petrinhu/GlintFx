// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/egl_context_adapter.hpp"

// gl_proc_address_oom_test.cpp - NOEXCEPT-ALLOC-B8 fatia F1
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F1",
// GODS_LAWS.md L-04/L-09/L-20/L-22): proves the REAL SITE, not just
// the atom (proc_name_buffer_test.cpp on its own would only prove form
// 1 of the plan's own sec. 10 table, "o teste do atomo e honesto e
// verde; o sitio continua com std::string") - wayland_egl_context_
// adapter::proc_address() (egl_context_adapter.cpp:851, before this
// fatia's own fix) allocates inside a `noexcept` function: an unlucky
// std::bad_alloc there calls std::terminate() and kills the WHOLE
// consumer process (ESCOPO.md, Decisao 8, "a biblioteca nunca mata o
// processo do consumidor"). The gemeo on Windows was already fixed in
// babbd77 (src/platform/win32/wgl_proc_address.cpp) - this file is
// this fatia's own proof that the Wayland side now matches it.
//
// NO COMPOSITOR NEEDED, MEASURED NOT ASSUMED: eglGetProcAddress() is
// display- and context-INDEPENDENT by its own Khronos specification
// ("eglGetProcAddress may be called to obtain the address of an EGL,
// client API or extension function... independent of the display" -
// registry.khronos.org/EGL/sdk/docs/man/html/eglGetProcAddress.xhtml)
// - it is a pure name-to-pointer lookup inside libEGL's own loaded
// dispatch table, never an IPC round-trip to a running compositor.
// wayland_egl_context_adapter::proc_address() itself touches no member
// state either (egl_context_adapter.hpp's own class comment: `const
// noexcept`, and the method body reads nothing but its own `name`
// parameter) - a DEFAULT-CONSTRUCTED, NEVER-open()'d adapter (the
// adapter's own ctor is `noexcept = default`, no EGL call at all) is
// therefore enough to exercise the real production call, without
// GODS_LAWS.md L-09's nested-compositor container machinery: this
// test never opens a window, a display connection or a GL context: it
// only ever calls proc_address() and lets the (idempotent-safe,
// egl_context_adapter.hpp's own contract) destructor run close() on
// an adapter that was never opened.
//
// THE NAME MATTERS, MEASURED NOT GUESSED (twice): a short name like
// "glClear" (7 chars) fits libstdc++'s std::string small-string
// optimization (15 chars inline on a 64-bit build) and never touches
// the heap at all - this test's FIRST attempt used "glClear" and its
// own "override was reached" assertion failed cleanly even against the
// PRE-fix site (no allocation ever happened, so forcing one to fail
// proved nothing). Every real name below is at least 20 characters - a
// genuine, published ARB/EXT-style GL function name this project's own
// win32 side already cites at that length (wgl_context_adapter.cpp's
// own "glGetUnsignedBytevEXT"/"wglGetExtensionsStringARB") - long
// enough to force the PRE-fix `std::string owned(name)` off SSO and
// onto the heap, which is exactly what proved the red genuine (this
// fatia's own commit history/report has the captured std::terminate()
// crash, contra the SHA before this fix).
//
// THE SECOND MEASUREMENT, ONLY VISIBLE AFTER THE FIX WAS WRITTEN: with
// the atom in place (copy_nul_terminated() into a std::array, no heap
// touched by this site AT ALL), the global operator new/delete
// override below is measured to be reached ZERO times by a call to
// proc_address() - not "sometimes", not "depends on the name": zero,
// for every name this file exercises. This is DEGRAU 1 of the plan's
// own escada (sec. 2, "nao alocar") doing exactly what it promises, so
// the "count > before" reached-proof the plan's own F1 section
// describes for this case is INAPPLICABLE post-fix by construction -
// there is nothing left to be forced to fail on this path anymore.
// proc_address_allocates_nothing_resolving_a_long_name below is the
// POSITIVE form of that same fact (the "no_alloc" family shape
// gpu_kind_exclusion_no_alloc_test/gfss_anb_parse_no_alloc_test also
// use, plan sec. F3/F4), and proc_address_survives_an_armed_allocator
// below still exists as the crash witness/mutation catcher: it proves
// the call is safe EVEN WITH an armed override present, without
// depending on that override ever firing.
//
// SECOND COMPILE, SAME TECHNIQUE AS EVERY OTHER wayland/*_test.cpp IN
// THIS SUITE (tests/CMakeLists.txt's own "the whole .cpp is one link
// unit" comment, e.g. window_adapter_listener_test): glintfx.so hides
// this class by default (CXX_VISIBILITY_PRESET hidden,
// cmake/GlintfxLibrary.cmake), and it is not public API anyway
// (GODS_LAWS.md L-19) - egl_context_adapter.cpp is compiled a SECOND
// time directly into this test's own executable, so this file's own
// global operator new/delete override below reaches every allocation
// production code performs, with no DSO boundary to cross (this file's
// own reasoning matches tests/gfui_complex_match_resource_exhausted_
// test.cpp's own "NO DLL-CROSSING GAP TO CLOSE HERE" paragraph
// exactly, for the identical reason).

namespace {

using glintfx::platform::wayland_egl_context_adapter;

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

// Same MSVC-ASan-only declare-and-skip valve tests/err_context_test.
// cpp's own oom_forcing_declared_not_applicable() uses (this file's
// own header comment: no DLL boundary here, but this leg is Linux-
// only anyway - kept for the SAME reason gfui_complex_match_resource_
// exhausted_test.cpp's own duplicate keeps it, honest declaration over
// silent assumption if this file is ever built under an ASan variant
// on a platform where the override loses the race).
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(__SANITIZE_ADDRESS__)
    return std::getenv("GLINTFX_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr ||
           true; // ASan's own operator new/delete wins by default over any
                 // user override linked into the same binary (learn.
                 // microsoft.com/cpp/sanitizers/asan-known-issues, the
                 // same citation this suite's other OOM-forcing tests
                 // already give in full) - declared unconditionally
                 // true under any ASan build, GCC or MSVC alike, rather
                 // than assumed safe on GCC's ASan without measuring it
                 // here.
#else
    return std::getenv("GLINTFX_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "gl_proc_address_oom_test: {} declared NOT APPLICABLE under AddressSanitizer "
                 "(this TU's own operator new/delete override never gets a chance to run under "
                 "ASan, so the assertion this case exists to prove would measure nothing)",
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

// THE CASE (plan's own F1, case 2 of "teste que falha ANTES"): against
// the pre-fix site (`const std::string owned(name);` inside a
// `noexcept` function), an allocation failure forced on the VERY NEXT
// call escapes as std::bad_alloc out of a noexcept function, which
// [except.terminate] mandates calls std::terminate() - this whole test
// BINARY dies before GLINTFX_CHECK ever runs, and ctest reports it as
// a crashed process (this fatia's own report cites the captured
// "terminate called after throwing an instance of 'std::bad_alloc'"
// against the SHA before this fix), not a clean assertion failure.
// After the fix, the call returns normally instead - WITHOUT the
// override ever firing at all (this file's own header comment, "THE
// SECOND MEASUREMENT"), so this case does not (and, post-fix, cannot)
// assert the override was reached: only that the process survives.
//
// MUTATION m1 OF THE PLAN'S OWN F1 SECTION LIVES HERE: reverting the
// site to `const std::string owned(name);` makes this exact case
// std::terminate() again, because the armed override IS reached again
// the moment the site allocates.
GLINTFX_TEST(proc_address_survives_an_armed_allocator) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable("proc_address_survives_an_armed_allocator");
        return;
    }

    wayland_egl_context_adapter adapter; // never open()'d - proc_address()
                                         // touches no member state (this
                                         // file's own header comment).

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // the very next allocation, if any, fails
    const void *first = adapter.proc_address("glClearNamedFramebufferfvEXT");
    g_force_alloc_failure = false;

    // Reaching this line at all already proves the process did not
    // std::terminate() - before this fatia's own fix, it never did.
    // eglGetProcAddress() may or may not resolve this name from
    // whatever headless libEGL this leg happens to load (GODS_LAWS.md
    // L-44: a driver's answer is a fact of the environment, never
    // something to assert about safely across every runner), so this
    // test asserts nothing about `first`'s VALUE.
    (void)first;

    // A second, healthy call (override no longer armed) proves the
    // adapter itself carries no corrupted state forward from the
    // first - the same "recovers" control tests/gfui_complex_match_
    // resource_exhausted_test.cpp's own second case already gives.
    const void *second = adapter.proc_address("glGetUnsignedBytevEXT");
    (void)second;
}

// THE POSITIVE FORM OF DEGRAU 1 (plan sec. 2, "nao alocar"; this
// file's own header comment, "THE SECOND MEASUREMENT"): with a HEALTHY
// allocator (never armed), resolving a real, long-enough-to-defeat-SSO
// name allocates exactly zero times - the same "no_alloc" shape the
// plan's own F3/F4 sections use (gpu_kind_exclusion_no_alloc_test,
// gfss_anb_parse_no_alloc_test). This is the case that actually proves
// degrau 1 was taken, independent of whatever eglGetProcAddress()
// itself happens to do internally on any given driver.
GLINTFX_TEST(proc_address_allocates_nothing_resolving_a_long_name) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable("proc_address_allocates_nothing_resolving_a_long_name");
        return;
    }

    wayland_egl_context_adapter adapter;
    const std::size_t calls_before = g_override_new_call_count;
    const void *result = adapter.proc_address("glClearNamedFramebufferfvEXT");
    (void)result;
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);
}
