// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/loop/loop_context_mark.hpp - LOOP-CONTEXT-MARK (S1d, docs/plano-loop-callbacks.md
// sec. 3.4/S1d, GODS_LAWS.md L-17/L-19/L-20/ L-22/L-28/L-32): layer 4 of the four-layer contract
// loop.hpp's own gltfx_loop_callbacks builds (docs/api-conventions.md R10) - an OPT-IN, DEBUG-ONLY
// liveness mark a consumer's own type can inherit from, so the thunks loop_bind.hpp's own layer 3
// generates can catch, in a build where NDEBUG is undefined, the single most dangerous way layer
// 3's own borrowing contract can be violated: calling a bound method on an object whose destructor
// has already run.
//
// ============================================================
// THE EFFECT, IN ONE SENTENCE: a consumer who writes `struct my_app :
// gltfx_loop_context_mark { ... }` and later destroys `my_app` while
// the loop still holds a gltfx_loop_callbacks bound to it (through
// gltfx_bind_loop_callbacks(), loop_bind.hpp) gets a deterministic,
// named abort() the NEXT time the loop tries to call into it, in any
// build where NDEBUG is undefined - instead of silent corruption or a
// crash with no diagnostic. In a build where NDEBUG is defined
// (Release, this project's default), the mark costs NOTHING: zero
// bytes, zero instructions, the exact same object layout as without
// it.
// ============================================================
//
// PRECEDENT (F22, docs/plano-loop-callbacks.md): the SAME shape
// include/glintfx/core/err.hpp already uses for gltfx_rslt<T>::
// value()/err()'s own debug-only precondition guard - a check
// compiled ONLY into a build without NDEBUG, that turns an existing
// undefined behavior into a deterministic, named abort() instead of
// changing what Release does. "adds diagnosability in debug, never
// changes the release contract" (err.hpp's own words) applies here
// unchanged, just guarding a DIFFERENT precondition (an object's own
// lifetime, not a gltfx_rslt<T>'s active alternative).
//
// WHERE THE CHECK LIVES, AND WHY IT CANNOT LIVE ANYWHERE ELSE (F21):
// this library's own .so/.dll is compiled ONCE, by whoever distributes
// it, in whatever mode THEY chose - a consumer may compile their OWN
// translation unit in Debug regardless. The check this header enables
// therefore has to live in code the CONSUMER compiles - loop_bind.
// hpp's own thunks, both header-only - never inside run() itself,
// which is compiled into the library's already-built binary and can
// never see the consumer's own NDEBUG setting.
//
// WHY A BASE CLASS, NOT A MEMBER (measured, F19): `[[no_unique_
// address]]` is not honored by MSVC (`[[msvc::no_unique_address]]` is
// a SEPARATE attribute it requires instead) - the empty-base-class
// optimization IS honored by all three compilers this project
// targets, so inheriting from an empty base class costs zero in
// Release (sizeof(marked) == sizeof(unmarked), proved by
// tests/loop_context_mark_test.cpp's own static_assert) where an
// EMPTY MEMBER of the same type, on MSVC, would not.

