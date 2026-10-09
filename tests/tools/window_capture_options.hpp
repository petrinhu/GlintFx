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
[[nodiscard]] bool parse_tool_options(const std::vector<std::string> &args, tool_options &options);

} // namespace glintfx::capture_tool
