// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <print>
#include <string>
#include <vector>

#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/wgl_context_adapter.hpp"
#include "platform/win32/window_adapter.hpp"

// win32_first_present_maps_test.cpp - WIN-MAP-FIRST-PRESENT (D-W8-38,
// D-W8-41; GODS_LAWS.md L-04/L-20/L-36). D-W5-8 ("mapear e de quem
// anexa buffer") and D-W5-13 (open() never shows) say a window is born
// invisible and the FIRST presented frame maps it, on both systems. On
// Wayland the first committed buffer maps the surface; this fixture is
// the Windows side of that promise, measured on the real internals (the
// HWND is internal, like win32_iconic_present_test).
//
// What each case proves (criteria written BEFORE the data, D-W8-38):
//   (1) after open(), the window is NOT visible (D-W5-13 still holds);
//   (2) after the first swap_buffers() answering `presented`, the window
//       IS visible, and a later swap_buffers() never shows it again
//       (once only: a window hidden afterwards by someone else stays
//       hidden, that is WIN-PRESENT-HIDDEN-EXTERNAL, not this item);
//   (3) the same as (2) when the process was launched with
//       STARTF_USESHOWWINDOW and SW_HIDE, the parent case relaunches
//       this very executable that way and requires exit code 0.
// Criterion (4), the first frame of gltfx_loop, and (5), the external
// capture, are proved by capture_known_color_smoke on the Windows
// executor (C2b-4), which runs the consumer loop and never shows the
// window itself.
//
// No Windows runs on the machine that wrote this file (L-27 declared):
// the windows-latest job is the only place these cases execute.

namespace {

constexpr const wchar_t *k_expect_hidden_env = L"GLINTFX_FIRST_PRESENT_EXPECT_SW_HIDE";
constexpr const char *k_hidden_child_case = "win32_first_present_maps_under_sw_hide_child";
constexpr DWORD k_child_timeout_ms = 60'000;

[[nodiscard]] bool
open_display_and_window(glintfx::platform::win32_display_adapter &display,
                        glintfx::platform::win32_window_adapter &window) noexcept {
    if (display.open().has_error()) {
        return false;
    }
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 320;
    desc.logical_height = 240;
    desc.title = "glintfx win32 first present maps test";
    return !window.open(display, desc).has_error();
}

void print_error_detail(std::string_view label, const glintfx::gltfx_err &err) noexcept {
    std::println("MEASURED win32_first_present_maps_test.{}_error_code={}", label,
                 glintfx::gltfx_err_code_name(err.code()));
    std::println("MEASURED win32_first_present_maps_test.{}_rejected_value={}", label,
                 err.rejected_value());
    std::println("MEASURED win32_first_present_maps_test.{}_os_error_code={}", label,
                 err.os_error_code());
}

// The instrument must be proved (L-36): when the parent asked for a
// hidden launch, THIS process has to see it in its own STARTUPINFO,
// otherwise the child would pass without the lancador ever hiding it.
void check_launch_matches_expectation() {
    const DWORD needed = ::GetEnvironmentVariableW(k_expect_hidden_env, nullptr, 0);
    if (needed == 0) {
        return;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    ::GetStartupInfoW(&startup);
    const bool launched_hidden =
        (startup.dwFlags & STARTF_USESHOWWINDOW) != 0 && startup.wShowWindow == SW_HIDE;
    std::println("MEASURED win32_first_present_maps_test.launched_with_sw_hide={}",
                 launched_hidden);
    GLINTFX_CHECK(launched_hidden);
}

// Criteria (1) and (2), shared by the direct case and the hidden child.
void check_first_present_maps_and_only_once() {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));
    const HWND hwnd = window.native_handle();

