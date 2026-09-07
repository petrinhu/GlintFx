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

# window/ is added unconditionally too, same reasoning as port/ above:
# window_state/window_desc_validation/utf8_validation (W-C', docs/
# plano-w6a-janela.md fatia 4) touch no operating-system header either -
# they are the ONE shared value type and the two validation atoms both
# backends (wayland/ today, win32/ WIN-WINDOW) write into and read from.
# Wired into glintfx_library by WL-WINDOW fatia W-E, the first
# production caller (src/platform/window/CMakeLists.txt's own header
# comment).
add_subdirectory(window)

# input/ is added unconditionally too, same reasoning as port/ and
# window/ above: seat_capabilities.hpp/.cpp (S-A', docs/plano-w6a-
# janela.md fatia 4, GODS_LAWS.md L-19) is the ONE shared value type
# both seat mechanisms (wayland/seat_adapter.cpp's S-B, win32/
# seat_adapter.cpp's Y-1) write into through set_capability() and
# every caller reads through has_capability() - it only includes
# <cstdint>, nothing operating-system-specific to gate here either.
#
# CONSERTO (05/09/2026, run 33997698610): this add_subdirectory() was
# MISSING until this commit - seat_capabilities.cpp had never been
# wired into glintfx_library on EITHER platform, only ever compiled a
# second time by tests/CMakeLists.txt (seat_capabilities_test,
# seat_adapter_listener_test, win32_seat_translation_test/seat_test).
# The Linux job stayed green because wayland/seat_adapter.cpp (the
# only production caller of set_capability() at the time) was ALSO
# missing from wayland/CMakeLists.txt's own target_sources - no
# caller in the library meant no pending symbol for the Linux linker
# to find. The Windows job broke first only because Y-1 (win32/
# seat_adapter.cpp) had already been wired into win32/CMakeLists.txt's
# target_sources in the same commit that introduced this gap - the
# Microsoft linker refused to produce glintfx.dll/glintfx.lib with
# seat_capabilities::set_capability() genuinely unresolved
# (LNK2019/LNK1120). Both gaps are closed together: this file gains
# the missing add_subdirectory(input) here, and wayland/CMakeLists.txt
# gains seat_adapter.cpp in its own target_sources (that file's own
# header comment) - the two seat mechanisms now enter glintfx_library
# THE SAME WAY on both platforms, closing the asymmetry that let one
# side compile silently while the other's linker caught it.
add_subdirectory(input)

# gl/ is added unconditionally too, same reasoning as port/, window/
# and input/ above: C-OPT's own registry and two pure atoms (docs/
# plano-w6b-placa-e-laco.md fatia 2a, src/platform/gl/CMakeLists.txt's
# own header comment) touch no operating-system header either - the
# gl_context handle fatia 2b adds to this same directory later is what
# first reaches EGL/WGL, and it will need its own per-platform wiring
# then, same as window/ already separates window_state.cpp (here) from
# wayland/window_adapter.cpp and win32/window_adapter.cpp (the two
# branches below).
add_subdirectory(gl)

# loop/ is added unconditionally too, same reasoning as port/, window/,
# input/ and gl/ above: LOOP-RUN's own fatia 6a atoms (frame_tick_
# state.cpp, frame_cap_schedule.cpp, loop_callbacks_validation.cpp -
# docs/plano-w6b-fatias-6-8.md sec. 8.1, src/platform/loop/CMakeLists.txt's
# own header comment) touch no operating-system header either - the
# facade that first reaches into a real display/window/context (a
# later fatia, 6b) is what will need its own per-platform wiring, same
# as window/ already separates window_state.cpp (here) from wayland/
# window_adapter.cpp and win32/window_adapter.cpp below.
add_subdirectory(loop)

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

# WIN32: WIN-DISPLAY (TODO.md, GODS_LAWS.md L-04 reabertura de
# ARCH-PORTS por paridade - "so entrego janela quando os dois sistemas
# tiverem") gives Windows its own backend directory here, the same
# shape wayland/ has above - win32/ wires in win32_display_adapter and
# the internal TU (selected_display_adapter_check.cpp) whose static_
# assert proves, on every build that reaches this branch, that the
# selection actually satisfies platform::display_connection_port,
# exactly the same guarantee the UNIX branch above already has. Only
# open/close/is_open exist so far - registry, roundtrip, the event
# pump and a VISIBLE window (WIN-WINDOW) still have no par requested
# and stay out of this branch until they do.
elseif(WIN32)
    add_subdirectory(win32)

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
