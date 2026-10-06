// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "win32_capture_child.hpp"
#include "win32_capture_grab.hpp"
#include "win32_capture_text.hpp"
#include "window_capture_rules.hpp"

// win32_window_capture.cpp - QA-SCREEN-CAPTURE C2b-2 (D-W8-36, D-W8-37, D-W8-40 of
// /var/tmp/cto-w8/decisao-c2b.md): the Windows capture tool. It LAUNCHES the fixture itself,
// finds the fixture's window by PID and title (counting: exactly one), reads the window's pixels
// by TWO mechanisms and writes one capture pair per mechanism, in the format the comparator
// (tests/tools/capture_vs_readback.py) already reads. Test tooling, not product code. Windows only.
//
// THIS TOOL NEVER SHOWS, HIDES, MOVES OR ACTIVATES THE WINDOW (D-W8-37). The window is what the
// library made of it; a tool that forced a show state would repair the defect it exists to measure.
// It prints `visivel=` and `iconico=` as found.
//
// USAGE: win32_window_capture --title <window title> --out <directory>
//            [--present-budget-ms <n>] [--exit-budget-ms <n>] -- <fixture.exe> [fixture args...]
//   the fixture's command line is passed through verbatim after `--`: the driver owns it.
//
// WHAT IT DOES, in order: launch; wait for the fixture's "presented at attempt" line (default
// 30 s); find the window; print `janelas=`, `visivel=`, `iconico=`; DwmFlush twice; client area
// against the virtual screen; PrintWindow capture into <out>/printwindow/; occlusion guard
// (WindowFromPoint at five points); BitBlt capture into <out>/bitblt/, only when the guard passed
// (a refused BitBlt goes to <out>/bitblt_refused/ as evidence, where the comparator never looks);
// WM_CLOSE; wait for the fixture's exit (default 10 s, then TerminateProcess).
//
// EXIT CODE: 0 all good. 1 the FIXTURE failed (never presented, exited early or non-zero, did not
// exit after WM_CLOSE). 2 the TOOL could not vouch for the capture (bad arguments, no unique
// window, off screen, occluded, a capture API failed). When both happen the tool's 2 wins.
//
// ASSUMPTION, declared: 96 DPI (the CI runner). The tool is not DPI aware, so a scaled desktop
// would shift the screen coordinates the BitBlt reads.

