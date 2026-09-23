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
#
# Built UNCONDITIONALLY, even under GL-CODEGEN-HOST-TOOL's "variable"
# or "nested" cross-compile modes below: this static lib is what
# tests/gl_registry_codegen_cli_test.cpp (a HOST-native test binary,
# never cross-built for the same reason gl_registry_codegen itself
# would not run there) links against, independent of which mode
# src/render/CMakeLists.txt ends up generating the GL loader through.

add_library(gl_registry_codegen_lib STATIC
    xml_reader.cpp
    registry_parser.cpp
    loader_codegen.cpp
    sha256.cpp
)
target_include_directories(gl_registry_codegen_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
glintfx_apply_compile_options(gl_registry_codegen_lib)

# GL-CODEGEN-HOST-TOOL (D-11 of docs/plano-fecho-w7b.md,
# cmake/GlintfxGlCodegenHostTool.cmake): the CROSS-compiled
# `gl_registry_codegen` executable is only worth building when the
# resolved mode actually calls it - "native" (not cross-compiling at
# all, the target IS the host tool, UNCHANGED from before this fatia)
# and "emulator" (PATH 2: src/render/CMakeLists.txt's custom command
# names this TARGET so CMake prepends CMAKE_CROSSCOMPILING_EMULATOR to
# it - it has to exist, cross-built, for the emulator to have anything
# to wrap). Modes "variable" (PATH 1, an EXTERNAL host binary) and
# "nested" (PATH 3, a SEPARATE native ExternalProject build of this
# same source, cmake/GlintfxGlCodegenHostTool.cmake's own resolver
# already decided a cross-built copy is useless there) skip it
# entirely - D-11's own promise ("o alvo nativo da ferramenta nao e
# construido").
if(GLINTFX_GL_CODEGEN_HOST_MODE STREQUAL "native" OR
   GLINTFX_GL_CODEGEN_HOST_MODE STREQUAL "emulator")
    # The HOST (or, under "emulator", cross-built-but-emulator-wrapped)
    # executable itself - src/render/CMakeLists.txt's
    # add_custom_command() is the only caller (GODS_LAWS.md L-07
    # EXCECAO No 1: this tool never links into glintfx_library nor is
    # it installed with it).
    add_executable(gl_registry_codegen main.cpp)
    target_link_libraries(gl_registry_codegen PRIVATE gl_registry_codegen_lib)
    glintfx_apply_compile_options(gl_registry_codegen)
endif()
