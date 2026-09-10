// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/loop/loop_callbacks_validation.hpp"

#include <glintfx/core/err_code.hpp>

namespace glintfx::platform {

gltfx_rslt<void> validate_loop_callbacks(const gltfx_loop_callbacks &callbacks) noexcept {
    if (!callbacks.on_frame) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("on_frame"));
    }
    if (!callbacks.on_render) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("on_render"));
    }
    if (callbacks.on_event) {
        // on_event is RESERVED (D-W6b-24/D-W6b-43, platform/loop/
        // loop.hpp's own header comment on gltfx_loop_callbacks::
        // on_event): the field already exists in the frozen struct's
        // own layout so INPUT-EVENTS (W7) can wire it in as a
        // compatible addition later, but THIS version of run() refuses
        // a caller that already filled it in - never silently drops a
        // callback nobody will ever invoke.
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("on_event"));
    }
    if (callbacks.destroy_context && !callbacks.context) {
        // LOOP-CONTEXT-OWNERSHIP (S1b), D-LF-7: a destroy function with
        // NO context to destroy is refused by name - there is nothing
        // for it to ever be called with, so accepting it would freeze
        // in a caller's mind a promise ("this will be destroyed") this
        // library can never keep. `destroy_context` WITH a `context` is
        // no longer refused here (S1a used to refuse it unconditionally
        // - platform::owned_loop_context, owned_loop_context.hpp, and
        // store_loop_callbacks.hpp/.cpp are what honor it now, on
        // whichever of the two forms the caller used, loop.hpp's own
        // header comment on gltfx_loop_callbacks::destroy_context).
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("destroy_context"));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
