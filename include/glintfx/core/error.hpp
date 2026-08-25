// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <type_traits>
#include <utility>

#include <glintfx/core/err_code.hpp>
#include <glintfx/export.hpp>

// core/error.hpp - gltfx_err (CE-2 of CORE-ERROR, TODO.md,
// GODS_LAWS.md L-19/L-22/L-26): the frozen-footprint error envelope
// every fallible public call in glintfx returns (inside gltfx_rslt<T>,
// CE-4, not written yet).
//
// SEMI-OPAQUE ENVELOPE, FIXED FOOTPRINT (CTO design, CORE-ERROR plan):
// exactly two members - the code, INLINE (common-path read, zero
// indirection), and an opaque pointer to a context that MAY be null.
// gltfx_err is instantiated directly in the CONSUMER's own binary
// (there is no template here, but the same reasoning applies to any
// concrete value type crossing the boundary), so its SIZE is ABI the
// moment a consumer compiles against it and can never shrink or grow
// again (GODS_LAWS.md L-26). The CONTENT behind the opaque pointer
// does not carry that constraint - it lives entirely inside
// error.cpp, on the library's side of the boundary, and growing it
// later (CE-3 and beyond) is additive, bumping B, never A.
//
// LIFECYCLE, AND THIS IS A SAFETY RULE, NOT STYLE: the copy
// constructor and the destructor are declared here and DEFINED in
// error.cpp, exported (GLINTFX_API) - so the allocation (copy ctor)
// and the matching deallocation (destructor) both run INSIDE the
// library's own compiled object code, no matter which side of the
// .so/.dll boundary calls them. On Windows, a block allocated by one
// C runtime and freed by a DIFFERENT one (library vs. executable, each
// linking its own CRT) corrupts the heap; keeping new/delete on the
// SAME side is what this design exists to guarantee. Copy is DEEP;
// move STEALS the pointer, is noexcept, and stays fully inline (moving
// a pointer VALUE never needs err_context's complete layout).
//
// COLLISION CHECKLIST (GODS_LAWS.md L-19/CORE-ERROR; extended by the
// CE-1 correction to also cover standard-library names, not just
// system macros - see err_code.hpp's own header comment for why):
// `gltfx_err` and `err_context` were checked against both lists.
// Neither collides with a Windows/glibc/POSIX macro (min/max/ERROR/
// DELETE/IN/OUT/CONST/VOID/TRUE/FALSE/interface/small/near/far/STRICT/
// major/minor/makedev/stdin/stdout/stderr/unix/linux) nor with a
// standard-library name (error/error_code/error_condition/result/
// expected) - `gltfx_err` is one character short of literal collision
// with none of them, on purpose.

namespace glintfx {

// Opaque (GODS_LAWS.md L-19): the full layout is a private
// implementation detail, defined ONLY in error.cpp. A consumer never
// sees this type - gltfx_err below carries at most a pointer to one.
struct err_context;

class gltfx_err {
  public:
    // Trivial: sets the code, leaves the context null. Inline, and
    // NEVER allocates - proven, not promised, by
    // tests/error_no_alloc_test.cpp (CE-2): that executable replaces
    // the global allocator and COUNTS calls across construct, copy,
    // move and destroy of a context-less gltfx_err.
    explicit gltfx_err(gltfx_err_code code) noexcept : m_code(code) {}

    // Deep copy. Declared here, DEFINED in error.cpp, exported - see
    // the "LIFECYCLE" paragraph above for why this one crosses the
    // boundary instead of staying inline.
    GLINTFX_API gltfx_err(const gltfx_err &other);

    // Steals the pointer; never allocates, and never needs
    // err_context's complete layout (only the pointer VALUE moves), so
    // this stays inline even though err_context is incomplete here.
    gltfx_err(gltfx_err &&other) noexcept : m_code(other.m_code), m_context(other.m_context) {
        other.m_context = nullptr;
    }

    // Copy-and-swap, inline: gltfx_err(other) above performs the
    // out-of-line deep copy (the only step that can throw
    // std::bad_alloc); swapping members afterwards only touches the
    // pointer VALUE, not err_context's layout. `tmp`'s destructor - the
    // exported ~gltfx_err() below - frees whatever *this used to own,
    // on the library's side of the boundary, the moment `tmp` goes out
    // of scope at the end of this function.
    gltfx_err &operator=(const gltfx_err &other) {
        if (this != &other) {
            gltfx_err tmp(other);
            std::swap(m_code, tmp.m_code);
            std::swap(m_context, tmp.m_context);
        }
        return *this;
    }

    // Same swap technique, noexcept: whatever *this used to own is
    // freed once `other` (now holding it) is destroyed by the caller.
    gltfx_err &operator=(gltfx_err &&other) noexcept {
        if (this != &other) {
            std::swap(m_code, other.m_code);
            std::swap(m_context, other.m_context);
        }
        return *this;
    }

    // Frees m_context, if any. Declared here, DEFINED in error.cpp,
    // exported - see the "LIFECYCLE" paragraph above.
    GLINTFX_API ~gltfx_err();

    // Inline: reads a plain field, no boundary crossing needed.
    [[nodiscard]] gltfx_err_code code() const noexcept { return m_code; }

  private:
    gltfx_err_code m_code;
    err_context *m_context = nullptr;
};

// Frozen footprint (GODS_LAWS.md L-19/L-26, CORE-ERROR CE-2): two
// pointers wide on every platform this project targets - the unit is
// sizeof(void*), not a literal byte count, so this holds on both
// 32-bit and 64-bit. m_code (4 bytes) plus alignment padding plus
// m_context (one pointer) always rounds up to exactly that, because
// nothing else lives in this class; nothing added later (CE-3 and
// beyond) is allowed to move this number, only what m_context points
// AT may grow.
static_assert(sizeof(gltfx_err) == 2 * sizeof(void *),
              "gltfx_err footprint is frozen ABI, GODS_LAWS.md L-19/L-26 (CORE-ERROR CE-2)");

// A type trait, not a promise: move must stay noexcept so a consumer's
// own std::vector<gltfx_err> (or any other container) can move-grow
// without falling back to copies under exception-safety rules.
static_assert(std::is_nothrow_move_constructible_v<gltfx_err>,
              "gltfx_err move construction must stay noexcept, CORE-ERROR CE-2");
static_assert(std::is_nothrow_move_assignable_v<gltfx_err>,
              "gltfx_err move assignment must stay noexcept, CORE-ERROR CE-2");

} // namespace glintfx
