// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/loop/loop_impl.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// loop_open_alloc_failure_test.cpp - LOOP-RUN fatia 6b (docs/plano-
// w6b-fatias-6-8.md sec. 8.3, F17, GODS_LAWS.md L-20/L-22/L-27): the
// TDD red/green witness for glintfx::allocate_loop_impl() (src/
// platform/loop/loop_impl.hpp/.cpp) - proves the ONE allocation
// gltfx_loop::open() makes for its own opaque impl degrades to
// gltfx_err_code::out_of_memory, never std::terminate(), when the heap
// refuses it.
//
// RED, SEEN: before loop_impl.{hpp,cpp} existed, this file's own
// #include "platform/loop/loop_impl.hpp" line failed to compile (no
// such header, no such function).
//
// WHY THIS TESTS allocate_loop_impl() DIRECTLY, NEVER gltfx_loop::
// open() ITSELF (loop_impl.hpp's own header comment on that function
// gives the full reasoning - summarized here for whoever reads this
// test without reading that header first): gltfx_loop::open() takes a
// gltfx_display&/gltfx_window&/gltfx_gl_context& that must already
// report is_open() == true, and the ONLY way to produce one of those
// is through ITS OWN real, platform-touching open() - there is no
// public or internal seam that fabricates an "already open" handle out
// of nothing. A test that needs to run on all five systems, with no
// live Wayland/Win32 connection (this file registers unguarded,
// GODS_LAWS.md L-09: this is not a window/input/display test), cannot
// call the public facade at all. allocate_loop_impl() is the pure-
// enough atom that makes the ONE thing this case needs to prove -
// "the allocation this facade's open() performs translates a failed
// heap request into out_of_memory, never a crash" - reachable without
// touching the operating system, the same "decides before either
// adapter is ever consulted" property src/platform/gl/gfx_open_only_
// fixation.hpp's own resolve_gfx_open_only_fixation() already has, one
// fatia over, for the identical reason (that function's own test,
// gfx_open_only_fixation_test.cpp, is this file's own direct
// template).
//
// NO win_dll_alloc_hook.hpp NEEDED (unlike err_context_test.cpp's own
// use of it, tests/harness/win_dll_alloc_hook.hpp's own header
// comment): allocate_loop_impl() carries no GLINTFX_API (GODS_LAWS.md
// L-19, "nada e exportado" for an internal atom) and this file's own
// target_sources() (tests/CMakeLists.txt) recompiles loop_impl.cpp a
// SECOND time, straight into THIS test binary - the exact same "no
// DLL boundary to cross" shape gfx_open_only_fixation_test.cpp's own
// header comment already documents for the identical reason. This
// TU's own operator new/new(nothrow) overrides below are therefore
// sufficient on every platform this project builds for, including
// Windows/SHARED - there is no separate glintfx.dll call site to
// reach, because this call never goes through one.
//
// NO N-TH-ALLOCATION CALIBRATION NEEDED (unlike gfx_open_only_
// fixation_test.cpp's own g_fail_at_alloc_number dance, forced by that
// function's std::vector::push_back()): loop_impl carries no std::
// vector/std::string/std::function member (loop_impl.hpp's own struct
// - three raw pointers, a frame_cap_schedule of two POD fields, a
// gltfx_time_point, an enum, a std::uint64_t), so the very FIRST
// allocation allocate_loop_impl() ever makes IS the one this case
// needs to fail - a plain force-fail boolean (the SAME shape err_
// no_alloc_test.cpp's own simpler counter already uses one directory
// over) is enough.

namespace {

bool g_force_alloc_failure = false;

// Counts every call to THIS TU's own operator new/new(nothrow)
// overrides below, regardless of whether g_force_alloc_failure is
// armed - the same "prove the override was actually reached before
// trusting what happened as a result" discipline err_context_test.
// cpp's own g_override_new_call_count already documents one directory
// over, applied here to a single call site instead of two.
std::size_t g_override_new_call_count = 0;

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

GLINTFX_TEST(allocate_loop_impl_degrades_to_out_of_memory_when_the_heap_refuses) {
    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true;
    const glintfx::gltfx_rslt<glintfx::loop_impl *> allocated = glintfx::allocate_loop_impl();
    g_force_alloc_failure = false;

    // Proves the FORCING MECHANISM actually reached the allocator this
    // call needed - see this file's own "NO win_dll_alloc_hook.hpp
    // NEEDED" header comment. A failure HERE, not below, means this
    // platform/configuration could not force the failure at all, not
    // that the library mishandled a real one.
    GLINTFX_CHECK(g_override_new_call_count > calls_before);

    // Degrades to out_of_memory - never a crash (proven simply by
    // reaching this line at all: an escaped std::bad_alloc from a
    // noexcept function calls std::terminate(), which would have ended
    // the process before this check ever ran).
    GLINTFX_CHECK(allocated.has_error());
    GLINTFX_CHECK(allocated.err().code() == glintfx::gltfx_err_code::out_of_memory);
}

GLINTFX_TEST(allocate_loop_impl_succeeds_with_a_healthy_allocator) {
    const glintfx::gltfx_rslt<glintfx::loop_impl *> allocated = glintfx::allocate_loop_impl();

    GLINTFX_CHECK(allocated.has_value());
    const glintfx::loop_impl *impl = allocated.value();
    GLINTFX_CHECK(impl != nullptr);

    // Default-initialized exactly as loop_impl.hpp's own struct
    // documents - nothing left indeterminate for open() (loop_facade.
    // cpp) to overwrite blindly.
    GLINTFX_CHECK(impl->display == nullptr);
    GLINTFX_CHECK(impl->window == nullptr);
    GLINTFX_CHECK(impl->context == nullptr);
    GLINTFX_CHECK(impl->book.last_present == glintfx::gltfx_present_outcome::presented);
    GLINTFX_CHECK(impl->book.frame_index == 0);

    delete impl;
}
