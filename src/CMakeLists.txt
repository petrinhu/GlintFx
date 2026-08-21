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
# target_sources(glintfx_library ...) on the target created below. Today
# only core/ exists; the layer that touches the OS is born as the next
# entry in this list, never mixed into the core sources.
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

glintfx_apply_compile_options(glintfx_library)
glintfx_set_target_properties(glintfx_library)
glintfx_generate_export_header(glintfx_library)
glintfx_configure_version_header()
glintfx_set_target_include_dirs(glintfx_library)
glintfx_apply_static_define(glintfx_library)
