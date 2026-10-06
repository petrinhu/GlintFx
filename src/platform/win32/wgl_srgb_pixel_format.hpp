// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/extension_token.hpp"

// platform/win32/wgl_srgb_pixel_format.hpp - D-SRGB-2 (D-SRGB2-9, D-A62): what the Windows side
// knows about the sRGB framebuffer WITHOUT touching a Windows header, so it compiles and runs under
// the same unit test on every system (the precedent is power_status_rule.hpp). The adapter
// (wgl_context_adapter.cpp) and the extension loader (wgl_extension_loader.cpp) both read it, so
// there is one copy of the rule.
//
// D-A62: the sRGB framebuffer is a fact the driver CONFIRMS on the pixel format that will be bound,
// never a fact read off the choose and never inferred from COLOR_ENCODING.

namespace glintfx::platform {

// WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, the attribute of wglGetPixelFormatAttribivARB (and of the
// choose list) that says whether a pixel format is sRGB capable. The value is from the
// specification (WGL_ARB_framebuffer_sRGB, "New Tokens"):
//   https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_framebuffer_sRGB.txt
// The Windows SDK has no wglext.h and the project does not vendor wgl.xml, so no header can be the
// oracle of this literal: the unit test pins it against the specification text (D-SRGB2-10).
inline constexpr int k_wgl_framebuffer_srgb_capable_arb = 0x20A9;

// Whether the WGL extension list announces the sRGB framebuffer: WGL_ARB_framebuffer_sRGB or its
// older twin WGL_EXT_framebuffer_sRGB, by the WHOLE token (extension_token.hpp). A null list
// announces nothing.
[[nodiscard]] inline bool wgl_framebuffer_srgb_advertised(const char *list) noexcept {
    return extension_token_listed(list, "WGL_ARB_framebuffer_sRGB") ||
           extension_token_listed(list, "WGL_EXT_framebuffer_sRGB");
}

// The verdict of the query wglGetPixelFormatAttribivARB(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB) on the
// pixel format that will be bound (D-A62): only a call that SUCCEEDED and answered TRUE confirms
// the sRGB framebuffer. A failed call, whatever it left in `value`, confirms nothing; and so does
// any answer other than TRUE (the Windows TRUE is 1; the literal keeps this header free of
// <windows.h>).
[[nodiscard]] constexpr bool wgl_srgb_confirmed(bool query_ok, int value) noexcept {
    return query_ok && value == 1;
}

} // namespace glintfx::platform