    const bool visible_after_open = ::IsWindowVisible(hwnd) != 0;
    std::println("MEASURED win32_first_present_maps_test.visible_after_open={}",
                 visible_after_open);
    GLINTFX_CHECK(!visible_after_open);

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());
    GLINTFX_CHECK(!context.make_current().has_error());

    // Still invisible after the context exists: opening a context is not presenting.
    GLINTFX_CHECK(::IsWindowVisible(hwnd) == 0);

    const auto first = context.swap_buffers();
    if (first.has_error()) {
        print_error_detail("first_swap", first.err());
    }
    GLINTFX_CHECK(!first.has_error());
    const bool presented = first.value() == glintfx::gltfx_present_outcome::presented;
    std::println("MEASURED win32_first_present_maps_test.first_swap_presented={}", presented);
    GLINTFX_CHECK(presented);

    const bool visible_after_first_present = ::IsWindowVisible(hwnd) != 0;
    std::println("MEASURED win32_first_present_maps_test.visible_after_first_present={}",
                 visible_after_first_present);
    GLINTFX_CHECK(visible_after_first_present);

    // Once only: somebody else hides the window; the next frame must not map it again.
    ::ShowWindow(hwnd, SW_HIDE);
    GLINTFX_CHECK(!display.pump_events().has_error());
    GLINTFX_CHECK(::IsWindowVisible(hwnd) == 0);
    const auto second = context.swap_buffers();
    if (second.has_error()) {
        print_error_detail("second_swap", second.err());
    }
    GLINTFX_CHECK(!second.has_error());
    const bool visible_after_second_present = ::IsWindowVisible(hwnd) != 0;
    std::println("MEASURED win32_first_present_maps_test.visible_after_second_present={}",
                 visible_after_second_present);
    GLINTFX_CHECK(!visible_after_second_present);
}

} // namespace

GLINTFX_TEST(win32_first_present_maps_window) { check_first_present_maps_and_only_once(); }

// Entry the parent relaunches. Run directly (no env var) it is the same
// check as the case above, so a full-suite run stays meaningful.
GLINTFX_TEST(win32_first_present_maps_under_sw_hide_child) {
    check_launch_matches_expectation();
    check_first_present_maps_and_only_once();
}

// Criterion (3): a lancador asking SW_HIDE must not keep the window
// hidden (parity with Wayland, where mapping is unconditional).
GLINTFX_TEST(win32_first_present_maps_window_under_sw_hide) {
    wchar_t exe_path[MAX_PATH]{};
    const DWORD length = ::GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    GLINTFX_CHECK(length > 0 && length < MAX_PATH);

    const std::string case_name = k_hidden_child_case;
    std::wstring command_line = L"\"";
    command_line += exe_path;
    command_line += L"\" ";
    command_line.append(case_name.begin(), case_name.end());

    GLINTFX_CHECK(::SetEnvironmentVariableW(k_expect_hidden_env, L"1") != 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
    for (const HANDLE h : {startup.hStdInput, startup.hStdOutput, startup.hStdError}) {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            ::SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
    }

    PROCESS_INFORMATION process{};
    const BOOL created = ::CreateProcessW(exe_path, command_line.data(), nullptr, nullptr, TRUE, 0,
                                          nullptr, nullptr, &startup, &process);
    const DWORD create_error = created != 0 ? 0 : ::GetLastError();
    ::SetEnvironmentVariableW(k_expect_hidden_env, nullptr);
    std::println("MEASURED win32_first_present_maps_test.child_created={}", created != 0);
    if (created == 0) {
        std::println("MEASURED win32_first_present_maps_test.child_create_os_error_code={}",
                     create_error);
    }
    GLINTFX_CHECK(created != 0);

    const DWORD waited = ::WaitForSingleObject(process.hProcess, k_child_timeout_ms);
    DWORD exit_code = 1;
    if (waited == WAIT_OBJECT_0) {
        ::GetExitCodeProcess(process.hProcess, &exit_code);
    } else {
        ::TerminateProcess(process.hProcess, 1);
    }
    ::CloseHandle(process.hProcess);
    ::CloseHandle(process.hThread);
    std::println("MEASURED win32_first_present_maps_test.child_exit_code={}", exit_code);
    GLINTFX_CHECK(waited == WAIT_OBJECT_0);
    GLINTFX_CHECK(exit_code == 0);
}

#endif // defined(_WIN32)
