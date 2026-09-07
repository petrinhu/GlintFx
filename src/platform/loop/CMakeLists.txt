# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/loop/CMakeLists.txt
#
# Single subject: LOOP-RUN's fatia 6a own pure atoms (docs/plano-w6b-
# fatias-6-8.md sec. 8.1) - frame_tick_state.cpp (D-W6b-42/44/45),
# frame_cap_schedule.cpp (D-W6b-49) and loop_callbacks_validation.cpp
# (D-W6b-43). None of the three reaches an operating-system header
# (each is a pure atom over plain data, the same "gl/ added
# unconditionally" reasoning src/platform/gl/CMakeLists.txt's own
# header comment already documents one directory over) - the facade
# that actually PIMPLs gltfx_loop over these (loop_impl.hpp/loop_
# facade.cpp) lands in a later fatia (6b), once fatia 7's own ports
# exist for it to call.
#
# Added UNCONDITIONALLY, before the UNIX/WIN32 branch in src/platform/
# CMakeLists.txt - same reasoning port/, window/, input/ and gl/
# already give for their own unconditional add_subdirectory().

target_sources(glintfx_library PRIVATE
    frame_tick_state.cpp
    frame_cap_schedule.cpp
    loop_callbacks_validation.cpp
)
