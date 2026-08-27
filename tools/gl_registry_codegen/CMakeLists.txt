# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/gl_registry_codegen/CMakeLists.txt
#
# Single subject: the gl_registry_codegen HOST tool (GODS_LAWS.md L-07
# EXCECAO No 1) and the object library its own test suite links
# against. This tool is NEVER linked into glintfx_library or installed
# with it - it runs once, at build time, to turn the vendored
# third_party/khronos/gl.xml into generated C++ (src/render/
# CMakeLists.txt is the ONLY caller of the executable target below).
#
# gl_registry_codegen_lib is a plain STATIC library, not glintfx's own
# glintfx_add_test() macro (cmake/GlintfxTest.cmake): that macro links
# every case against glintfx::glintfx, which this tool's own tests have
# no reason to need - they exercise XML parsing and text generation,
# never anything inside the shipped library.

add_library(gl_registry_codegen_lib STATIC
    xml_reader.cpp
    registry_parser.cpp
    loader_codegen.cpp
    sha256.cpp
)
target_include_directories(gl_registry_codegen_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
glintfx_apply_compile_options(gl_registry_codegen_lib)

# The HOST executable itself - src/render/CMakeLists.txt's
# add_custom_command() is the only caller (GODS_LAWS.md L-07 EXCECAO
# No 1: this tool never links into glintfx_library nor is it
# installed with it).
add_executable(gl_registry_codegen main.cpp)
target_link_libraries(gl_registry_codegen PRIVATE gl_registry_codegen_lib)
glintfx_apply_compile_options(gl_registry_codegen)
