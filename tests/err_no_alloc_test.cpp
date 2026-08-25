// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
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

std::size_t g_alloc_count = 0;
std::size_t g_dealloc_count = 0;

void reset_counts() {
    g_alloc_count = 0;
    g_dealloc_count = 0;
}

} // namespace

void *operator new(std::size_t size) {
    ++g_alloc_count;
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void *p) noexcept {
    ++g_dealloc_count;
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
