// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/wgl_proc_address.hpp"

// win32_gl_proc_address_oom_test.cpp - the Windows twin of tests/
// gl_proc_address_oom_test.cpp (NOEXCEPT-ALLOC-B8 fatia F1, GODS_LAWS.md
// L-04/L-09/L-20/L-22), closing the PARITY-GATE gap the server found:
// gl_proc_address_oom_test existed on Linux with no counterpart here.
//
// PLAN B, DECLARED (not the original plan A this fatia's briefing
// described): the briefing expected this file to close over win32_gl_
// context_adapter (default-constructed, never open()'d, proc_address()
// only delegating). Measured against the real tree instead: win32_gl_
// context_adapter::proc_address() (wgl_context_adapter.cpp:624-626) is
// ONE LINE - "return resolve_wgl_proc_address(name);" - but compiling
// wgl_context_adapter.cpp a second time (the "the whole .cpp is one
// link unit" technique every win32/*_test target in this file already
// uses) would drag in win32_window_adapter, wgl_extension_loader, gl_
// version_policy, gl_memory_facts, gpu_kind_exclusion, gpu_kind_seam,
// memory_separation_kind, gpu_kind_report, and the four dxcore atoms -
// the SAME set win32_iconic_present_test (tests/CMakeLists.txt) already
// needs to drag in just to exercise the whole adapter. NONE of those
// files take part in the allocation this test exists to prove - the
// entire body of the real site is resolve_wgl_proc_address() (wgl_proc_
// address.cpp), the exact function proc_address() delegates to in full,
// with no intermediate step that touches memory. This file therefore
// links ONLY wgl_proc_address.cpp against opengl32, the same minimal
// shape tests/win32_wgl_proc_address_test.cpp (tests/CMakeLists.txt,
// same if(WIN32) block) already uses for the identical source file, and
// calls resolve_wgl_proc_address() directly instead of going through
// the adapter's one-line shell.
//
// NO WINDOW, NO DEVICE CONTEXT, NO RENDERING CONTEXT NEEDED (same
// reasoning win32_wgl_proc_address_test.cpp already documents for
// itself): resolve_wgl_proc_address() only calls wglGetProcAddress()
// (which accepts the absence of a current context, returning one of
// its five documented "unresolved" shapes) and, when that path fails,
// GetProcAddress() against the already-loaded opengl32.dll module -
// neither call needs a live window.
//
// WHAT THIS TEST IS, AND WHAT IT IS NOT - READ BEFORE ASSUMING IT
// CAUGHT A LIVE BUG: resolve_wgl_proc_address() never allocated via
// std::string on this platform, not even before this fatia existed.
// commit babbd77 already replaced `const std::string owned(name)` with
// a fixed stack buffer (copy_name_into()), and THIS SAME NIGHT's F1
// fatia migrated that buffer into the shared nul_terminated_name.hpp
// atom (copy_nul_terminated() into a std::array<char, k_max_proc_name_
// chars>, 256 bytes) - never a std::string, never an allocation, for
// any name within that ceiling. CONSEQUENCE, stated plainly: unlike a
// test written against a site that was actually broken at the moment
// of writing, the two cases below do NOT catch a defect alive today.
// Case 1 (proc_address_does_not_terminate_under_an_armed_allocator) is
// a REGRESSION DETECTOR - it would catch someone reintroducing
// `const std::string owned(name)` here, the same mistake clang-tidy's
// bugprone-exception-escape once caught on the real Windows CI runner
// for this very file (wgl_proc_address.cpp's own header comment). Case
// 2 (proc_address_allocates_nothing_resolving_a_long_name) is a
// POSITIVE PROOF that the site allocates zero, mirroring the Linux
// sibling's own "THE SECOND MEASUREMENT" case. Neither case is red
// against the site as it stands in this tree - a future reader must
// not build on the idea that this file demonstrates a bug it found.
//
// NAMES OF 20+ CHARACTERS, SAME REASON AS THE LINUX SIBLING (gl_proc_
// address_oom_test.cpp's own "THE NAME MATTERS" paragraph): a short
// name like "glClear" fits small-string-optimization and would not
// have allocated even against the pre-babbd77 site - the names used
// here (glGetUnsignedBytevEXT, wglGetExtensionsStringARB) are the SAME
// ones wgl_context_adapter.cpp already cites at that length (lines
// 451/465), so they stay meaningful even though this file never links
// that .cpp.
//
// WHAT STAYS UNPROVEN, DECLARED NOT ASSUMED (GODS_LAWS.md L-20/L-44):
// the RED (reverting the site to `const std::string owned(name)` and
// watching case 1 call std::terminate()) is not executable on this
// machine - this project has no Windows toolchain here, the same
// limitation tests/win32_wgl_proc_address_test.cpp's own header
// comment already declares for itself. The windows-latest server
// runner is what proves GREEN; running under wine64, once released, is
// a CLUE, never an oracle (tools/msvc-container/README.md already
// fixes this distinction).

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

