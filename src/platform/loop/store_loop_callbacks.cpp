// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/loop/store_loop_callbacks.hpp"

#include <glintfx/core/err_code.hpp>

#include "platform/loop/loop_callbacks_validation.hpp"
#include "platform/loop/owned_loop_context.hpp"

// store_loop_callbacks.cpp - LOOP-CONTEXT-OWNERSHIP (S1b, /var/tmp/
// glintfx-plan/loop-fix.md sec. 3.2/S1b) - see this file's own header
// comment (store_loop_callbacks.hpp) for what this solves and does
// not.

namespace glintfx::platform {

gltfx_rslt<void> store_loop_callbacks(loop_impl &impl, gltfx_loop_callbacks callbacks) noexcept {
    // D-LF-6d: a set_callbacks() called from INSIDE on_frame/on_render
    // of a run() ALREADY IN PROGRESS on this same loop is refused BY
    // NAME, before validate_loop_callbacks() ever runs and before
    // `callbacks` is stored - platform::loop_run's own running_guard
    // (loop_engine.hpp) is what armed `impl.book.running` in the first
    // place; this function only ever READS it, never arms or disarms
    // it itself (running_guard.hpp's own header comment). Without this
    // check, a re-entrant call here would destroy the very context the
    // outer run() is still calling into (loop.hpp's own header comment
    // on set_callbacks()).
    if (impl.book.running) {
        // The pair THIS call was just handed is destroyed; whatever is
        // already stored is left completely untouched (D-LF-6e's own
        // "recusado = nada substituído").
        owned_loop_context rejected{callbacks.context, callbacks.destroy_context};
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("running"));
    }

    if (const gltfx_rslt<void> validated = validate_loop_callbacks(callbacks);
        validated.has_error()) {
        owned_loop_context rejected{callbacks.context, callbacks.destroy_context};
        return validated;
    }

    // Accepted: store the struct, then hand the (context, destroy)
    // pair to the guarded atom - reset() destroys whatever was
    // PREVIOUSLY stored only AFTER this new pair is already in place
    // (owned_loop_context::reset()'s own header comment).
    impl.book.stored_callbacks = callbacks;
    impl.book.stored_context.reset(callbacks.context, callbacks.destroy_context);
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
