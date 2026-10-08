// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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
    int code =
        0; // the tool's exit code for this verdict (0 when it passes); see k_verdict_code_table
};

// THE TABLE OF EXIT CODES, one per verdict (D-W8-45), the single source: the driver
// (tests/tools/run_capture_known_color_win32.py) holds a copy that check_sibling_lists.py keeps
// equal, token by token. The codes avoid the ones already in use: 0 ok, 1 fixture, 2 tool, 77
// declared absence, 124 timeout and 127 impossible run. The name has no spaces (the gate splits on
// them); the `veredito:` line carries the same name with spaces ("janelas=<n>" for JANELAS).

// #  GLINTFX-SIBLING-LIST:window-capture-verdict-codes:START
inline constexpr std::array<std::string_view, 6> k_verdict_code_table{
    "10=JANELAS",      "11=INVISIVEL", "12=ICONICA",
    "13=FORA_DA_TELA", "14=OCLUIDA",   "15=CAPTURA_RECUSADA"};
// #  GLINTFX-SIBLING-LIST:window-capture-verdict-codes:END

// The code of a verdict NAME as written in the table; -1 when the name is not in it.
[[nodiscard]] int verdict_code(std::string_view name);

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

// Passes only when exactly five probes came back and all are the target window's own. A different
// count rejects as OCLUIDA (code 14, text "OCLUIDA pontos=<n>"): it cannot happen by construction,
// but a rejection never leaves with code 0.
// KNOWN GAP, declared (D-W8-36 chose five points): a window that covers the client area WITHOUT
// touching any of the five points (a notification bar along the middle of an edge, a small popup
// over the scene) passes this guard, and the screen read then shows up as a wrong color, never as
// OCLUIDA. Also by the API's documentation (inference, not measured): WindowFromPoint skips
// disabled and WS_EX_TRANSPARENT windows while BitBlt with CAPTUREBLT captures them, so a cover of
// those classes escapes the guard too.
[[nodiscard]] verdict judge_occlusion(const std::vector<occlusion_probe> &probes);

// What the tool read about the window itself (IsWindowVisible, IsIconic), before any pixel.
struct window_facts {
    bool visible = false;
    bool iconic = false;
};

// THE ORDER OF THE VERDICTS (D-W8-43; the first that matches decides, so a cause is never
// reported as a later, vaguer one): (1) window count, judged by judge_window_count; (2) INVISIVEL
// (the window is not visible: the library never showed it); (3) ICONICA (minimized); (4) FORA DA
// TELA; (5) OCLUIDA; (6) the bytes, which are the driver's. This function is steps 2 to 5. A
// verdict that rejects here is a CAPTURE verdict: the screen read would measure nothing.
[[nodiscard]] verdict judge_capture_readiness(const window_facts &facts, const pixel_rect &client,
                                              const pixel_rect &virtual_screen,
                                              const std::vector<occlusion_probe> &probes);

// D-W8-44: the readiness is decided WHOLE before any capture, and a verdict that rejects is
// terminal: nothing after it can change it. A window that is not ready never reaches PrintWindow
// or BitBlt (the PrintWindow reading of a never-shown window, I-2, is deliberately NOT measured).
struct capture_plan {
    bool printwindow = false;
    bool bitblt = false;
};

[[nodiscard]] capture_plan plan_captures(const verdict &readiness);

// A READY window whose PrintWindow or BitBlt answered zero is a fact measured on the system (it may
// be a presentation defect), not a dead tool: it is the verdict CAPTURA RECUSADA, naming the
// mechanism and the GetLastError. The tool's own failure (bad arguments, launch, file writing) is
// something else and keeps its own exit code.
[[nodiscard]] verdict judge_capture_refused(std::string_view mechanism, unsigned long error);

// The line the tool prints for the `index`-th DwmFlush. `result` is empty when dwmapi.dll or the
// DwmFlush export is not there (it is resolved at run time, never linked): the capture goes on,
// and the line says so, because a screen read taken before composition would be a false red and
// must be readable as such. A failing HRESULT (negative) is named too; it is never judged here.
[[nodiscard]] std::string describe_dwm_flush(int index, std::optional<long> result);

// What the tool did to the fixture's window before it waited for the fixture to exit (I-3 of the
// C2b-5 review). The tool decides it from what it SAW (whether the fixture presented, whether one
// window was identified, what PostMessageW answered), and the text names exactly that state.
enum class close_attempt {
    fixture_not_presented, // the fixture never presented: the window was never even looked for
    window_not_identified, // the fixture presented, but no single window of it was identified
    close_not_delivered,   // a window was identified, PostMessageW refused the WM_CLOSE
    close_posted,          // a window was identified and PostMessageW accepted the WM_CLOSE
};

// The line the tool prints when the fixture did not exit by itself within the exit budget and was
// terminated. It claims a WM_CLOSE only for `close_posted`; every other state says why none was
// delivered, and never says a window was not found when it was never looked for.
[[nodiscard]] std::string describe_fixture_overstay(close_attempt attempt, int budget_ms);

// The .meta text of a capture the tool writes: XRGB8888 (wl_shm format 1; the GDI DIB carries
// no useful alpha), rows tightly packed (stride = width * 4). Same four keys, same order and same
// decimal-integer grammar tests/tools/raw_to_png.py's parse_meta reads.
[[nodiscard]] std::string capture_meta_text(int width, int height);

} // namespace glintfx::capture_tool
