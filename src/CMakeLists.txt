# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/CMakeLists.txt
#
# Single subject: the glintfx library target and the layers that add
# sources to it. Properties, export header and include dirs come from
# GlintfxLibrary.cmake; warnings and language standard come from
# GlintfxCompileOptions.cmake; neither is redefined here.
#
# Each layer (GODS_LAWS.md L-19) is a visible CMake unit: its own
# add_subdirectory(), whose CMakeLists.txt only does
# target_sources(glintfx_library ...) on the target created below.
# platform/ (UNIX only for now, WL-PROTO) is the layer that touches the
# OS - kept strictly separate from core/'s add_subdirectory() below, per
# GODS_LAWS.md L-19 item 1 ("a single layer touches the OS") and the
# check_layers.py gate that reproves core/ reaching into it.
#
# The CMake target is named `glintfx_library`, not the bare `glintfx`
# (FIX-CONSUMO-2, achado QA-3): a consumer that embeds glintfx via
# add_subdirectory/FetchContent and happens to name one of its OWN
# targets `glintfx` collided with ours (CMake target names share one
# global namespace; CMake Error, policy CMP0002). The alias below is the
# only name a consumer is ever meant to reference (GODS_LAWS.md L-19/
# L-26: `glintfx::glintfx` is the public contract, unchanged); the bare
# `glintfx_library` name is this build's internal implementation detail,
# and GlintfxLibrary.cmake's EXPORT_NAME property keeps the INSTALLED
# package's imported target `glintfx::glintfx` too, not
# `glintfx::glintfx_library`.
add_library(glintfx_library)
add_library(glintfx::glintfx ALIAS glintfx_library)

add_subdirectory(core)

# gfss layer (GFSS-TOKEN, GODS_LAWS.md L-19/L-28): pure core, same
# reason core/ above has no OS header - added as its own
# add_subdirectory() rather than folded into core/'s (GODS_LAWS.md
# L-17: a new subject gets its own module, never bolted onto an
# existing one "because it fits").
add_subdirectory(gfss)

# gfui layer (GFSS-NODE-VIEW, GODS_LAWS.md L-17/L-19/L-28): the
# motor's own module - see src/gfui/CMakeLists.txt's own header
# comment for why this is its own add_subdirectory() rather than
# folded into gfss/'s (GODS_LAWS.md L-17: a new subject gets its own
# module, never bolted onto an existing one "because it fits").
add_subdirectory(gfui)

# render layer (GL-LOADER, GODS_LAWS.md L-07 EXCECAO No 1/L-19/L-31):
# NOT guarded by if(UNIX) - GL 3.3 core is a Windows target too (WGL),
# unlike the Wayland platform/ backend below. See src/render/
# CMakeLists.txt's own header comment for the full reasoning.
add_subdirectory(render)

# draw2d layer (R2D-BATCH, GODS_LAWS.md L-19): above platform/gl, never included by platform/. Added after
# render/ because the layer talks GL through the generated loader header render/ produces.
add_subdirectory(draw2d)

# Unconditional as of ARCH-PORTS (GODS_LAWS.md L-19/L-40): the
# UNIX/WIN32/unsupported branching that used to live in THIS if(UNIX)
# guard now lives inside src/platform/CMakeLists.txt itself, as a
# three-state selection (UNIX selects Wayland; WIN32 is a declared
# absence pending WIN-WINDOW; any other platform is a loud
# FATAL_ERROR at configure time, never a silently empty platform
# layer) - see that file's own header comment. Net effect on the two
# platforms this project's CI already builds is UNCHANGED: Linux still
# gets the Wayland backend, Windows still compiles glintfx with no
# platform/ sources at all (WL-PROTO's original if(UNIX) fact, now
# enforced one level deeper, with a printed reason instead of silent
# absence).
add_subdirectory(platform)

# glintfx_generated_sources (D-CI2-LINT, errata sec. 15): ONE target that depends on EVERY generator of a
# source the library includes but does not track - so a job that only CONFIGURES (the Windows Lint, whose
# clang-tidy reads compile_commands.json and never builds) can build exactly this before the tidy and give
# it the headers the build would. A NEW generator joins this list in the SAME commit that creates it.
#   glintfx_gl_functions_generated      - gl_functions.hpp/.cpp (src/render/, every platform)
#   glintfx_wayland_xdg_shell_binding   - the xdg-shell binding (only where Wayland is built)
add_custom_target(glintfx_generated_sources)
foreach(_glintfx_generator IN ITEMS glintfx_gl_functions_generated glintfx_wayland_xdg_shell_binding)
    if(TARGET ${_glintfx_generator})
        add_dependencies(glintfx_generated_sources ${_glintfx_generator})
    endif()
endforeach()

glintfx_apply_compile_options(glintfx_library)
glintfx_set_target_properties(glintfx_library)
glintfx_apply_export_map(glintfx_library)
glintfx_generate_export_header(glintfx_library)
glintfx_configure_version_header()
glintfx_set_target_include_dirs(glintfx_library)
glintfx_apply_static_define(glintfx_library)
glintfx_colocate_embedded_runtime_dll(glintfx_library)
