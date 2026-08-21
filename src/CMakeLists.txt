# src/CMakeLists.txt
#
# Single subject: the glintfx library target and the layers that add
# sources to it. Properties, export header and include dirs come from
# GlintfxLibrary.cmake; warnings and language standard come from
# GlintfxCompileOptions.cmake; neither is redefined here.
#
# Each layer (GODS_LAWS.md L-19) is a visible CMake unit: its own
# add_subdirectory(), whose CMakeLists.txt only does
# target_sources(glintfx ...) on the target created below. Today only
# core/ exists; the layer that touches the OS is born as the next
# entry in this list, never mixed into the core sources.

add_library(glintfx)
add_library(glintfx::glintfx ALIAS glintfx)

add_subdirectory(core)

glintfx_apply_compile_options(glintfx)
glintfx_set_target_properties(glintfx)
glintfx_generate_export_header(glintfx)
glintfx_configure_version_header()
glintfx_set_target_include_dirs(glintfx)
glintfx_apply_static_define(glintfx)
