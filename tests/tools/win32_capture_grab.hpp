// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "window_capture_rules.hpp"

// win32_capture_grab.hpp - QA-SCREEN-CAPTURE C2b-2 (D-W8-36): the two ways the capture tool reads
// a window's pixels, and the facts the pure rules (window_capture_rules.hpp) judge. Test tooling,
// not product code. Windows only; no <windows.h> here, `window` is the HWND as an opaque pointer.
//
//   PrintWindow(PW_RENDERFULLCONTENT) reads the window's OWN surface: it proves the content.
//   BitBlt of the screen DC with CAPTUREBLT reads what the screen shows: it proves the frame
//   reached the screen. (The first does not depend on position, occlusion or focus; the second
//   is the one that sees "the window never appeared".)

namespace glintfx::capture_tool {

// The client area, tightly packed: B,G,R,X per pixel (the GDI DIB layout), top row first. The X
// byte carries no usable alpha, which is why the capture is written as XRGB8888 (format 1).
struct captured_image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> bgrx;
};

struct grab_result {
    bool ok = false;
    std::string detail; // empty when ok; otherwise the API that failed and its GetLastError
};

[[nodiscard]] pixel_rect client_rect_on_screen(void *window);
[[nodiscard]] pixel_rect virtual_screen_rect();

// Asks WindowFromPoint (reduced to its top-level window) about the five occlusion points.
[[nodiscard]] std::vector<occlusion_probe> probe_occlusion(void *window, const pixel_rect &client);

// PrintWindow over the whole window (no PW_CLIENTONLY, so PW_RENDERFULLCONTENT applies as in the
// common use), then the client area is cropped out.
[[nodiscard]] grab_result grab_with_printwindow(void *window, captured_image &image);

// BitBlt (SRCCOPY | CAPTUREBLT) of `client` from the screen.
[[nodiscard]] grab_result grab_with_bitblt(const pixel_rect &client, captured_image &image);

// <directory>/conn1_surface1.raw and .meta (format 1), the names tests/tools/capture_vs_readback.py
// looks for. Creates the directory.
[[nodiscard]] grab_result write_capture_pair(const std::string &directory,
                                             const captured_image &image);

} // namespace glintfx::capture_tool
