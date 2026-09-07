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
#
# wgl_extension_loader.cpp / wgl_proc_address.cpp / wgl_context_
# adapter.cpp / selected_gl_context_adapter_check.cpp - X-WGL (docs/
# plano-w6b-placa-e-laco.md fatia 4, D-W6b-1/4/6/7/17/18, GODS_LAWS.md
# L-04/L-19): the Win32 counterpart of W-EGL's own egl_context_adapter.
# cpp/frame_callback_sequence.cpp (src/platform/wayland/, fatia 3) -
# each its own atom, same "one .cpp per reviewable reason to change"
# reasoning app_user_model_id.cpp/window_message_route.cpp already
# established above: wgl_extension_loader.cpp is the disposable-window
# dance to resolve wglChoosePixelFormatARB/wglCreateContextAttribsARB/
# wglSwapIntervalEXT; wgl_proc_address.cpp is the general-purpose
# proc_address() resolver every GL function (including the ~40 GL 1.1
# ones wglGetProcAddress never resolves) goes through; wgl_context_
# adapter.cpp is win32_gl_context_adapter itself.
#
# opengl32/gdi32 - the two system libraries this fatia's own atoms link
# against directly (GODS_LAWS.md L-07, "Win32 counts as system API for
# the dependency-zero rule", the SAME category user32 already sits in
# above): opengl32.lib carries wglCreateContext/wglMakeCurrent/wglDelete
# Context/wglGetProcAddress plus the GL 1.0/1.1 functions this fatia
# links directly (glGetString/glGetIntegerv, wgl_context_adapter.cpp's
# own header comment); gdi32.lib carries ChoosePixelFormat/SetPixel
# Format/DescribePixelFormat/SwapBuffers - the same two libraries tests/
# CMakeLists.txt's own win32_runner_probe_test target already links for
# the identical set of calls (X-GL-0, that test's own header comment).
target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    selected_display_adapter_check.cpp
    seat_adapter.cpp
    window_adapter.cpp
    selected_window_adapter_check.cpp
    window_message_route.cpp
    app_user_model_id.cpp
    wgl_extension_loader.cpp
    wgl_proc_address.cpp
    wgl_context_adapter.cpp
    selected_gl_context_adapter_check.cpp
    dxcore_adapter_match.cpp
    dxcore_gpu_kind.cpp
    dxcore_adapter_enumeration.cpp
    gl_device_luid.cpp
)

# GL-GPU-KIND (docs/plano-w6b-fatias-5b-revisao.md sec. 3/4.1, D-W6b-
# 37/40, GODS_LAWS.md L-04/L-07/L-19): "quem identifica as placas de
# video e o OS" on Windows is DXCore, per adapter, WITHOUT counting how
# many adapters exist - DXGI plays no part (sec. 3's own "DXGI sai do
# desenho inteiro"). dxcore_adapter_match.cpp/dxcore_gpu_kind.cpp are
# the two pure, "Win32-domain not Win32-only" atoms (tests/CMakeLists.
# txt's own comment on their unit tests has the full reasoning);
# dxcore_adapter_enumeration.cpp is the real LoadLibraryW(L"dxcore.dll")
# + COM enumeration (NEVER a static import - tools/ci/check-dep-zero-
# win.ps1's own allowlist stays untouched); gl_device_luid.cpp reads
# GL_DEVICE_LUID_EXT off the current context (F13 of the plan) so
# wgl_context_adapter.cpp can match it against DXCore's own
# InstanceLuid (D-W6b-32).

target_link_libraries(glintfx_library PRIVATE user32 shell32 ole32 propsys opengl32 gdi32)
