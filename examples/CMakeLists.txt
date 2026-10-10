# SPDX-License-Identifier: AGPL-3.0-or-later
#
# examples/CMakeLists.txt
#
# Single subject: the list of example subdirectories. Inside the glintfx build (GLINTFX_BUILD_EXAMPLES
# ON, the default only when glintfx is the top-level project, see cmake/GlintfxOptions.cmake), the
# glintfx::glintfx target already exists, and warnings, sanitizer and the output directory of every
# example are applied by the glintfx root (cmake/GlintfxExamples.cmake), never from here. OUTSIDE the
# tree (this directory configured on its own against an installed glintfx, D-W8-86), the branch below
# is the project and finds the package; the CMake and package versions follow the consumer of
# tests/package/CMakeLists.txt.
if(NOT TARGET glintfx::glintfx)
    cmake_minimum_required(VERSION 3.28)
    project(glintfx_examples LANGUAGES CXX)
    find_package(glintfx 0.6 CONFIG REQUIRED)
endif()

add_subdirectory(first_window)
