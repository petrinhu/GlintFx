// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>

// platform/gl/gl_surface_size_policy.hpp - GL-CONTEXT (docs/plano-w6b-
// placa-e-laco.md fatia 2b, D-W6b-5, GODS_LAWS.md L-04/L-17): the ONE
// pure atom that decides "does the EGL surface need resizing before
// the next frame" - the answer to D-W6b-5's own question, "quem
// redimensiona o buffer quando pixel_size() muda". The Wayland
// adapter (fatia 3) calls this once per swap_buffers(), comparing the
// window's own `pixel_size()` (glintfx::gltfx_window, already public)
// against whatever size it last gave `wl_egl_window_resize()` - never
// the consumer's job, never a callback wired into the window (D-W6b-1
// already refused "the context lives inside the window", D-W6b-5's
// own option (c) refused the mirror image, "the window calls back into
// the context").
//
// A PURE COMPARISON OF FOUR INTEGERS, NOTHING MORE: this atom does not
// know what a wl_egl_window or an HWND is - it takes the window's
// current pixel size and whatever size the surface was LAST resized
// to, and answers whether a resize is due and, if so, to what. The
// Win32 side (fatia 4) has nothing to do here (D-W6b-5's own "no Win32
// nao ha o que fazer: o DC segue o HWND") - this atom is Wayland's own
// tool, kept here (not under src/platform/wayland/) only because it is
// OS-agnostic arithmetic that testing does not need a compositor to
// exercise (the same reasoning gfx_open_only_fixation.hpp - a Wayland-
// motivated rule, tested with plain data - already established).
//
// ZERO IS NEVER A VALID PIXEL SIZE HERE (mirrors window_desc_
// validation.hpp's own "both dimensions must be non-zero" rule, one
// layer up): a 0x0 window is refused at gltfx_window::open() itself
// (WINDOW-SIZE-REFUSE), long before any gl context exists to ask this
// question - `resolve_gl_surface_size()` REFUSES a 0x0 `window_pixel_
// width`/`window_pixel_height` with gltfx_err_code::invalid_argument,
// rejected_value() == "pixel_size", as a live assertion that the zero
// case genuinely never reaches here, not a defensive guess.

namespace glintfx::platform {

struct gl_surface_size_decision {
    bool should_resize = false;
    // Meaningful iff should_resize is true - the size to resize the
    // surface TO (always the window's own current pixel size).
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

[[nodiscard]] gltfx_rslt<gl_surface_size_decision>
resolve_gl_surface_size(std::uint32_t window_pixel_width, std::uint32_t window_pixel_height,
                        std::uint32_t current_buffer_width,
                        std::uint32_t current_buffer_height) noexcept;

} // namespace glintfx::platform