// Same MSVC-ASan valve tests/err_context_test.cpp's own oom_
// forcing_declared_not_applicable() uses, in the same three-branch shape:
// MSVC's own <cstdlib> flags std::getenv() as C4996 ("may be unsafe")
// under /W4, which -DGLINTFX_WERROR=ON escalates to a build failure, so
// a plain "#if ASan #else getenv #endif" would break plain Windows
// builds for a code path they never take. Splitting the Windows case in
// two (this file only ever compiles under defined(_WIN32), so there is
// no third, non-Windows branch to fall through to here) keeps that call
// scoped to where it can ever matter - see err_context_test.cpp for the
// full citation and the Linux sibling that still needs the third branch.
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(__SANITIZE_ADDRESS__)
    return true; // learn.microsoft.com/cpp/sanitizers/asan-known-issues,
                 // "Overriding operator new and delete": ASan's own
                 // operator new/delete wins by default over any user
                 // override linked into the same binary - the same
                 // citation err_context_test.cpp's own header comment
                 // already gives in full.
#elif defined(_WIN32)
    return false;
#else
    return std::getenv("GLINTFX_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "win32_gl_proc_address_oom_test: {} declared NOT APPLICABLE under MSVC "
                 "AddressSanitizer (this TU's own operator new/delete override never gets a "
                 "chance to run under ASan, so the assertion this case exists to prove would "
                 "measure nothing)",
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

// THE REGRESSION DETECTOR (this file's own header comment, "WHAT THIS
// TEST IS, AND WHAT IT IS NOT"): against today's site (copy_nul_
// terminated() into a std::array, no allocation at all), this call
// trivially does not terminate even with the very next allocation
// armed to fail - the real, positive proof is the case below (proc_
// address_allocates_nothing_resolving_a_long_name). This case earns
// its keep the moment someone reintroduces `const std::string
// owned(name)` at the site: the armed override is REACHED again, the
// bad_alloc escapes a noexcept function, and [except.terminate]
// mandates std::terminate() - the WHOLE test binary dies before
// GLINTFX_CHECK ever runs, and ctest reports it as a crashed process,
// not a clean assertion failure. What this case actually asserts is
// exactly that: the process did not call std::terminate() - never
// anything about the resolved address itself.
GLINTFX_TEST(proc_address_does_not_terminate_under_an_armed_allocator) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "proc_address_does_not_terminate_under_an_armed_allocator");
        return;
    }

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // the very next allocation, if any, fails
    void *first = glintfx::platform::resolve_wgl_proc_address("glGetUnsignedBytevEXT");
    g_force_alloc_failure = false;

    // Reaching this line at all already proves the process did not
    // std::terminate(). Which headless driver this runner loads is a
    // FACT OF THE ENVIRONMENT (GODS_LAWS.md L-44) - this test asserts
    // nothing about `first`'s VALUE, only that the call returned
    // normally.
    (void)first;

    // A second, healthy call (override no longer armed) proves the
    // function carries no corrupted state forward - the same shape the
    // Linux sibling case already uses.
    void *second = glintfx::platform::resolve_wgl_proc_address("wglGetExtensionsStringARB");
    (void)second;
}

// THE POSITIVE FORM (the one that actually proves degrau 1 was taken,
// GODS_LAWS.md L-44 - independent of whatever this headless driver does
// internally): with a healthy allocator (never armed), resolving a
// real, long-enough-to-defeat-small-string-optimization name allocates
// exactly zero times. SSO is moot at this specific site (it never uses
// std::string at all, only nul_terminated_name.hpp's own fixed array),
// but the name stays long for the same reason the header comment gives
// - it matches names this project already cites at that length.
GLINTFX_TEST(proc_address_allocates_nothing_resolving_a_long_name) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable("proc_address_allocates_nothing_resolving_a_long_name");
        return;
    }

    const std::size_t calls_before = g_override_new_call_count;
    void *result = glintfx::platform::resolve_wgl_proc_address("glGetUnsignedBytevEXT");
    (void)result;
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);
}

#endif // defined(_WIN32)
