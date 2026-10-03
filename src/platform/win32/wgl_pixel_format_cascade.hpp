// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <span>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_format_decision.hpp"

// platform/win32/wgl_pixel_format_cascade.hpp - D-SRGB-2 (D-SRGB2-2/3, S5): the cascade that picks
// the pixel format of the window, one atom per step, the Win32 mirror of the choose/decision half
// of egl_context_adapter.cpp. It was born inside wgl_context_adapter.cpp and moved here without any
// change of behavior (L-17: one subject per file; the adapter keeps the GPU and the context).
// The order of the refusals is gfx_format_decision.hpp's, the SAME the EGL adapter calls (L-04).
//
// wglChoosePixelFormatARB has no fixed address: the caller resolves it through
// wgl_extension_loader.hpp and hands the pointer in as this type.

namespace glintfx::platform {

using wgl_choose_pixel_format_arb_fn = BOOL(WINAPI *)(HDC, const int *, const FLOAT *, UINT, int *,
                                                      UINT *);

// What the options ask of the pixel format (D-W6b-4: RGBA8 + stencil8, no depth, plus these two).
struct wgl_format_request {
    std::int64_t msaa_samples = 0;
    bool srgb = false;
};

// What the choose answered (D-SRGB2-2): the format to bind, whether one with EVERYTHING asked
// exists, whether one WITHOUT the sRGB exists (only to NAME the refused option, never to open with
// less), and the OS error of the failed choose.
struct wgl_format_choice {
    int format = 0;
    bool found = false;
    bool found_without_srgb = false;
    DWORD os_error = 0;
};

// What the options ask of the pixel format.
[[nodiscard]] wgl_format_request
read_wgl_format_request(std::span<const gltfx_gfx_option_entry> options) noexcept;

// D-SRGB2-2 steps 1 and 2 (choose, and the second choose that only names the culprit).
[[nodiscard]] wgl_format_choice choose_wgl_formats(HDC dc, wgl_choose_pixel_format_arb_fn fn,
                                                   const wgl_format_request &request,
                                                   bool srgb_advertised) noexcept;

// D-SRGB2-2 step 3 (D-A62): only a successful query answering TRUE confirms.
[[nodiscard]] bool confirm_wgl_srgb_capable(HDC dc, void *get_pixel_format_attribiv_arb,
                                            int format) noexcept;

// The refusal the neutral decision chose, as the error open() returns.
[[nodiscard]] gltfx_rslt<void> wgl_refusal_result(gfx_format_refusal refusal,
                                                  DWORD os_error) noexcept;

// The facts the neutral decision reads.
[[nodiscard]] gfx_format_facts make_wgl_format_facts(const wgl_format_request &request,
                                                     const wgl_format_choice &choice,
                                                     bool srgb_advertised,
                                                     bool srgb_confirmed) noexcept;

// D-SRGB2-2 step 4: the ONE SetPixelFormat the window will ever get; it comes LAST.
[[nodiscard]] gltfx_rslt<void> bind_wgl_pixel_format(HDC dc, int format) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
