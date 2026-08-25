// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <glintfx/core/err_code.hpp>
#include <glintfx/export.hpp>

// core/err.hpp - gltfx_err (CE-2/CE-3) and gltfx_rslt<T> (CE-4) of
// CORE-ERROR (TODO.md, GODS_LAWS.md L-19/L-22/L-26): the frozen-
// footprint error envelope every fallible public call in glintfx
// returns, and the return-value envelope that carries it.
//
// gltfx_rslt<T> (CE-4, decision of the leader, TODO.md CORE-ERROR row,
// 25/08/2026): ONE convention for the WHOLE library - every fallible
// function returns gltfx_rslt<T>, INCLUDING when there is no success
// value (gltfx_rslt<void>, the template specialization below) -
// THERE IS NO SECOND FORM. No function anywhere in glintfx returns a
// bare gltfx_err, a bool-plus-out-parameter, or an errno-style
// integer. `[[nodiscard]]` sits on the CLASS TEMPLATE itself (and on
// its <void> specialization), not on each individual function: every
// function that returns a gltfx_rslt<T>, anywhere in this library or
// in a consumer's own code that adopts the same convention, gets the
// discard diagnostic FOR FREE, structurally, without anyone having to
// remember to tag it. tests/tools/check_nodiscard_rslt.sh proves this
// is a REAL compiler diagnostic, not decoration, by compiling a
// fixture that drops the result and asserting the compile FAILS,
// naming the diagnostic.
//
// gltfx_rslt<T> is a TEMPLATE, so - unlike gltfx_err above - it cannot
// be PIMPL'd behind an opaque pointer (GODS_LAWS.md L-19's opacity
// clause is scoped to "handle e subsistema com estado"; a generic
// value type instantiated per call site, where T may be a consumer's
// own type, has no single definition to hide behind a library-side
// pointer). It is therefore entirely INLINE, header-only, like
// std::optional/std::expected - there is nothing of it in err.cpp.
//
// noexcept AT THE FUNCTION BOUNDARY, not necessarily inside every
// gltfx_rslt<T> member (decision of the leader: "a assinatura padrao
// ... e noexcept, porque a fronteira captura e traduz"): every
// FALLIBLE FUNCTION SIGNATURE in glintfx is
// `[[nodiscard]] gltfx_rslt<T> foo(...) noexcept` - internally it may
// use exceptions freely (GODS_LAWS.md L-22/CONTRACT.md 6.4: exceptions
// are permitted INSIDE the library, forbidden only crossing the public
// boundary), and its own top-level try/catch translates any of them
// into `gltfx_rslt<T>::err(...)` before returning. gltfx_rslt<T>'s OWN
// `ok()` factory is not unconditionally noexcept (T is caller-supplied
// and may have a throwing move constructor, exactly like
// std::optional/std::expected do not force T to be nothrow either) -
// the noexcept GUARANTEE lives at the function that WRAPS the factory
// call in that try/catch, not inside the envelope type itself.
//
// value()/error() PRECONDITION (same convention as
// std::optional::operator*()/std::expected::operator*(), not a new
// idiom invented here): calling value() when has_value() is false, or
// error() when has_error() is false, is UNDEFINED BEHAVIOR by
// documented precondition. This is a DIFFERENT category from
// gltfx_err's own "never UB" accessors (CE-3): those answer a DATA
// question ("was this optional diagnostic field ever attached?",
// always well-defined to answer with empty/zero); has_value()/
// has_error() here answer a DIFFERENT, universally-checked-first
// question ("did the call succeed?") before value()/error() are ever
// called - the same two-step contract std::optional itself uses.
//
// CORRECTION (adversarial review, 25/08/2026): an earlier version of
// this comment claimed std::get_if (returns nullptr, dereferenced by
// the caller) was chosen over std::variant::get() (throws
// std::bad_variant_access) specifically to avoid "killing the
// process". That claim was WRONG, and the reviewer reproduced why:
// get() throwing through a `noexcept` function calls std::terminate()
// (abort) exactly as surely as dereferencing the resulting null
// pointer segfaults - misuse kills the process EITHER WAY, only the
// SIGNAL and the amount of information differ. What get_if genuinely
// buys is avoiding exception machinery inside a noexcept function (a
// simplicity, not a safety property).
//
// DEBUG-ONLY PRECONDITION GUARD (decision of the leader, 25/08/2026,
// correcting the claim above): value()/error() on BOTH gltfx_rslt<T>
// and the gltfx_rslt<void> specialization now assert() the
// precondition before touching storage. In a build where NDEBUG is
// undefined (this project's CMAKE_BUILD_TYPE=Debug - the STANDARD
// language mechanism <cassert> already gives every C++ toolchain this
// project targets, not a project invention), the assert fires FIRST,
// with a message naming exactly which precondition the CALLER
// violated, and calls abort() - a deterministic SIGABRT the consumer
// can read and act on, instead of: a confusing SIGSEGV with zero
// context (the primary template's null-pointer dereference), or worse,
// SILENT GARBAGE (the T=void specialization's std::optional::
// operator*() on an unengaged optional does not reliably fault at all
// - it can hand back a fabricated gltfx_err whose m_context pointer,
// if later copied or destroyed, corrupts the heap; this is a MORE
// dangerous misuse than a crash, not a safer one). In a build where
// NDEBUG IS defined (CMAKE_BUILD_TYPE=Release, this project's default
// for CI/tools/preci.sh), assert() compiles to nothing - zero cost -
// and the SAME undefined behavior this code already had before this
// guard still happens, completely unchanged: this guard adds
// diagnosability in debug, never changes the release contract.
// tests/tools/check_rslt_precondition.sh proves both halves live -
// see that script's own header. docs/api-conventions.md documents
// this for the external, unknown consumer base (LEI ZERO): they need
// to read, in prose, that this is a precondition violation and what
// each build mode does about it, before their own program crashes and
// they have to reverse-engineer why.
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

