// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/color.hpp>

// draw2d/frame_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::begin_frame() needs.
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header).
//
// WHY THE FRAME IS CLEARED BY DEFAULT: after a buffer swap, what is left
// in the buffer you draw into is undefined unless the
// surface was created to preserve it (the EGL reference for
// eglSwapBuffers says so in those words). A frame that does not clear
// shows whatever the driver left there. Leaving it out is a choice you
// make on purpose, by writing std::nullopt.

namespace glintfx {

struct gltfx_frame_2d_desc {
    // The color the whole surface is filled with before anything is
    // drawn in this frame, or std::nullopt to keep what is there.
    // Linear light, straight alpha, like every gltfx_rgba. Default:
    // opaque black.
    std::optional<gltfx_rgba> clear_color = gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F};
};

} // namespace glintfx
