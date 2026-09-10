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
    if (callbacks.destroy_context) {
        // LOOP-CONTEXT-OWNERSHIP has not landed yet (platform/loop/
        // loop.hpp's own header comment on gltfx_loop_callbacks::
        // destroy_context) - refused BY NAME rather than accepted and
        // silently never invoked, the same discipline the on_event
        // check right above already applies to its own reserved
        // field. This whole check is REMOVED when that fatia lands and
        // teaches run() to actually call destroy_context.
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("destroy_context"));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
