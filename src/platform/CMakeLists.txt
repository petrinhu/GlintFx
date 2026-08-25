# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/CMakeLists.txt
#
# Single subject: the layer that touches the operating system
# (GODS_LAWS.md L-19). Only add_subdirectory()s below - the platform
# adapters themselves each get their own CMakeLists.txt, one per
# backend, the same one-file-one-assunto pattern as src/CMakeLists.txt
# uses for core/ vs. platform/. Only included from src/CMakeLists.txt
# under if(UNIX) (WL-PROTO, GODS_LAWS.md L-05): today wayland/ is the
# only backend, because Wayland is the only Linux surface this project
# targets; a Windows backend, when it is born, becomes its own sibling
# add_subdirectory() here, guarded by its own platform check, never
# mixed into wayland/'s sources.

add_subdirectory(wayland)
