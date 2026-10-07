// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/first_present_map.hpp"
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
// D-W8-47: every swap_buffers() below is preceded by a real draw (clear_frame), because the
// Mesa 26 D3D12 driver (commit 21e5d19f) fails a swap with no GL command since the previous one.
// The empty frame is covered by the slice PRESENT-EMPTY-FRAME (D-W8-48), not by this test.
//
// No Windows runs on the machine that wrote this file (L-27 declared):
// the windows-latest job is the only place these cases execute.

namespace {

constexpr const wchar_t *k_expect_hidden_env = L"GLINTFX_FIRST_PRESENT_EXPECT_SW_HIDE";
constexpr const char *k_hidden_child_case = "win32_first_present_maps_under_sw_hide_child";
constexpr DWORD k_child_timeout_ms = 60'000;

// Draws a real frame (glClearColor + glClear) so the swap that follows presents a frame with
// content. D-W8-47: Mesa 26 (commit 21e5d19f) refuses to present a frame with no GL command since
// the previous swap; the empty frame is covered by PRESENT-EMPTY-FRAME (D-W8-48), not here.
void clear_frame(const glintfx::platform::win32_gl_context_adapter &context) {
    using fn_clear = void (*)(unsigned);
    using fn_clear_color = void (*)(float, float, float, float);
    const auto clear_color = reinterpret_cast<fn_clear_color>(context.proc_address("glClearColor"));
    const auto clear = reinterpret_cast<fn_clear>(context.proc_address("glClear"));
    GLINTFX_CHECK(clear_color != nullptr && clear != nullptr);
    clear_color(0.25F, 0.5F, 0.75F, 1.0F);
    clear(0x00004000);
}

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

// Seam state for the order and failure cases (GODS_LAWS.md L-04: the order is only observable
// from inside the first frame, so the fake records what the adapter had done when it was asked
// to show the window). Single-threaded fixture, plain statics.
std::unique_ptr<glintfx::platform::win32_gl_context_adapter> g_seam_context;
std::uint32_t g_swaps_issued_when_shown = 0xFFFFFFFFu;
DWORD g_seam_error = 0;

// Drops the context on every exit path (the recorder reaches it through a global, not a local
// address).
struct seam_context_guard {
    seam_context_guard() = default;
    seam_context_guard(const seam_context_guard &) = delete;
    seam_context_guard &operator=(const seam_context_guard &) = delete;
    ~seam_context_guard() { g_seam_context.reset(); }
};

DWORD recording_show(HWND window) noexcept {
    g_swaps_issued_when_shown = g_seam_context->swap_calls_issued();
    if (g_seam_error != 0) {
        return g_seam_error;
    }
    return glintfx::platform::show_window_now(window);
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

    clear_frame(context);
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
    clear_frame(context);
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

// A closed and reopened context shows its window on its first frame again (the twin of
// close() resetting every other per-context fact, such as the swap counter).
GLINTFX_TEST(win32_first_present_maps_window_after_context_reopen) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));
    const HWND hwnd = window.native_handle();

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());
    GLINTFX_CHECK(!context.make_current().has_error());
    clear_frame(context);
    GLINTFX_CHECK(!context.swap_buffers().has_error());
    GLINTFX_CHECK(::IsWindowVisible(hwnd) != 0);

    context.close();
    ::ShowWindow(hwnd, SW_HIDE);
    GLINTFX_CHECK(!display.pump_events().has_error());
    GLINTFX_CHECK(::IsWindowVisible(hwnd) == 0);

    GLINTFX_CHECK(!context.open(window, options).has_error());
    GLINTFX_CHECK(!context.make_current().has_error());
    clear_frame(context);
    GLINTFX_CHECK(!context.swap_buffers().has_error());
    const bool visible_after_reopen = ::IsWindowVisible(hwnd) != 0;
    std::println("MEASURED win32_first_present_maps_test.visible_after_reopen_first_present={}",
                 visible_after_reopen);
    GLINTFX_CHECK(visible_after_reopen);
}

// The window must already be visible when the frame is presented: the show comes BEFORE the swap.
GLINTFX_TEST(win32_first_present_maps_window_before_the_swap) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));
    g_seam_context = std::make_unique<glintfx::platform::win32_gl_context_adapter>();
    const seam_context_guard seam_guard;
    glintfx::platform::win32_gl_context_adapter &context = *g_seam_context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());
    GLINTFX_CHECK(!context.make_current().has_error());

    g_seam_error = 0;
    g_swaps_issued_when_shown = 0xFFFFFFFFu;
    context.set_first_present_show_for_test(&recording_show);
    clear_frame(context);
    const auto presented = context.swap_buffers();
    context.set_first_present_show_for_test(nullptr);
    GLINTFX_CHECK(!presented.has_error());
    std::println("MEASURED win32_first_present_maps_test.swaps_issued_when_shown={}",
                 g_swaps_issued_when_shown);
    GLINTFX_CHECK(g_swaps_issued_when_shown == 0);
    GLINTFX_CHECK(context.swap_calls_issued() == 1);
    GLINTFX_CHECK(::IsWindowVisible(window.native_handle()) != 0);
}

// A failure to show is a platform_failure "show_window" with a non-zero code, no swap is issued,
// and the next frame tries again.
GLINTFX_TEST(win32_first_present_show_failure_is_reported_and_retried) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));
    g_seam_context = std::make_unique<glintfx::platform::win32_gl_context_adapter>();
    const seam_context_guard seam_guard;
    glintfx::platform::win32_gl_context_adapter &context = *g_seam_context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());
    GLINTFX_CHECK(!context.make_current().has_error());

    g_seam_error = ERROR_INVALID_STATE;
    context.set_first_present_show_for_test(&recording_show);
    ::SetLastError(0); // the failure must not depend on GetLastError() carrying anything
    clear_frame(context);
    const auto failed = context.swap_buffers();
    GLINTFX_CHECK(failed.has_error());
    GLINTFX_CHECK(failed.err().code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(failed.err().rejected_value() == std::string_view{"show_window"});
    GLINTFX_CHECK(failed.err().os_error_code() == ERROR_INVALID_STATE);
    GLINTFX_CHECK(context.swap_calls_issued() == 0);
    GLINTFX_CHECK(::IsWindowVisible(window.native_handle()) == 0);

    g_seam_error = 0;
    clear_frame(context);
    const auto retried = context.swap_buffers();
    context.set_first_present_show_for_test(nullptr);
    GLINTFX_CHECK(!retried.has_error());
    GLINTFX_CHECK(context.swap_calls_issued() == 1);
    GLINTFX_CHECK(::IsWindowVisible(window.native_handle()) != 0);
}

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
