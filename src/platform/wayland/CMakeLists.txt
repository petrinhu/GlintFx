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

glintfx_add_wayland_xdg_shell_binding(glintfx_library)

target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    global_catalog.cpp
    selected_display_adapter_check.cpp
    shell_requirements.cpp
    shell_adapter.cpp
    window_configure_sequence.cpp
    window_adapter.cpp
)
