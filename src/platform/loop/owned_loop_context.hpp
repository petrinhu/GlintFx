// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/platform/loop/loop.hpp>

// platform/loop/owned_loop_context.hpp - LOOP-CONTEXT-OWNERSHIP (S1b,
// /var/tmp/glintfx-plan/loop-fix.md sec. S1b/3.2, GODS_LAWS.md
// L-17/L-19/L-22): the internal RAII atom BOTH forms of posse
// (gltfx_loop::run(callbacks) and set_callbacks()+run(), loop.hpp's
// own header comment on gltfx_loop_callbacks::destroy_context) are
// built on top of.
//
// WHAT THIS SOLVES: a context handed over through the frozen struct
// (destroy_context non-null) is destroyed EXACTLY ONCE, whatever
// happens between construction and destruction of this atom - no
// double-destroy, no leak, on any path (loop_engine.hpp's own
// loop_run() relies on this for every one of its return paths,
// tests/loop_engine_test.cpp's own T13; store_loop_callbacks.hpp's own
// store_loop_callbacks() relies on it for substitution and for
// loop_impl's own destructor, tests/store_loop_callbacks_test.cpp).
// destroy_context nullptr means BORROWED (`context` stays the
// consumer's, exactly as before this sub-fatia) - this atom's own
// destructor is then a no-op, forever.
//
// WHAT THIS DOES NOT SOLVE: WHEN this atom's own destructor runs is a
// decision made entirely by whoever constructs and holds it, never by
// this file - form 1 (gltfx_loop::run(callbacks), loop_facade.cpp)
// builds one on ITS OWN stack, so it dies with that one call; form 2
// (platform::loop_book::stored_context, store_loop_callbacks.hpp/.cpp)
// lives inside the loop's own bookkeeping, so it dies only when
// replaced by a later successful set_callbacks(), or when the loop
// itself is destroyed. This file has no opinion on which - see
// loop.hpp's own (b0)/(b1)/(b2) truths for what each form promises the
// consumer.
//
// NON-COPYABLE, NON-MOVABLE: exactly one atom ever owns a given
// (context, destroy) pair - a copy or a move would create a SECOND
// atom that could call destroy_context a second time on the SAME
// pointer, the double-free this atom exists to make unrepresentable
// (the same reasoning platform::display_connection, src/platform/
// port/display_connection.hpp's own header comment, already gives for
// itself). reset() below is the only way this atom's own (context,
// destroy) pair ever changes after construction.
namespace glintfx::platform {

class owned_loop_context {
  public:
    owned_loop_context() noexcept = default;
    owned_loop_context(void *context, gltfx_loop_context_destroy_fn destroy) noexcept
        : m_context(context), m_destroy(destroy) {}

    owned_loop_context(const owned_loop_context &) = delete;
    owned_loop_context &operator=(const owned_loop_context &) = delete;
    owned_loop_context(owned_loop_context &&) = delete;
    owned_loop_context &operator=(owned_loop_context &&) = delete;

    // Destroys whatever this atom still holds - a no-op when either
    // half of the pair is null (borrowed, or already reset to empty).
    ~owned_loop_context() noexcept {
        if (m_destroy != nullptr && m_context != nullptr) {
            m_destroy(m_context);
        }
    }

    // D-LF-6e: replaces this atom's own (context, destroy) pair - the
    // PREVIOUS pair is destroyed AFTER the new one is already stored
    // (tests/owned_loop_context_test.cpp's own "reset destroys the
    // previous exactly once, and after the new one is stored" case),
    // so a destroy_context that somehow re-entered this atom would
    // always see the NEW pair in place, never a half-updated one.
    // store_loop_callbacks.hpp's own store_loop_callbacks() is the one
    // caller that matters here: substituting the guarded callbacks
    // must never leave a window where neither the old nor the new
    // context is the one this atom would destroy.
    void reset(void *context, gltfx_loop_context_destroy_fn destroy) noexcept {
        void *old_context = m_context;
        gltfx_loop_context_destroy_fn old_destroy = m_destroy;
        m_context = context;
        m_destroy = destroy;
        if (old_destroy != nullptr && old_context != nullptr) {
            old_destroy(old_context);
        }
    }

  private:
    void *m_context = nullptr;
    gltfx_loop_context_destroy_fn m_destroy = nullptr;
};

} // namespace glintfx::platform
