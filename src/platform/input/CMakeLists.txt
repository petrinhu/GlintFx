# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/input/CMakeLists.txt
#
# Single subject: seat_capabilities (S-A', docs/plano-w6a-janela.md
# fatia 4, GODS_LAWS.md L-19) - the one shared, OS-agnostic value type
# both seat mechanisms (wayland/seat_adapter.cpp's S-B, win32/
# seat_adapter.cpp's Y-1) write into through set_capability() and
# every caller reads through has_capability(). Only includes
# <cstdint> - added UNCONDITIONALLY by src/platform/CMakeLists.txt,
# before its own UNIX/WIN32 branch, same reasoning port/ and window/
# already document one directory over: there is nothing platform-
# specific here to gate.
#
# CONSERTO (05/09/2026, run 33997698610): this CMakeLists.txt did not
# exist until this commit, and seat_capabilities.cpp had never been
# wired into glintfx_library on EITHER platform - only ever compiled a
# second time by tests/CMakeLists.txt (seat_capabilities_test, seat_
# adapter_listener_test, win32_seat_translation_test/seat_test). The
# Windows job's linker was the one that caught it: win32/seat_
# adapter.cpp calls set_capability() and IS wired into glintfx_library
# (win32/CMakeLists.txt's own target_sources), so a build reaching that
# TU with no definition anywhere in the library fails at link time
# (LNK2019/LNK1120) - see src/platform/CMakeLists.txt's own header
# comment on add_subdirectory(input) for why the Linux job did not
# catch the same gap.
target_sources(glintfx_library PRIVATE seat_capabilities.cpp)
