# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/embed_name_collision/CMakeLists.txt
#
# Single subject: standalone CMake project that proves an embedding
# consumer can name its OWN target `glintfx` without colliding with
# glintfx's internal CMake target (FIX-CONSUMO-2, achado QA-3). Before
# the fix, glintfx's own library target was also the bare `glintfx`
# (CMake target names share one global namespace), so a consumer
# declaring `add_library(glintfx ...)` after embedding glintfx via
# add_subdirectory hit `CMake Error ... policy CMP0002`.
#
# Is NOT included by the main build via add_subdirectory; runs as an
# independent configure/build, passing GLINTFX_SOURCE_DIR at the command
# line, same convention as tests/embed/.

cmake_minimum_required(VERSION 3.28)

project(glintfx_name_collision_consumer LANGUAGES CXX)

if(NOT DEFINED GLINTFX_SOURCE_DIR)
    message(FATAL_ERROR
        "GLINTFX_SOURCE_DIR must be set to the glintfx source tree "
        "(-DGLINTFX_SOURCE_DIR=<path>)")
endif()

add_subdirectory("${GLINTFX_SOURCE_DIR}" glintfx-build)

# The collision under test: a consumer target named exactly `glintfx`,
# the same bare name glintfx's own internal target used to have. This
# line alone is the assertion - if the rename regresses, configure
# fails here with CMake Error ... policy CMP0002, before any of the code
# below even runs.
add_library(glintfx STATIC dummy.cpp)

add_executable(collision_consumer main.cpp)
target_link_libraries(collision_consumer PRIVATE glintfx::glintfx)
set_target_properties(collision_consumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
