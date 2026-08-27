# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/CMakeLists.txt
#
# Single subject: the layer that touches the operating system
# (GODS_LAWS.md L-19), and - as of ARCH-PORTS - the THREE-STATE
# backend selection for platform::display_connection_port (CTO plan
# sec. 1.3). This file is now entered on EVERY platform (src/CMakeLists.txt
# calls add_subdirectory(platform) unconditionally, no longer wrapped
# in if(UNIX)): the branching below is what used to live in that outer
# guard, moved HERE so a platform this project has not written a
# backend for yet fails the CONFIGURE step loudly instead of the
# subdirectory simply never being entered.
#
# port/ is added unconditionally, before the branch: the port's
# concept and RAII wrapper (src/platform/port/) touch no operating-
# system header at all (see display_connection_port.hpp's own
# "DELIBERATELY NARROW" comment), so there is nothing platform-specific
# to gate for that directory - only a BACKEND directory (wayland/
# today) is conditional.
add_subdirectory(port)

# UNIX: selects the Wayland backend (WL-PROTO, GODS_LAWS.md L-05 -
# Wayland is the only Linux surface this project targets). wayland/'s
# own CMakeLists.txt wires in wayland_display_adapter and the internal
# TU (selected_display_adapter_check.cpp) whose static_assert proves,
# on EVERY build that reaches this branch, that the selection actually
# satisfies platform::display_connection_port - a build that somehow
# wired in nothing usable fails to COMPILE here, never links an empty
# platform layer in silence (GODS_LAWS.md L-40).
if(UNIX)
    add_subdirectory(wayland)

# WIN32: a DECLARED absence, not an error and not silence (CTO plan,
# decision D-AP-5). WIN-WINDOW (TODO.md) is the fatia that gives
# Windows its own backend directory here, the same shape wayland/ has
# today; until then, the Windows CI leg keeps compiling glintfx with no
# display backend at all - UNCHANGED from every fatia before ARCH-PORTS
# (GODS_LAWS.md L-04: the five-platform matrix stays green).
elseif(WIN32)
    message(STATUS
        "glintfx: no display backend on this platform yet (WIN-WINDOW pending, see TODO.md)")

# Any OTHER platform: FATAL_ERROR at configure time, on purpose. The
# danger this branch exists to close is not "Windows has no backend
# today" (that is the declared, named WIN32 case right above) - it is
# a SIXTH, unknown platform passing this file in silence and compiling
# an empty platform layer nobody decided to accept (GODS_LAWS.md L-40:
# "o portao nao erra a analise - ele nao olha, e imprime verde",
# applied here to a configure step instead of a shell gate).
else()
    message(FATAL_ERROR
        "glintfx: unsupported platform for the display connection port "
        "(GODS_LAWS.md L-19/ARCH-PORTS) - supported today: UNIX (Wayland backend) "
        "and WIN32 (backend pending, WIN-WINDOW, TODO.md). A build reaching this "
        "point on any other platform must fail loud here, never compile an empty "
        "platform layer in silence.")
endif()
