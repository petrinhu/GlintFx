# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/port/CMakeLists.txt
#
# Single subject (ARCH-PORTS, GODS_LAWS.md L-19): the display
# connection port itself - display_connection_port.hpp (the concept)
# and display_connection.hpp (the RAII wrapper every caller programs
# against). Both are header-only: neither has behavior of its own to
# compile, only a compile-time contract and a template that forwards
# to whatever adapter satisfies it, so there is no .cpp to add as a
# source here.
#
# Added unconditionally (called from src/platform/CMakeLists.txt
# BEFORE its own UNIX/WIN32/unsupported branch, GODS_LAWS.md L-19 item
# 2): the port touches no operating-system header at all (see display_
# connection_port.hpp's own "DELIBERATELY NARROW" comment), so there is
# nothing platform-specific to gate on this file's side. A backend
# directory (src/platform/wayland/ today, a future src/platform/win32/)
# is what is conditionally added, never this one.
#
# The one piece of CMake work this file does: makes "platform/port/
# *.hpp" resolvable via a QUOTED, src-rooted include, on glintfx_library
# itself - the SAME "${PROJECT_SOURCE_DIR}/src" pattern tests/CMakeLists.txt
# already uses (PRIVATE, per test target) to reach src/gfss/ and
# src/render/'s own private headers. ARCH-PORTS is the first PRODUCTION
# (not test-only) code that needs this: src/platform/wayland/
# display_adapter.cpp includes "platform/port/display_connection_port.hpp".
# Added exactly once, here, rather than once per backend, so a future
# backend does not have to remember to repeat it.
target_include_directories(glintfx_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
