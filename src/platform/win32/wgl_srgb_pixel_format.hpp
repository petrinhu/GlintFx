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

// Whether the WGL extension list announces the sRGB framebuffer: WGL_ARB_framebuffer_sRGB or its
// older twin WGL_EXT_framebuffer_sRGB, by the WHOLE token (extension_token.hpp). A null list
// announces nothing.
[[nodiscard]] inline bool wgl_framebuffer_srgb_advertised(const char *list) noexcept {
    return extension_token_listed(list, "WGL_ARB_framebuffer_sRGB") ||
           extension_token_listed(list, "WGL_EXT_framebuffer_sRGB");
}

} // namespace glintfx::platform
