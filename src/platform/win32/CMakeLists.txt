# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/win32/CMakeLists.txt
#
# Single subject: the Win32 backend (WIN-DISPLAY, TODO.md,
# GODS_LAWS.md L-04 reabertura de ARCH-PORTS por paridade) - the exact
# win32/ sibling src/platform/CMakeLists.txt's own WIN32 branch comment
# promised: win32_display_adapter (display_adapter.hpp/.cpp), the
# compile-time alias that selects it (selected_display_adapter.hpp),
# and the internal TU that proves the alias satisfies the port
# (selected_display_adapter_check.cpp - see that file's own header
# comment). No registry, no roundtrip, no event pump, no visible window
# - this fatia's own adapter only opens and closes a message-only
# connection (fechar-w1.md sec. 2.3: WL-DISPLAY fatias B/C/D and
# WIN-WINDOW have no par requested by this onda).
#
# All PRIVATE (GODS_LAWS.md L-19: the public surface does not move for
# an internal platform detail) - the exact same shape src/platform/
# wayland/CMakeLists.txt already uses.
#
# user32: carries RegisterClassExW/CreateWindowExW/DestroyWindow/
# UnregisterClassW/DefWindowProcW/GetModuleHandleW - the SAME system
# library tests/win32_runner_probe_test.cpp already links for the
# identical calls (tests/CMakeLists.txt: "user32 carries
# CreateWindowExW/RegisterClassExW/DestroyWindow/PeekMessageW - Win32
# counts as system API for the dependency-zero rule, GODS_LAWS.md
# L-07, same category as libwayland-client on Linux").
target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    selected_display_adapter_check.cpp
)

target_link_libraries(glintfx_library PRIVATE user32)
