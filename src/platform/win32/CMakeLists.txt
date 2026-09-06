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
# seat_adapter.cpp - Y-1 (docs/plano-w6a-janela.md fatia 13, TODO.md
# WIN-SEAT): the seat's own message-only window, built under the
# display adapter's own class (seat_adapter.hpp's own "WHY A SECOND
# HWND" paragraph), feeding the SAME glintfx::platform::seat_
# capabilities (platform/input/seat_capabilities.hpp) S-B (Wayland)
# feeds from its own, completely different, mechanism.
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
# L-07, same category as libwayland-client on Linux") - seat_
# adapter.cpp's own RegisterRawInputDevices/GetRawInputDeviceList/
# GetSystemMetrics/SetWindowLongPtrW/CallWindowProcW are all user32
# exports too, nothing new to link.
#
# window_adapter.cpp - X-2 (docs/plano-w6a-janela.md fatia 9, TODO.md
# WIN-WINDOW): the real, top-level window (WS_OVERLAPPEDWINDOW, never
# WS_VISIBLE - D-W5-13), sharing the display adapter's own registered
# class (display_adapter.hpp's own "F2/D-W6a-16" paragraph). app_
# user_model_id.cpp (D-W6a-21) is its OWN target_sources entry on
# purpose, not folded into window_adapter.cpp - this project's own §6
# table for this fatia: "o .cpp de application_id e um atomo
# separado... para uma reprovacao de lint ali nao derrubar o
# adaptador inteiro" - <shellapi.h>/<propsys.h>/<propkey.h>/
# <propvarutil.h> are heavy, macro-hostile system headers isolated to
# that one file alone.
#
# shell32: SHGetPropertyStoreForWindow (app_user_model_id.cpp).
# ole32: CoInitializeEx/CoUninitialize/PropVariantClear (same file).
# propsys: InitPropVariantFromString, and PKEY_AppUserModel_ID's own
# storage (propkey.h declares it `extern` without INITGUID - the
# symbol's DEFINITION lives in Propsys.lib, not in this project's own
# object files).
#
# window_message_route.cpp - split OUT of window_adapter.cpp (fix for
# the link defect the windows-latest runner found live, X-2/WIN-WINDOW
# fatia 9's own review): handle_message() and the free function that
# reaches it from display_adapter.cpp's own shared window_proc are
# their own atom, a separate reason to change from window_adapter.cpp's
# own lifecycle code (open/close/set_title) - see that file's own
# target_sources comment above for the app_user_model_id.cpp precedent,
# and window_message_route.cpp's own header comment for the full
# reasoning (GODS_LAWS.md L-17).
target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    selected_display_adapter_check.cpp
    seat_adapter.cpp
    window_adapter.cpp
    window_message_route.cpp
    app_user_model_id.cpp
)

target_link_libraries(glintfx_library PRIVATE user32 shell32 ole32 propsys)
