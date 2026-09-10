// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/loop/loop_book.hpp"

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
// `book` IS THE BOOKKEEPING loop_engine.hpp's own loop_step()/loop_
// present() read and write between calls (LOOP-RUN cobertura, S2,
// /var/tmp/glintfx-plan/loop-fix.md sec. S2.2) - previously four
// separate fields directly on this struct (cap_schedule/previous_now/
// last_present/frame_index), now grouped under platform::loop_book
// (platform/loop/loop_book.hpp's own header comment has the full
// reasoning for the move). loop_impl still owns the STORAGE; only the
// grouping changed.
namespace glintfx {

struct loop_impl {
    // Borrowed, never owned (D-W6b-55) - open() (loop_facade.cpp)
    // fills these in via the three internal-access passkeys, and
    // gltfx_loop's own destructor never deletes any of the three.
    display_impl *display = nullptr;
    window_impl *window = nullptr;
    gl_context_impl *context = nullptr;

    // The engine's own memory between calls - see platform::loop_book's
    // own header comment (platform/loop/loop_book.hpp) for what each
    // field is and why it lives here.
    platform::loop_book book;
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
