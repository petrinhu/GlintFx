// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

// window_capture_options.hpp - QA-SCREEN-CAPTURE D5a (D-W8-113, DEMO-1): the command line of the
// Windows window-capture tool (tests/tools/win32_window_capture.cpp) as a pure unit. Test tooling,
// not product code. It names what the tool accepts and turns the argument list into a
// tool_options; it makes no system call and includes no <windows.h>, so it is proven on every
// system.
//
// The ready line is the text the fixture prints when its first frame is presented. The tool waits
// for it before it reads any pixel. The default is the line the fixture has always printed; the
// driver can name another one with --ready-line.

namespace glintfx::capture_tool {

// The line tests/parity/capture_known_color_smoke.cpp prints when it presents (its `presented at
// attempt` line). The two are kept equal by the real capture: if they drift, the tool never finds
// the line and capture_known_color_smoke fails on every Windows job (D-W8-123).
inline constexpr std::string_view k_default_ready_line = "presented at attempt";
inline constexpr int k_default_present_budget_ms = 30000;
inline constexpr int k_default_exit_budget_ms = 10000;

struct tool_options {
    std::string title;
    std::string out_directory;
    int present_budget_ms = k_default_present_budget_ms;
    int exit_budget_ms = k_default_exit_budget_ms;
    bool sabotage_occlude = false;
    std::string ready_line{k_default_ready_line};
    std::vector<std::string> fixture_command;
};

// True when the arguments before `--` name a title, an output directory and budgets in range, and
// after `--` name a fixture command. Anything else is refused as a whole, never partly applied.
// An option's value is the next argument whatever it is, `--` included, as getopt reads it
// (D-W8-120); a repeated option keeps its last value (D-W8-121); a budget is decimal digits only,
// no sign, no space (D-W8-122).
[[nodiscard]] bool parse_tool_options(const std::vector<std::string> &args, tool_options &options);

} // namespace glintfx::capture_tool
