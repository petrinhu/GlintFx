# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/wayland/CMakeLists.txt
#
# Single subject: the Wayland backend - the generated xdg-shell
# protocol binding (WL-PROTO), and, as of ARCH-PORTS, the FIRST
# adapter to satisfy platform::display_connection_port
# (src/platform/port/display_connection_port.hpp): wayland_display_
# adapter (display_adapter.hpp/.cpp), the compile-time alias that
# selects it (selected_display_adapter.hpp), and the internal TU that
# proves the alias satisfies the port (selected_display_adapter_check.cpp -
# see that file's own header comment). Registry, roundtrip and the
# event pump are still out of scope here (GODS_LAWS.md L-05: that is
# WL-DISPLAY, a later fatia, in a later onda) - this fatia's own
# adapter only opens and closes a connection.
#
# All PRIVATE (GODS_LAWS.md L-19: the public surface does not move for
# an internal platform detail).
#
# WL-DISPLAY fatia A adds global_catalog.cpp (TODO.md): the pure
# registry-announcement record display_adapter.cpp's own fatia B
# extends this same class with (registry creation, listener, roundtrip)
# will populate. No Wayland type in that file itself - see its own
# header comment.
#
# WL-WINDOW fatia W-B (docs/plano-w6a-janela.md fatia 5) adds
# shell_requirements.cpp (pure, no Wayland type reachable - same shape
# as global_catalog.cpp) and shell_adapter.cpp (the wl_compositor/
# xdg_wm_base adapter WL-WINDOW fatia W-E builds a window on top of).
#
# WL-WINDOW fatia W-E (docs/plano-w6a-janela.md fatia 8) adds window_
# adapter.cpp (the wl_surface/xdg_surface/xdg_toplevel adapter itself,
# the one caller that reaches for shell_adapter's compositor()/shell()
# accessors) and window_configure_sequence.cpp (the pure xdg_surface/
# xdg_toplevel configure-sequence translator, W-C', already living in
# this directory since fatia 4 - only ever compiled a second time by
# its own unit test before this fatia, same shape global_catalog.cpp
# had before fatia B). window_adapter.cpp also depends on src/platform/
# window/'s three files (window_state.cpp, window_desc_validation.cpp,
# utf8_validation.cpp), sourced into glintfx_library from that
# directory's own CMakeLists.txt (src/platform/CMakeLists.txt's window/
# subdirectory) - not repeated here.

# GL-CONTEXT fatia 5, P-GL (docs/plano-w6b-fatias-5.md, D-W6b-28) adds
# connection_failure.cpp: the "a dead wl_display names the interface
# that reprovou" atom, EXTRACTED from display_adapter.cpp's own
# anonymous-namespace build_fatal_error() the moment egl_context_
# adapter.cpp (this same fatia) became a SECOND caller that needs the
# identical logic - not a "regra de 3" extraction, a "two callers, one
# body" one (GODS_LAWS.md L-33).

glintfx_add_wayland_xdg_shell_binding(glintfx_library)

target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    global_catalog.cpp
    selected_display_adapter_check.cpp
    shell_requirements.cpp
    shell_adapter.cpp
    window_configure_sequence.cpp
    window_adapter.cpp
    selected_window_adapter_check.cpp
    seat_adapter.cpp
    frame_callback_sequence.cpp
    connection_failure.cpp
    egl_context_adapter.cpp
    selected_gl_context_adapter_check.cpp
)

# W-EGL (docs/plano-w6b-placa-e-laco.md fatia 3, D-W6b-3): EGL and
# libwayland-egl, PRIVATE, the same plain pkg-config-variable technique
# glintfx_add_wayland_xdg_shell_binding() already uses one file over
# (cmake/GlintfxEgl.cmake's own header comment has the full reasoning,
# not repeated here). egl_context_adapter.cpp is the ONLY tracked file
# under glintfx_library's own object set that reaches for <EGL/egl.h>/
# <EGL/eglext.h>/<wayland-egl.h> - tests/container/egl_probe_smoke.cpp
# (fatia 1) is a STANDALONE test executable, compiled by tests/
# container/Containerfile, never by this CMakeLists.txt.
glintfx_add_egl(glintfx_library)

# gl_context_facade.cpp (src/platform/gl/CMakeLists.txt's own header
# comment) is DELIBERATELY STILL NOT WIRED into glintfx_library's own
# target_sources() here: it PIMPLs over platform::selected_gl_context_
# adapter, which this fatia (Wayland/EGL) now provides on Linux - but
# gl_context_impl.hpp's own #if also expects src/platform/win32/
# selected_gl_context_adapter.hpp to exist on Windows, and fatia 4
# (Win32/WGL) has not landed in this commit. Wiring gl_context_facade.
# cpp in NOW would build fine here, on Linux, and break the SAME single
# shared CMakeLists.txt on Windows - docs/plano-w6b-placa-e-laco.md's
# own push-2 bundles fatias 2+3+4 for exactly this reason. The LAST
# step of whichever of fatia 3/4 lands second in the shared branch is
# adding src/platform/gl/gl_context_facade.cpp to src/platform/gl/
# CMakeLists.txt's own target_sources() call - not this file, and not
# this commit.

# seat_adapter.cpp - WL-SEAT fatia S-B (docs/plano-w6a-janela.md fatia
# 12, seat_adapter.hpp's own header comment): wayland_seat_adapter, the
# Wayland MECHANISM that feeds the shared seat_capabilities value type
# (src/platform/input/, S-A'). CONSERTO (05/09/2026, run 33997698610):
# this file existed in this directory since S-B was written, but was
# MISSING from the list above until this commit - the win32/ sibling
# (win32/CMakeLists.txt's own target_sources) already carried its own
# seat_adapter.cpp, so the two seat mechanisms entered glintfx_library
# asymmetrically: Windows wired in, Linux did not. That asymmetry is
# exactly why the Windows linker caught the missing seat_capabilities
# definition (LNK2019) while the Linux undefined-symbol gate
# (tests/tools/check_no_undef_glintfx.py) stayed green - with no
# caller in the Linux library, there was no pending symbol for it to
# find. Both platforms now wire their own seat mechanism into
# glintfx_library the same way (src/platform/CMakeLists.txt's own
# add_subdirectory(input) header comment has the full account).
