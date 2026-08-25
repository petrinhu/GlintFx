// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <glintfx/core/err_code.hpp>
#include <glintfx/export.hpp>

// core/err.hpp - gltfx_err (CE-2/CE-3 of CORE-ERROR, TODO.md,
// GODS_LAWS.md L-19/L-22/L-26): the frozen-footprint error envelope
// every fallible public call in glintfx returns (inside gltfx_rslt<T>,
// CE-4, not written yet).
//
// DIAGNOSTIC CONTEXT (CE-3): six accessors - path(), line(), column(),
// byte_offset(), rejected_value(), os_error_code() - read whatever
// diagnostic detail was attached through the matching with_*()
// method. Absent always reads back EMPTY (string_view) or ZERO
// (numeric), NEVER undefined behavior - v1 has no separate has_*()
// query, so the convention IS the contract: 0/empty means "not set".
// os_error_code() is a raw platform number with DIFFERENT origins per
// platform (POSIX errno on Linux, GetLastError() on Windows - both fit
// in an int64_t without truncation, unlike squeezing a Win32 DWORD
// into a signed 32-bit int); glintfx does not interpret it, only
// carries it.
//
// ATTACH IS BEST-EFFORT AND noexcept (decision of the leader, TODO.md
// CORE-ERROR row, 25/08/2026): if the context cannot be allocated, or
// a field cannot be copied in, the with_*() call silently does
// nothing and returns *this UNCHANGED - the ERROR BEING DECORATED
// degrades to carrying just its code (or whatever context it already
// had) instead of turning into a SECOND failure with no channel to
// report it through. This is a DIFFERENT mechanism from "library-wide
// out-of-memory becomes gltfx_err_code::out_of_memory and is
// RETURNED" (the leader's other OOM decision, which applies to
// call sites that already return a gltfx_rslt<T> - CE-4, not written
// yet): attaching a diagnostic to an error that is FAILING TO
// ALLOCATE PIECEMEAL, WHILE BEING CONSTRUCTED as a mutable in-place
// value, has no expected-typed return channel to carry that second
// failure through, and inventing one here would be exactly the
// "exception as control flow" CONTRACT.md 6.4 already forbids.
// tests/err_context_test.cpp proves the degrade path by FORCING an
// allocation to fail, not by inspecting the source for a try/catch.
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
// err.cpp, on the library's side of the boundary, and growing it
// later (CE-3 and beyond) is additive, bumping B, never A.
//
// LIFECYCLE, AND THIS IS A SAFETY RULE, NOT STYLE: the copy
// constructor and the destructor are declared here and DEFINED in
// err.cpp, exported (GLINTFX_API) - so the allocation (copy ctor)
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
// implementation detail, defined ONLY in err.cpp. A consumer never
// sees this type - gltfx_err below carries at most a pointer to one.
struct err_context;

class gltfx_err {
  public:
    // Trivial: sets the code, leaves the context null. Inline, and
    // NEVER allocates - proven, not promised, by
    // tests/err_no_alloc_test.cpp (CE-2): that executable replaces
    // the global allocator and COUNTS calls across construct, copy,
    // move and destroy of a context-less gltfx_err.
    explicit gltfx_err(gltfx_err_code code) noexcept : m_code(code) {}

    // Deep copy. Declared here, DEFINED in err.cpp, exported - see
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

    // Frees m_context, if any. Declared here, DEFINED in err.cpp,
    // exported - see the "LIFECYCLE" paragraph above.
    GLINTFX_API ~gltfx_err();

    // Inline: reads a plain field, no boundary crossing needed.
    [[nodiscard]] gltfx_err_code code() const noexcept { return m_code; }

    // CE-3 accessors. All read err_context's real layout, so all are
    // declared here and DEFINED in err.cpp, exported. Absent always
    // reads back empty/zero - see the "DIAGNOSTIC CONTEXT" paragraph
    // above.
    [[nodiscard]] GLINTFX_API std::string_view path() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint32_t line() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint32_t column() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint64_t byte_offset() const noexcept;
    [[nodiscard]] GLINTFX_API std::string_view rejected_value() const noexcept;
    [[nodiscard]] GLINTFX_API std::int64_t os_error_code() const noexcept;

    // CE-3 attach, best-effort and noexcept - see the "ATTACH IS
    // BEST-EFFORT" paragraph above. Strings are COPIED IN at the
    // moment of attaching (the caller's buffer may not outlive this
    // call). Returns *this by reference so calls chain:
    //   auto err = gltfx_err(gltfx_err_code::parse_failure)
    //                  .with_path(path)
    //                  .with_position(line, column);
    GLINTFX_API gltfx_err &with_path(std::string_view path) noexcept;
    GLINTFX_API gltfx_err &with_position(std::uint32_t line, std::uint32_t column) noexcept;
    GLINTFX_API gltfx_err &with_byte_offset(std::uint64_t offset) noexcept;
    GLINTFX_API gltfx_err &with_rejected_value(std::string_view value) noexcept;
    GLINTFX_API gltfx_err &with_os_error_code(std::int64_t code) noexcept;

  private:
    // Lazily allocates m_context if it is still null. noexcept,
    // best-effort: returns false (never throws, never crashes) if the
    // allocation itself fails, which is what lets every with_*() above
    // degrade instead of propagating a second failure.
    bool ensure_context() noexcept;

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
