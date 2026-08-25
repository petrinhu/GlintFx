# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/wayland/CMakeLists.txt
#
# Single subject, THIS fatia (WL-PROTO): attaches the generated
# xdg-shell protocol binding to glintfx_library. Nothing here opens a
# connection, creates a window or handles an event (GODS_LAWS.md L-05:
# that is WL-DISPLAY, a later fatia, in a later onda) - this file's
# only job today is making the binding part of the library's own
# object set, PRIVATE (GODS_LAWS.md L-19: the public surface does not
# move). No hand-written .cpp is added here yet; the day connection
# code lands, it target_sources(glintfx_library ...)s from this same
# file, the same pattern src/core/CMakeLists.txt already uses.

glintfx_add_wayland_xdg_shell_binding(glintfx_library)
