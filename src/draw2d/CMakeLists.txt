# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/draw2d/CMakeLists.txt
#
# R2D-BATCH (docs/plano-w7d.md sec. 4.3, D-W7D-14): the 2D drawing layer, ABOVE platform/gl (GODS_LAWS.md
# L-19: src/draw2d/ includes NOTHING of src/platform/ but the ONE header of the closed list,
# platform/gl/gl_context_access.hpp, which renderer_2d_facade.cpp alone uses, and never a header of the operating system). Every
# file here is a pure atom over plain data, or talks to GL only through the already loaded
# gl_function_table of src/render/, so all of it compiles on all five systems.
#
# The generated loader header (gl_functions.hpp) and gl_abi.hpp reach these translation units through
# the include directories src/render/CMakeLists.txt already gives glintfx_library.

target_sources(glintfx_library PRIVATE
    quad_vertices.cpp
    draw_order.cpp
    triangle_batch.cpp
    frame_report_tally.cpp
    piece_refusal.cpp
    embedded_program.cpp
    gl_state_contract.cpp
    vertex_stream.cpp
    piece_list.cpp
    triangle_store.cpp
    gl_load_refusal.cpp
    srgb_encoding.cpp
    pixel_premultiply.cpp
    texture_request_check.cpp
    mipmap_reduce.cpp
    frame_2d_report.cpp
    renderer_2d_impl.cpp
    renderer_2d_facade.cpp
    texture_upload.cpp
    texture_release.cpp
    texture_2d_facade.cpp
    renderer_2d_facade_textures.cpp
    triangle_list_check.cpp
    renderer_2d_facade_shapes.cpp
    triangle_pixel_vertices.cpp
)

target_include_directories(glintfx_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