namespace glintfx {

#ifndef NDEBUG

// 'GLTX' read as a 32-bit word - an arbitrary but recognizable value,
// chosen so a raw memory dump makes the mark legible instead of
// looking like an ordinary zero-initialized field. Exists ONLY when
// NDEBUG is undefined - see "WHAT THIS LAYER SOLVES" below for what
// that buys, and this header's own top comment for why it cannot
// exist unconditionally.
inline constexpr std::uint32_t k_gltfx_loop_context_mark_live = 0x584C5447;

#endif

// ============================================================
// WHAT THIS LAYER SOLVES:
// ============================================================
//   - A consumer's own type opts in by inheriting from this class:
//     `struct my_app : gltfx_loop_context_mark { ... };`. loop_bind.
//     hpp's own thunks (gltfx_loop_frame_thunk/gltfx_loop_render_
//     thunk) detect the inheritance at COMPILE TIME (`if constexpr
//     (std::is_base_of_v<gltfx_loop_context_mark, Object>)`) and, in a
//     build where NDEBUG is undefined, assert() that the object still
//     carries the mark BEFORE calling through to the bound method.
//   - The destructor is the SOLE writer that clears the mark - the
//     one deterministic signal this layer has, independent of whether
//     the allocator happens to poison freed memory or not.
//   - Costs EXACTLY ZERO in a build where NDEBUG is defined: the mark
//     word does not exist at all (`#ifndef NDEBUG` around it), so the
//     base class is empty, and the empty-base-class optimization
//     every compiler this project targets already applies makes a
//     derived object's size identical to one that never inherited
//     from this class at all (F19, tests/loop_context_mark_test.cpp's
//     own static_assert).
// ============================================================
// WHAT THIS LAYER DOES NOT SOLVE - stated so nobody has to infer it
// from an absence (docs/plano-loop-callbacks.md sec. 3.4):
// ============================================================
//   - Does NOT stop the object from dying - it only lets a
//     SUBSEQUENT call notice, after the fact, that it already did.
//   - Only catches an object whose OWN destructor actually ran (the
//     mark is cleared there) or whose memory was reused/overwritten
//     by something that happens to zero or poison it - it does NOT
//     catch memory reused by ANOTHER object of the SAME marked type
//     at the SAME address (that new object carries a live mark of its
//     own, indistinguishable from the original one still being
//     alive).
//   - Exists ONLY when the CONSUMER compiles their OWN translation
//     unit without NDEBUG - a property of the CONSUMER's build, never
//     of how this library's own .so/.dll was built (see this header's
//     own top comment, "WHERE THE CHECK LIVES").
//   - A MOVED-FROM object stays alive and marked - moving a
//     gltfx_loop_context_mark-derived object does not run its
//     destructor, so the mark survives exactly as it would for a copy
//     of a live object (both are legitimate, live objects; this layer
//     answers "is this object's lifetime over", never "is this the
//     object you originally bound").
//   - THE SIZE OF A MARKED OBJECT DIFFERS BETWEEN A BUILD WITH NDEBUG
//     AND ONE WITHOUT IT. Never pass a marked object, or anything
//     derived from its size, across a boundary compiled with a
//     DIFFERENT NDEBUG setting than the code that allocated it - the
//     same rule any Debug/Release ABI mismatch already implies in
//     C++, stated explicitly here because this is the first type in
//     glintfx's own public surface whose size depends on the
//     consumer's own build mode.
// ============================================================
struct gltfx_loop_context_mark {
#ifndef NDEBUG
    std::uint32_t gltfx_mark_word = k_gltfx_loop_context_mark_live;

    gltfx_loop_context_mark() noexcept = default;
    // The ONLY writer that CLEARS the mark - the sole deterministic
    // signal this layer has (this header's own top comment, "WHY A
    // BASE CLASS").
    ~gltfx_loop_context_mark() noexcept { gltfx_mark_word = 0; }
    // Copying a LIVE object produces another LIVE object - copying
    // never runs a destructor, so the copy's own mark word is simply
    // initialized the same way the original's was (the default
    // member-wise copy already does exactly that).
    gltfx_loop_context_mark(const gltfx_loop_context_mark &) noexcept = default;
    gltfx_loop_context_mark &operator=(const gltfx_loop_context_mark &) noexcept = default;
    // A MOVE does not run the source's destructor either ("WHAT THIS
    // LAYER DOES NOT SOLVE" above) - defaulted for the same reason
    // the copy operations are: the moved-from object stays alive and
    // marked, on purpose, never zeroed here.
    gltfx_loop_context_mark(gltfx_loop_context_mark &&) noexcept = default;
    gltfx_loop_context_mark &operator=(gltfx_loop_context_mark &&) noexcept = default;
#endif
};

#ifndef NDEBUG
// gltfx_loop_context_mark_is_live - the ONE query loop_bind.hpp's own
// thunks call before dispatching through a marked object. A free
// function, not a member, on purpose: it needs nothing beyond the
// mark word itself, and keeping it free keeps gltfx_loop_context_mark
// a pure data holder with no behavior a consumer's own derived type
// could accidentally shadow or override.
[[nodiscard]] inline bool
gltfx_loop_context_mark_is_live(const gltfx_loop_context_mark &mark) noexcept {
    return mark.gltfx_mark_word == k_gltfx_loop_context_mark_live;
}
#endif

} // namespace glintfx