namespace {

using glintfx::capture_tool::child_process;
using glintfx::capture_tool::pixel_rect;

constexpr int k_exit_ok = 0;
constexpr int k_exit_fixture = 1;
constexpr int k_exit_tool = 2;
constexpr std::string_view k_presented_line = "presented at attempt";
constexpr int k_default_present_budget_ms = 30000;
constexpr int k_default_exit_budget_ms = 10000;
constexpr int k_dwm_flushes = 2;

struct tool_options {
    std::string title;
    std::string out_directory;
    int present_budget_ms = k_default_present_budget_ms;
    int exit_budget_ms = k_default_exit_budget_ms;
    std::vector<std::string> fixture_command;
};

void say(const std::string &line) {
    std::printf("win32_window_capture: %s\n", line.c_str());
    std::fflush(stdout);
}

bool read_positive_int(const char *text, int &value) {
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > 600000) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

// Takes the value that follows option `name` at argv[index]; false when the option is not `name`.
bool take_option(const std::vector<std::string> &args, std::size_t &index, std::string_view name,
                 std::string &value) {
    if (args[index] != name) {
        return false;
    }
    value = index + 1 < args.size() ? args[index + 1] : std::string();
    index += 2;
    return true;
}

bool parse_options(const std::vector<std::string> &args, tool_options &options) {
    std::size_t index = 0;
    std::string present_text = std::to_string(k_default_present_budget_ms);
    std::string exit_text = std::to_string(k_default_exit_budget_ms);
    while (index < args.size() && args[index] != "--") {
        const bool known = take_option(args, index, "--title", options.title) ||
                           take_option(args, index, "--out", options.out_directory) ||
                           take_option(args, index, "--present-budget-ms", present_text) ||
                           take_option(args, index, "--exit-budget-ms", exit_text);
        if (!known) {
            return false;
        }
    }
    if (index >= args.size() || options.title.empty() || options.out_directory.empty()) {
        return false;
    }
    options.fixture_command.assign(args.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                                   args.end());
    return !options.fixture_command.empty() &&
           read_positive_int(present_text.c_str(), options.present_budget_ms) &&
           read_positive_int(exit_text.c_str(), options.exit_budget_ms);
}

struct window_search {
    DWORD process_id = 0;
    std::wstring title;
    std::vector<HWND> found;
};

BOOL CALLBACK collect_matching_window(HWND window, LPARAM parameter) {
    auto &search = *reinterpret_cast<window_search *>(parameter);
    DWORD owner = 0;
    ::GetWindowThreadProcessId(window, &owner);
    wchar_t text[256] = {};
    const int length = ::GetWindowTextW(window, text, 256);
    if (owner == search.process_id && length > 0 &&
        std::wstring(text, static_cast<std::size_t>(length)) == search.title) {
        search.found.push_back(window);
    }
    return TRUE;
}

// Every top-level window of process `process_id` whose title is exactly `title`, visible or not.
std::vector<HWND> find_windows(DWORD process_id, const std::string &title) {
    window_search search{
        .process_id = process_id, .title = glintfx::capture_tool::utf8_to_wide(title), .found = {}};
    ::EnumWindows(collect_matching_window, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

// DwmFlush is resolved at run time (dwmapi.dll is never linked, so the zero-dependency gates of
// the repository see no new import and no new header): the composition engine finishes what it
// was asked to draw before the screen is read. A missing DLL or export only prints a note.
using dwm_flush_function = HRESULT(WINAPI *)();

dwm_flush_function find_dwm_flush() {
    const HMODULE dwmapi = ::LoadLibraryW(L"dwmapi.dll");
    if (dwmapi == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<dwm_flush_function>(
        reinterpret_cast<void *>(::GetProcAddress(dwmapi, "DwmFlush")));
}

void flush_dwm() {
    const dwm_flush_function dwm_flush = find_dwm_flush();
    for (int flush = 1; flush <= k_dwm_flushes; ++flush) {
        const std::string result =
            dwm_flush == nullptr ? "indisponivel" : std::to_string(static_cast<long>(dwm_flush()));
        say("dwmflush" + std::to_string(flush) + "=" + result);
    }
}

// The window facts the plan fixes: how many, visible or not, iconic or not. Nothing here changes
// the window.
void report_window_state(HWND window) {
    say("janelas=1 visivel=" + std::to_string(::IsWindowVisible(window) != 0 ? 1 : 0) +
        " iconico=" + std::to_string(::IsIconic(window) != 0 ? 1 : 0));
}

int refuse(const std::string &why) {
    say("RECUSADO " + why);
    return k_exit_tool;
}

// One mechanism's capture goes to its own directory, named like the relay's files.
int save_capture(const std::string &directory, const glintfx::capture_tool::captured_image &image) {
    const glintfx::capture_tool::grab_result written =
        glintfx::capture_tool::write_capture_pair(directory, image);
    if (!written.ok) {
        return refuse(written.detail);
    }
    say("capture " + directory + " " + std::to_string(image.width) + "x" +
        std::to_string(image.height));
    return k_exit_ok;
}

int capture_with_printwindow(HWND window, const tool_options &options) {
    glintfx::capture_tool::captured_image image;
    const glintfx::capture_tool::grab_result grabbed =
        glintfx::capture_tool::grab_with_printwindow(window, image);
    if (!grabbed.ok) {
        return refuse("printwindow: " + grabbed.detail);
    }
    return save_capture(options.out_directory + "/printwindow", image);
}

// The screen read is vouched for only when the five points belong to the window; otherwise the
// verdict names the cause and the pixels go to bitblt_refused/ as evidence.
int capture_with_bitblt(HWND window, const pixel_rect &client, const tool_options &options) {
    const glintfx::capture_tool::verdict guard = glintfx::capture_tool::judge_occlusion(
        glintfx::capture_tool::probe_occlusion(window, client));
    say("oclusao: " + guard.text);
    glintfx::capture_tool::captured_image image;
    const glintfx::capture_tool::grab_result grabbed =
        glintfx::capture_tool::grab_with_bitblt(client, image);
    if (!grabbed.ok) {
        return refuse("bitblt: " + grabbed.detail);
    }
    if (!guard.pass) {
        (void)save_capture(options.out_directory + "/bitblt_refused", image);
        return refuse("bitblt: " + guard.text);
    }
    return save_capture(options.out_directory + "/bitblt", image);
}

// From the unique window to both captures. Returns k_exit_ok or k_exit_tool.
int capture_window(HWND window, const tool_options &options) {
    report_window_state(window);
    flush_dwm();
    const pixel_rect client = glintfx::capture_tool::client_rect_on_screen(window);
    const glintfx::capture_tool::verdict on_screen = glintfx::capture_tool::judge_client_on_screen(
        client, glintfx::capture_tool::virtual_screen_rect());
    if (!on_screen.pass) {
        return refuse(on_screen.text);
    }
    const int printed = capture_with_printwindow(window, options);
    const int blitted = capture_with_bitblt(window, client, options);
    return printed != k_exit_ok || blitted != k_exit_ok ? k_exit_tool : k_exit_ok;
}

// The capture phase: finds THE window of the fixture and captures it. `window` stays null when the
// window was not found, so the shutdown knows whether WM_CLOSE has anyone to talk to.
int run_capture_phase(const child_process &child, const tool_options &options, HWND &window) {
    const std::vector<HWND> windows = find_windows(child.id, options.title);
    const glintfx::capture_tool::verdict unique =
        glintfx::capture_tool::judge_window_count(windows.size());
    if (!unique.pass) {
        say(unique.text);
        return refuse("a unique window of the fixture was not found");
    }
    window = windows.front();
    return capture_window(window, options);
}

// WM_CLOSE (when there is a window), then the fixture must exit by itself within the budget.
int shut_down_fixture(const child_process &child, HWND window, int exit_budget_ms) {
    if (window != nullptr) {
        ::PostMessageW(window, WM_CLOSE, 0, 0);
    }
    unsigned long exit_code = 1;
    if (!glintfx::capture_tool::wait_for_exit(child, exit_budget_ms, exit_code)) {
        glintfx::capture_tool::terminate_child(child);
        say("FIXTURE nao saiu em " + std::to_string(exit_budget_ms) +
            " ms apos WM_CLOSE: TerminateProcess");
        return k_exit_fixture;
    }
    say("fixture exit=" + std::to_string(exit_code));
    return exit_code == 0 ? k_exit_ok : k_exit_fixture;
}

int run(const tool_options &options) {
    child_process child;
    if (!glintfx::capture_tool::launch_child(options.fixture_command, child)) {
        return k_exit_tool;
    }
    std::string output;
    const glintfx::capture_tool::wait_outcome waited = glintfx::capture_tool::wait_for_output(
        child, k_presented_line, options.present_budget_ms, output);
    int code = k_exit_ok;
    HWND window = nullptr;
    if (waited == glintfx::capture_tool::wait_outcome::found) {
        code = run_capture_phase(child, options, window);
    } else {
        say(waited == glintfx::capture_tool::wait_outcome::child_exited
                ? "FIXTURE saiu antes de apresentar o primeiro quadro"
                : "FIXTURE nao apresentou o primeiro quadro no orcamento");
        code = k_exit_fixture;
    }
    const int shutdown_code = shut_down_fixture(child, window, options.exit_budget_ms);
    glintfx::capture_tool::close_child(child);
    return code != k_exit_ok ? code : shutdown_code;
}

} // namespace

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::vector<std::string> args(argv + 1, argv + argc);
    tool_options options;
    if (!parse_options(args, options)) {
        std::fprintf(
            stderr,
            "usage: win32_window_capture --title <title> --out <directory> "
            "[--present-budget-ms <n>] [--exit-budget-ms <n>] -- <fixture.exe> [args...]\n");
        return k_exit_tool;
    }
    const int code = run(options);
    say("exit=" + std::to_string(code));
    return code;
}
