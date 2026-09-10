# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/loop/CMakeLists.txt
#
# Single subject: LOOP-RUN's own files under src/platform/loop/ -
# fatia 6a's three pure atoms (docs/plano-w6b-fatias-6-8.md sec. 8.1) -
# frame_tick_state.cpp (D-W6b-42/44/45), frame_cap_schedule.cpp
# (D-W6b-49) and loop_callbacks_validation.cpp (D-W6b-43) - plus fatia
# 6b's own facade (sec. 8.3) - loop_impl.cpp (the allocate_loop_impl()
# atom, loop_impl.hpp's own header comment) and loop_facade.cpp (the
# gltfx_loop PIMPL itself, loop_facade.cpp's own header comment). None
# of the fatia-6a atoms reaches an operating-system header (each is a
# pure atom over plain data, the same "gl/ added unconditionally"
# reasoning src/platform/gl/CMakeLists.txt's own header comment already
# documents one directory over); loop_facade.cpp DOES reach one -
# transitively, through display_impl.hpp/window_impl.hpp/gl_context_
# impl.hpp's own internal #if branches (the SAME "the impl headers
# branch by platform internally, the facade .cpp itself needs no
# CMake-level UNIX/WIN32 guard" shape window_facade.cpp/gl_context_
# facade.cpp already use, one directory over each).
#
# Added UNCONDITIONALLY, before the UNIX/WIN32 branch in src/platform/
# CMakeLists.txt - same reasoning port/, window/, input/ and gl/
# already give for their own unconditional add_subdirectory().

target_sources(glintfx_library PRIVATE
    frame_tick_state.cpp
    frame_cap_schedule.cpp
    loop_callbacks_validation.cpp
    loop_impl.cpp
    loop_facade.cpp
)
