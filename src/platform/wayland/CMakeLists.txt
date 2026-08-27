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

glintfx_add_wayland_xdg_shell_binding(glintfx_library)

target_sources(glintfx_library PRIVATE
    display_adapter.cpp
    selected_display_adapter_check.cpp
)
