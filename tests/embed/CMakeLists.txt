# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/embed/CMakeLists.txt
#
# Single subject: standalone CMake project that proves glintfx works
# embedded via add_subdirectory (FIX-CONSUMO, achado A7). Is NOT included
# by the main build via add_subdirectory; runs as an independent
# configure/build, passing GLINTFX_SOURCE_DIR at the command line so it
# never depends on the tree it lives in.

cmake_minimum_required(VERSION 3.28)

project(glintfx_embed_consumer LANGUAGES CXX)

if(NOT DEFINED GLINTFX_SOURCE_DIR)
    message(FATAL_ERROR
        "GLINTFX_SOURCE_DIR must be set to the glintfx source tree "
        "(-DGLINTFX_SOURCE_DIR=<path>)")
endif()

# The embedding case under test: a consumer that vendors glintfx by
# source, the same way FetchContent would after populating the tree.
# The second argument names the binary subdirectory explicitly so the
# test can assert on it below, mirroring what FetchContent itself does.
add_subdirectory("${GLINTFX_SOURCE_DIR}" glintfx-build)

add_executable(embed_consumer main.cpp)
target_link_libraries(embed_consumer PRIVATE glintfx::glintfx)
set_target_properties(embed_consumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
