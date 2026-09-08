// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/loop/frame_cap_schedule.hpp"

// loop_impl.hpp - LOOP-RUN fatia 6b (docs/plano-w6b-fatias-6-8.md sec.
// 8.3, D-W6b-41/44/49/55, GODS_LAWS.md L-17/L-19): the concrete type
// glintfx::loop_impl (forward-declared, opaque, in the public
// include/glintfx/platform/loop/loop.hpp) actually IS - the same shape
// display_impl.hpp/window_impl.hpp/gl_context_impl.hpp already give
// their own public handles, one directory over, applied here to the
// FOURTH handle this library freezes.
//
// POINTERS TO THE THREE FOREIGN IMPLS, NEVER OWNERSHIP (D-W6b-55): a
// gltfx_loop does not own the display/window/context it was opened
// over - open() below (loop_facade.cpp) merely borrows each one's own
// opaque impl through the SAME passkey idiom display_internal_access/
// window_internal_access/gl_context_internal_access already establish
// one directory over, and this struct's destructor (loop_facade.cpp's
// own gltfx_loop::~gltfx_loop()) never closes any of the three - only
// glintfx_loop's OWN scheduling state (frame_cap_schedule below) is
// this handle's to destroy.
//
// ONLY FORWARD-DECLARED TYPES REACHED HERE (display_impl, window_impl,
// gl_context_impl - each already opaque in its own public header,
// included above): this header stores POINTERS to all three, never a
// value member of any of them, so it never needs their real,
// platform-specific definitions (src/platform/window/display_impl.hpp,
// src/platform/window/window_impl.hpp, src/platform/gl/gl_context_
// impl.hpp) - those three definitions, and the real display/window/gl
// backends they in turn #include, are needed ONLY by loop_facade.cpp,
// the one translation unit that actually dereferences these pointers.
// This is DELIBERATE, not an oversight: it is what lets allocate_
// loop_impl() below (defined in this SAME file's own loop_impl.cpp)
// allocate and default-initialize a loop_impl WITHOUT ever touching a
// real Wayland connection, an EGL context, or a live HWND - see that
// atom's own header comment for why a test needs exactly this.
//
// previous_now/last_present/frame_index ARE THE BOOKKEEPING platform/
// loop/frame_tick_state.hpp's own compute_frame_tick() (a pure
// function, no member state of its own) is handed as plain arguments
// every step() - this struct is where that bookkeeping actually LIVES
// between calls, the same "the atom is pure, the caller owns the
// state" division of labor frame_cap_schedule below already
// establishes for a DIFFERENT budget (that class, unlike compute_
// frame_tick(), does carry its own tiny bit of state - m_has_deadline/
// m_next_deadline - because "the deadline advances from the PREVIOUS
// deadline, never from `now`" needs a place to remember the previous
// one across calls; loop_impl is that place for BOTH kinds of state at
// once, since a gltfx_loop only ever has ONE of each).
namespace glintfx {

struct loop_impl {
    // Borrowed, never owned (D-W6b-55) - open() (loop_facade.cpp)
    // fills these in via the three internal-access passkeys, and
    // gltfx_loop's own destructor never deletes any of the three.
    display_impl *display = nullptr;
    window_impl *window = nullptr;
    gl_context_impl *context = nullptr;

    // The ONE piece of state frame_cap_schedule.hpp's own class
    // carries (P7, D-W6b-49) - owned by this loop, reset to a fresh
    // schedule only when the loop itself is (re)opened.
    platform::frame_cap_schedule cap_schedule;

    // The previous tick's own gltfx_now() reading - compute_frame_
    // tick()'s own `previous_now` argument every step() (P5: elapsed
    // is measured from here). Zero-valued (the same "zero on the very
    // first tick" gltfx_frame_tick::elapsed already promises) until
    // the first real step() overwrites it.
    gltfx_time_point previous_now{};

    // What the last present() call actually returned - `presented`
    // before the very first one (the same default gltfx_frame_tick::
    // last_present already documents, platform/loop/loop.hpp), fed
    // straight into compute_frame_tick()'s own `last_present` argument
    // every step().
    gltfx_present_outcome last_present = gltfx_present_outcome::presented;

    // The previous tick's own frame_index - 0 before the very first
    // step() (P2: the first tick this loop ever produces reports 1,
    // this field plus one).
    std::uint64_t frame_index = 0;
};

// Allocates and default-initializes a fresh loop_impl ON THE HEAP,
// translating a failed allocation into gltfx_err_code::out_of_memory
// rather than letting std::bad_alloc escape a noexcept boundary - the
// SAME "aloca antes, abre dentro" FACADE-PIN shape gl_context_facade.
// cpp's own gltfx_gl_context::open() already uses for its own `new
// (std::nothrow) gl_context_impl{}` allocation, one directory over
// (this project's own git history, commit 4dc010d: a `noexcept`
// function letting `bad_alloc` escape used to call std::terminate()
// and take the consumer's whole process down with it - GODS_LAWS.md
// L-22/docs/api-conventions.md R3, "nunca aborta o processo do
// consumidor").
//
// EXTRACTED AS ITS OWN ATOM, RATHER THAN INLINED DIRECTLY INSIDE
// gltfx_loop::open() (loop_facade.cpp), FOR EXACTLY ONE REASON -
// TESTABILITY (docs/plano-w6b-fatias-6-8.md sec. 8.3, F17: "a prova e'
// a mesma FORMA de gfx_open_only_fixation_test"): gltfx_loop::open()
// itself cannot be armed against a forced allocation failure in a
// test that runs on all five systems without a live display/window/
// context (a genuinely-open gltfx_display/gltfx_window/gltfx_gl_
// context can ONLY be produced by their own real, platform-touching
// open() calls - there is no public or internal seam that fabricates
// one). This function needs none of that: since loop_impl (above)
// stores only POINTERS to the three foreign impls, allocating and
// default-initializing one touches no operating system at all - the
// SAME "pure enough to test directly" property src/platform/gl/gfx_
// open_only_fixation.hpp's own resolve_gfx_open_only_fixation()
// already has, one fatia over, for the identical reason (that
// function's own header comment: it decides BEFORE either adapter is
// ever consulted). tests/loop_open_alloc_failure_test.cpp is the
// proof: it recompiles THIS file's own .cpp straight into the test
// binary (no GLINTFX_API on the declaration below, no DLL boundary to
// cross - src/platform/loop/CMakeLists.txt's own target_sources()
// comment on loop_impl.cpp), arms a TU-local operator new
// override around exactly one call to allocate_loop_impl(), and
// asserts the returned gltfx_rslt<loop_impl *> holds out_of_memory,
// never a crash.
[[nodiscard]] gltfx_rslt<loop_impl *> allocate_loop_impl() noexcept;

} // namespace glintfx
