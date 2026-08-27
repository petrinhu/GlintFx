# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/render/CMakeLists.txt
#
# GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1, L-19, L-31, L-40).
#
# Single subject: turns the vendored third_party/khronos/gl.xml into
# the generated GL 3.3 core loader (header + source) at build time via
# the gl_registry_codegen HOST tool (tools/gl_registry_codegen/), and
# attaches BOTH the generated .cpp and the hand-written gl_abi.hpp/
# gl_proc_address.hpp to glintfx_library as PRIVATE sources/include
# dirs - mirrors cmake/GlintfxWaylandProtocols.cmake's own idempotent,
# function-based shape (its own header comment explains why:
# generation is a build STEP, not a target property, so it needs to be
# a function callable more than once, not inline code that runs once
# at include() time).
#
# UNLIKE the Wayland binding, this is NOT guarded by if(UNIX): GL 3.3
# core is a WINDOWS target too (GODS_LAWS.md L-31 - EGL on Linux, WGL
# on Windows, ESCOPO.md SS6), and gl.xml's own <feature api="gl">
# resolution does not depend on the host platform at all - the SAME
# generated gl_functions.hpp/.cpp is correct on every one of this
# project's five target platforms; only the calling-convention macro
# inside the hand-written gl_abi.hpp (GLINTFX_GL_APIENTRY) branches at
# COMPILE time by _WIN32, never by anything decided here.

# Computed via CMake's own file(SHA256 ...), not shelled out to
# sha256sum(1) (which may not exist on every platform this project's CI
# runs, e.g. a bare Windows runner) - a single source of truth for the
# rest of this file AND for tests/CMakeLists.txt's own CLI-level
# controls (CACHE INTERNAL makes it visible there; tests/ is add_
# subdirectory()'d after src/, see the root CMakeLists.txt's own
# ordering comment).
file(SHA256 "${PROJECT_SOURCE_DIR}/third_party/khronos/gl.xml" GLINTFX_GL_XML_SHA256)
set(GLINTFX_GL_XML_SHA256 "${GLINTFX_GL_XML_SHA256}" CACHE INTERNAL
    "sha256 of the vendored third_party/khronos/gl.xml, computed once at configure time")

# The value RECORDED in third_party/khronos/README.md (GODS_LAWS.md
# L-07 EXCECAO No 1, obligation 3) - kept as a literal here, not
# derived from the file itself, on purpose: if a future edit to
# gl.xml ever changes its hash without a matching, DELIBERATE update to
# this literal (and to the README's own table), configure fails loud
# instead of silently accepting a file that no longer matches what the
# provenance record promises.
set(GLINTFX_GL_XML_EXPECTED_SHA256
    "fba2eaa6262cededdba0dd3cd1e3b1806c24899a7c5df8158467e41c19969426")
if(NOT GLINTFX_GL_XML_SHA256 STREQUAL GLINTFX_GL_XML_EXPECTED_SHA256)
    message(FATAL_ERROR
        "src/render/CMakeLists.txt: third_party/khronos/gl.xml sha256 is "
        "${GLINTFX_GL_XML_SHA256}, expected ${GLINTFX_GL_XML_EXPECTED_SHA256} "
        "(GODS_LAWS.md L-07 EXCECAO No 1, obrigacao 3 - o arquivo vendorizado "
        "deve permanecer verbatim; se o lider autorizou re-vendorizar, "
        "atualize ESTE literal E a tabela de proveniencia em "
        "third_party/khronos/README.md no MESMO commit)")
endif()

set(GLINTFX_GENERATED_RENDER_DIR "${PROJECT_BINARY_DIR}/generated/render")
set(GLINTFX_GL_XML_PATH "${PROJECT_SOURCE_DIR}/third_party/khronos/gl.xml")

set(GLINTFX_GL_FUNCTIONS_HEADER "${GLINTFX_GENERATED_RENDER_DIR}/gl_functions.hpp")
set(GLINTFX_GL_FUNCTIONS_SOURCE "${GLINTFX_GENERATED_RENDER_DIR}/gl_functions.cpp")

# Idempotent, same pattern as glintfx_generate_xdg_shell_binding(): safe
# to call more than once, the custom command/target are only declared
# the first time.
function(glintfx_generate_gl_functions)
    if(TARGET glintfx_gl_functions_generated)
        return()
    endif()

    add_custom_command(
        OUTPUT "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
        COMMAND "$<TARGET_FILE:gl_registry_codegen>"
            "${GLINTFX_GL_XML_PATH}"
            "${GLINTFX_GENERATED_RENDER_DIR}"
            "3.3"
            "--expect-sha256=${GLINTFX_GL_XML_EXPECTED_SHA256}"
        DEPENDS "${GLINTFX_GL_XML_PATH}" gl_registry_codegen
        COMMENT "gl_registry_codegen: generating GL 3.3 core loader from third_party/khronos/gl.xml"
        VERBATIM
    )

    add_custom_target(glintfx_gl_functions_generated
        DEPENDS "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
    )
endfunction()

# Attaches the generated loader to `target` as PRIVATE (GODS_LAWS.md
# L-19: the public surface does not move for an internal loading
# detail): the generated .cpp becomes one of target's own sources, and
# BOTH the generated directory and src/render/ itself (where the
# hand-written gl_abi.hpp/gl_proc_address.hpp live, which the generated
# header #includes by its own bare filename) become PRIVATE include
# dirs.
function(glintfx_add_generated_gl_loader target)
    glintfx_generate_gl_functions()

    add_dependencies(${target} glintfx_gl_functions_generated)
    target_sources(${target} PRIVATE "${GLINTFX_GL_FUNCTIONS_SOURCE}")
    target_include_directories(${target} PRIVATE
        "${GLINTFX_GENERATED_RENDER_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

glintfx_add_generated_gl_loader(glintfx_library)
