// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/platform/loop/loop.hpp>

#include "platform/loop/loop_impl.hpp"

// platform/loop/store_loop_callbacks.hpp - LOOP-CONTEXT-OWNERSHIP
// (S1b, /var/tmp/glintfx-plan/loop-fix.md sec. 3.2/S1b, D-LF-6a/6b/6e):
// the ONE place gltfx_loop::set_callbacks() (loop_facade.cpp) stores a
// gltfx_loop_callbacks for FORM 2 (posse pelo laço, loop.hpp's own
// (b2)) - the same "validated once, common to every caller" shape
// platform::validate_loop_callbacks (loop_callbacks_validation.hpp)
// already gives run()'s own FORM 1 validation, one file over.
//
// WHAT THIS SOLVES: `impl.book.stored_callbacks`/`impl.book.
// stored_context` (loop_book.hpp) are updated TOGETHER, and the
// previously-stored context is destroyed AFTER the new one is already
// in place (owned_loop_context::reset()'s own header comment) - never
// a half-updated pair a re-entrant read could observe. A refusal (the
// loop is re-entrant right now, or `callbacks` itself fails
// validate_loop_callbacks) destroys ONLY what THIS call was just
// handed, and leaves whatever was already stored completely untouched
// (tests/store_loop_callbacks_test.cpp).
//
// WHAT THIS DOES NOT SOLVE: it never runs a tick, never calls
// on_frame/on_render, and never decides when `stored_callbacks` is
// actually USED - that is platform::loop_run's own job
// (loop_engine.hpp), called later, separately, by gltfx_loop::run()
// (no arguments) with an EMPTY owned_loop_context (loop_facade.cpp's
// own header comment on that overload) because ownership already
// lives here, in `impl.book`, not in that call's own stack.
namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void> store_loop_callbacks(loop_impl &impl,
                                                    gltfx_loop_callbacks callbacks) noexcept;

} // namespace glintfx::platform
