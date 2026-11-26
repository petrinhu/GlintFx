// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

// win32_capture_child.hpp - QA-SCREEN-CAPTURE C2b-2 (D-W8-37): the child process the capture tool
// launches itself (the fixture). Test tooling, not product code. Windows only; no <windows.h> here,
// the handles are opaque.
//
// The child is created WITHOUT STARTF_USESHOWWINDOW: its first window is shown (or not) by what the
// LIBRARY does, never by what the launcher asked (the ctest launcher asks for the default show
// state, and a tool that forced a show state would fix the very defect the capture exists to
// measure). Standard output and error go to a pipe the tool reads; standard input is NUL.

namespace glintfx::capture_tool {

struct child_process {
    void *process = nullptr;
    void *output_read = nullptr;
    unsigned long id = 0;
};

enum class wait_outcome { found, child_exited, timed_out };

// argv[0] is the executable path. False (and a message on stderr) when the child could not start.
[[nodiscard]] bool launch_child(const std::vector<std::string> &argv, child_process &child);

// Appends what the child wrote so far to `captured` (and echoes it on stdout); never blocks.
void drain_output(const child_process &child, std::string &captured);

// Polls until `needle` shows up in the child's output, the child exits, or `budget_ms` runs out.
[[nodiscard]] wait_outcome wait_for_output(const child_process &child, std::string_view needle,
                                           int budget_ms, std::string &captured);

// True when the child exited within `budget_ms`; `exit_code` is then its exit code.
[[nodiscard]] bool wait_for_exit(const child_process &child, int budget_ms,
                                 unsigned long &exit_code);

void terminate_child(const child_process &child);
void close_child(child_process &child);

} // namespace glintfx::capture_tool