// CE-4: the single return-value envelope. See the header comment above
// for the full design rationale (one convention, [[nodiscard]] on the
// class template itself, noexcept lives at the FUNCTION boundary).
template <typename T> class [[nodiscard]] gltfx_rslt {
  public:
    [[nodiscard]] static gltfx_rslt ok(T value) {
        return gltfx_rslt(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static gltfx_rslt err(gltfx_err error) noexcept {
        return gltfx_rslt(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept { return m_storage.index() == 0; }
    [[nodiscard]] bool has_error() const noexcept { return m_storage.index() == 1; }

    // Precondition: has_value(). UB otherwise if the assert below is
    // compiled out (NDEBUG/Release) - see the header comment's
    // "value()/error() PRECONDITION" paragraph for both halves.
    [[nodiscard]] const T &value() const noexcept {
        assert(has_value() &&
               "gltfx_rslt<T>::value() called on a result that holds an error, not a value - "
               "call has_value() first");
        return *std::get_if<0>(&m_storage);
    }
    [[nodiscard]] T &value() noexcept {
        assert(has_value() &&
               "gltfx_rslt<T>::value() called on a result that holds an error, not a value - "
               "call has_value() first");
        return *std::get_if<0>(&m_storage);
    }

    // Precondition: has_error(). UB otherwise if the assert below is
    // compiled out (NDEBUG/Release).
    [[nodiscard]] const gltfx_err &error() const noexcept {
        assert(has_error() &&
               "gltfx_rslt<T>::error() called on a result that holds a value, not an error - "
               "call has_error() first");
        return *std::get_if<1>(&m_storage);
    }

  private:
    explicit gltfx_rslt(std::in_place_index_t<0>, T value)
        : m_storage(std::in_place_index<0>, std::move(value)) {}
    explicit gltfx_rslt(std::in_place_index_t<1>, gltfx_err error) noexcept
        : m_storage(std::in_place_index<1>, std::move(error)) {}

    std::variant<T, gltfx_err> m_storage;
};

// The T = void specialization CE-4's "no second form" decision
// requires: a fallible function with nothing to return on success
// still returns gltfx_rslt<T>, here with T = void - C++ cannot store a
// `void` value, so the storage is just "is there an error or not"
// (std::optional<gltfx_err>), but the PUBLIC SHAPE mirrors the primary
// template exactly minus value() (there is nothing to read on
// success).
template <> class [[nodiscard]] gltfx_rslt<void> {
  public:
    [[nodiscard]] static gltfx_rslt ok() noexcept { return gltfx_rslt(std::nullopt); }

    [[nodiscard]] static gltfx_rslt err(gltfx_err error) noexcept {
        return gltfx_rslt(std::optional<gltfx_err>(std::move(error)));
    }

    [[nodiscard]] bool has_value() const noexcept { return !m_error.has_value(); }
    [[nodiscard]] bool has_error() const noexcept { return m_error.has_value(); }

    // Precondition: has_error(). UB otherwise if the assert below is
    // compiled out (NDEBUG/Release) - same documented precondition as
    // std::optional::operator*() itself uses, which is exactly what
    // clang-tidy is flagging below (it cannot see that the assert
    // JUST proved has_error() any more than it could see a caller-side
    // check before a bare std::optional::operator*() call).
    [[nodiscard]] const gltfx_err &error() const noexcept {
        assert(has_error() &&
               "gltfx_rslt<void>::error() called on a result that holds success (ok()), not an "
               "error - call has_error() first");
        return *m_error; // NOLINT(bugprone-unchecked-optional-access) reason: documented
                         // precondition, just asserted above
    }

  private:
    explicit gltfx_rslt(std::optional<gltfx_err> error) noexcept : m_error(std::move(error)) {}

    std::optional<gltfx_err> m_error;
};

} // namespace glintfx
