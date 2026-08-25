// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

#include <glintfx/core/err_code.hpp>
#include <glintfx/core/error.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// error_context_test.cpp - CE-3 of CORE-ERROR (TODO.md, GODS_LAWS.md
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
// in error.cpp uses `new (std::nothrow)`, std::string's own internal
// growth uses the throwing form) with a pass-through to malloc/free
// that can be armed to fail on demand via g_force_alloc_failure. Only
// the two OOM-degradation cases below arm it; every other case in this
// file runs with a normal, working allocator - safe because
// glintfx_add_test() (cmake/GlintfxTest.cmake) gives every test case
// its OWN executable, so there is no ODR collision with any other TU.

namespace {

bool g_force_alloc_failure = false;

// Long enough (well past libstdc++/libc++'s small-string-optimization
// capacity, typically 15-22 bytes) to force std::string::assign() to
// actually call the THROWING global operator new below, not just
// reuse its inline buffer - otherwise the "string growth fails mid-
// attach" case would never exercise the code path it exists to prove.
constexpr std::string_view k_long_value =
    "a value long enough to defeat small-string-optimization and force a real heap allocation";

} // namespace

void *operator new(std::size_t size) {
    if (g_force_alloc_failure) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    if (g_force_alloc_failure) {
        return nullptr;
    }
    return std::malloc(size);
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

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
    // constructor's deep-copy branch (error.cpp) exists to prevent,
    // and the exact bug a shallow-copy regression would reintroduce.
    original.with_path("mutated/path.txt").with_position(99, 99);

    GLINTFX_CHECK(copy.path() == std::string_view{"original/path.txt"});
    GLINTFX_CHECK(copy.line() == 1);
    GLINTFX_CHECK(copy.column() == 1);

    GLINTFX_CHECK(original.path() == std::string_view{"mutated/path.txt"});
    GLINTFX_CHECK(original.line() == 99);
}

GLINTFX_TEST(attach_degrades_to_code_only_when_context_allocation_fails) {
    g_force_alloc_failure = true;
    glintfx::gltfx_err err(glintfx::gltfx_err_code::parse_failure);
    // ensure_context()'s `new (std::nothrow) err_context()` is the
    // VERY FIRST allocation with_path() would need (err has no
    // context yet) - forcing it to fail exercises the "context itself
    // cannot be allocated at all" branch.
    err.with_path("does/not/matter");
    g_force_alloc_failure = false;

    // Degrades to code-only: the ORIGINAL code survives unchanged (it
    // does NOT become out_of_memory - that translation is a SEPARATE
    // mechanism for call sites that already return a gltfx_rslt<T>,
    // see error.hpp's own "ATTACH IS BEST-EFFORT" comment), and no
    // exception escaped this noexcept call (proven simply by reaching
    // this line at all - an escaped exception from a noexcept function
    // calls std::terminate(), which would have ended the process
    // before this check ever ran).
    GLINTFX_CHECK(err.code() == glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(err.path().empty());
}

GLINTFX_TEST(attach_degrades_to_code_only_when_a_field_allocation_fails_mid_attach) {
    glintfx::gltfx_err err(glintfx::gltfx_err_code::unsupported);
    // Succeeds with the allocator healthy: ensure_context() runs here,
    // so the SECOND case below exercises a DIFFERENT failure point (a
    // field growing inside an ALREADY-allocated context), not context
    // allocation itself.
    err.with_position(3, 4);

    g_force_alloc_failure = true;
    // k_long_value is long enough to force std::string::assign() to
    // call the THROWING global operator new above; ensure_context()
    // itself is a no-op here (m_context is already non-null), so this
    // isolates the "string buffer growth throws mid-attach" branch.
    err.with_path(k_long_value);
    g_force_alloc_failure = false;

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
