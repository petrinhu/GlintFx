# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/draw2d/CMakeLists.txt
#
# R2D-BATCH (docs/plano-w7d.md sec. 4.3, D-W7D-14): the 2D drawing layer, ABOVE platform/gl (GODS_LAWS.md
# L-19: src/draw2d/ never includes src/platform/, and never a header of the operating system). Every
# file here is a pure atom over plain data, or talks to GL only through the already loaded
# gl_function_table of src/render/, so all of it compiles on all five systems.
#
# The generated loader header (gl_functions.hpp) and gl_abi.hpp reach these translation units through
# the include directories src/render/CMakeLists.txt already gives glintfx_library.

target_sources(glintfx_library PRIVATE
    quad_vertices.cpp
    draw_order.cpp
)

target_include_directories(glintfx_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
