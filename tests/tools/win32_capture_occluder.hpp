// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "window_capture_rules.hpp"

// win32_capture_occluder.hpp - QA-SCREEN-CAPTURE C2b-4 (D-W8-42 of /var/tmp/cto-w8/decisao-c2b.md):
// the sabotage `occlude` of the capture tool, the debut proof that the occlusion guard bites on the
// real system (the pure rule is proven in window_capture_rules_test; WindowFromPoint is not). Test
// tooling, not product code. Windows only; no <windows.h> here, the HWND is an opaque pointer.
//
// WHAT IT IS: a top-level window OF THE TOOL, opaque, painted one solid color that is none of the
// scene's, placed over a screen area (the fixture's client area) and shown WITHOUT activating. It
// is TOPMOST and NOACTIVATE, enabled, and never WS_EX_TRANSPARENT: WindowFromPoint skips disabled
// and transparent windows, so a cover of those classes would escape the guard (the KNOWN GAP
// declared in window_capture_rules.hpp) and the sabotage would prove nothing. The guard compares
// window roots, never processes, so a window of the tool is as foreign to the fixture as a third
// party's.
//
// WHAT THE DEBUT MUST SHOW (fixed before the data): the verdict `OCLUIDA por
// classe=<k_occluder_class>` (exit 14), and that line wins over the `differing>0` the BitBlt would
// also give because the cover's color is not the scene's. The sabotage only ever produces a
// rejection, and only when asked for by the explicit option `--sabotage-occlude`.

namespace glintfx::capture_tool {

// The window class of the cover: what `OCLUIDA por classe=` names when the guard meets it.
inline constexpr std::string_view k_occluder_class = "GlintFxCaptureOccluder";

class window_occluder {
  public:
    window_occluder() = default;
    window_occluder(const window_occluder &) = delete;
    window_occluder &operator=(const window_occluder &) = delete;
    ~window_occluder();

    // Creates and shows the cover over `area`, then dispatches the tool thread's messages until the
    // window has painted (a window only paints when its own thread pumps). False when the system
    // refused any step; nothing is left behind in that case.
    [[nodiscard]] bool cover(const pixel_rect &area);

  private:
    void *m_window = nullptr;
    bool m_class_registered = false;
};

} // namespace glintfx::capture_tool
