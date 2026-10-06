// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <vector>

// window_capture_rules.hpp - QA-SCREEN-CAPTURE C2b-1 (D-W8-36, D-W8-37, D-W8-40 of
// /var/tmp/cto-w8/decisao-c2b.md): the PURE decisions of the Windows window-capture tool
// (tests/tools/win32_window_capture.cpp). Test tooling, not product code. No <windows.h> and no
// system call in here: the tool collects the facts (how many windows the child owns, where the
// client area is, who answered WindowFromPoint) and these functions turn facts into a verdict,
// so the rules run and are proven on EVERY system, not only where the tool can run.
//
// Every verdict carries a text the tool prints as is: a rejection names the cause (`janelas=0`,
// `janelas=2`, `FORA DA TELA`, `OCLUIDA por classe=<x>`), never a bare "wrong color" (the window
// that never appeared, the one that left the screen and the one covered by another must stay
// distinguishable from a wrong pixel).

namespace glintfx::capture_tool {

struct verdict {
    bool pass = false;
    std::string text;
};

// A rectangle in virtual-screen pixels, right/bottom EXCLUSIVE (the RECT convention).
struct pixel_rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct screen_point {
    int x = 0;
    int y = 0;
};

// What WindowFromPoint (reduced to its root by the tool) answered at one point.
struct occlusion_probe {
    screen_point point;
    bool owned_by_target = false;
    std::string owner_class;
};

// Exactly ONE top-level window of the launched process must carry the title: zero and several
// both reject, and both print how many were found.
[[nodiscard]] verdict judge_window_count(std::size_t found);

// The whole client area must lie inside the virtual screen (and not be empty).
[[nodiscard]] verdict judge_client_on_screen(const pixel_rect &client,
                                             const pixel_rect &virtual_screen);

// The five points the occlusion guard asks about: the four corners and the center of the client
// area (right/bottom exclusive, so the last pixel is `right - 1`, `bottom - 1`).
[[nodiscard]] std::vector<screen_point> occlusion_points(const pixel_rect &client);

// Passes only when exactly five probes came back and all are the target window's own.
// KNOWN GAP, declared (D-W8-36 chose five points): a window that covers the client area WITHOUT
// touching any of the five points (a notification bar along the middle of an edge, a small popup
// over the scene) passes this guard, and the screen read then shows up as a wrong color, never as
// OCLUIDA. Also by the API's documentation (inference, not measured): WindowFromPoint skips
// disabled and WS_EX_TRANSPARENT windows while BitBlt with CAPTUREBLT captures them, so a cover of
// those classes escapes the guard too.
[[nodiscard]] verdict judge_occlusion(const std::vector<occlusion_probe> &probes);

// The .meta text of a capture the tool writes: XRGB8888 (wl_shm format 1; the GDI DIB carries
// no useful alpha), rows tightly packed (stride = width * 4). Same four keys, same order and same
// decimal-integer grammar tests/tools/raw_to_png.py's parse_meta reads.
[[nodiscard]] std::string capture_meta_text(int width, int height);

} // namespace glintfx::capture_tool
