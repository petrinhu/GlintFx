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

# GL-CODEGEN-HOST-TOOL (D-11 of docs/plano-fecho-w7b.md,
# cmake/GlintfxGlCodegenHostTool.cmake): sets up the nested,
# host-native build of THIS SAME source tree - PATH 3 of the four-path
# resolution, only reached when neither an explicit
# GLINTFX_GL_CODEGEN_EXECUTABLE nor a CMAKE_CROSSCOMPILING_EMULATOR
# were available. Split into its own function (not inlined into
# glintfx_generate_gl_functions() below) because the nested build's own
# BINARY_DIR is computed once here, read back by the
# add_custom_command() that actually invokes the tool it produces.
#
# NOT ExternalProject_Add() (GODS_LAWS.md L-07, dep_zero_test's own
# reprova, caught live 23/09/2026 by the team lead re-running the full
# suite this fatia's first draft skipped): that module is the classic
# "pull a dependency from outside" mechanism the L-07 gate exists to
# forbid, categorically, with zero exception for "it is really just
# ourselves" - the gate does not, and should not, special-case that.
# The fix is the plain CMake primitives below: an `add_custom_target()`
# with no OUTPUT (unconditionally out-of-date, the exact "rebuild every
# time" LLVM's own native-tool build already needs - see this file's
# own BUILD_ALWAYS-era comment, still true) that shells out to
# "${CMAKE_COMMAND}" on our OWN tools/gl_registry_codegen/ subtree -
# nothing pulled from anywhere, the SAME two-process shape
# ExternalProject_Add() used under the hood, just spelled out instead
# of imported from a CMake module this project's own L-07 forbids.
function(glintfx_setup_nested_gl_codegen_host_build out_tool_path_var out_depend_target_var)
    set(_nested_bin "${PROJECT_BINARY_DIR}/gl-codegen-host-nested-build")

    if(NOT TARGET glintfx_gl_codegen_host_nested)
        # CC/CXX cleared for BOTH the nested configure and the nested
        # build step (D-11: "variaveis de ambiente CC/CXX do cruzado
        # limpas para a sub-construcao") - this OUTER process may
        # itself be running under CC/CXX exported to the CROSS
        # compiler (the toolchain file's own IF(NOT DEFINED ENV{CXX})
        # guard, /usr/share/mingw/toolchain-mingw64.cmake, means a
        # packager who happens to have CXX set in their environment
        # would otherwise poison the nested, supposedly-native build
        # too). The explicit -DCMAKE_CXX_COMPILER pins the resolved
        # host compiler on top - belt and suspenders, not redundant:
        # the env clear stops ambient contamination the nested
        # project's OWN unrelated find_package() calls might otherwise
        # read; the -D pin is what actually selects the compiler this
        # build uses.
        #
        # No OUTPUT (a bare add_custom_target(), never
        # add_custom_command(OUTPUT ...)): unconditionally considered
        # out of date by both Ninja and Make, so it reruns on EVERY
        # build - the LLVM dor this whole PATH answers
        # (llvm-project#125402, cited in this module's own header): a
        # nested, separate cmake process has no way to see file-level
        # DEPENDS across the process boundary the way an in-tree
        # add_custom_command() can, so there is no way to know from OUT
        # HERE whether gl.xml or this tool's own .cpp files changed
        # since the nested build's last run - re-running the (fast,
        # incremental on its OWN side) nested cmake/ninja invocation
        # every time is the only way this stays correct. Any target
        # that DEPENDS on this one (below) inherits that same
        # always-rebuild behavior, because a dependency on a
        # never-up-to-date target is itself never up to date.
        add_custom_target(glintfx_gl_codegen_host_nested
            COMMAND "${CMAKE_COMMAND}" -E env --unset=CC --unset=CXX
                "${CMAKE_COMMAND}"
                    -S "${PROJECT_SOURCE_DIR}"
                    -B "${_nested_bin}"
                    -G "${CMAKE_GENERATOR}"
                    "-DCMAKE_BUILD_TYPE=Release"
                    "-DCMAKE_CXX_COMPILER=${GLINTFX_GL_CODEGEN_RESOLVED_HOST_CXX_COMPILER}"
                    "-DGLINTFX_BUILD_TESTS=OFF"
                    "-DGLINTFX_INSTALL=OFF"
            COMMAND "${CMAKE_COMMAND}" -E env --unset=CC --unset=CXX
                "${CMAKE_COMMAND}" --build "${_nested_bin}" --target gl_registry_codegen
            COMMENT "gl_registry_codegen (nested native build): configuring and building the HOST tool"
            USES_TERMINAL
            VERBATIM
        )
    endif()

    # CMAKE_HOST_WIN32, NOT WIN32 (bug caught live, H-3's own first red
    # run): WIN32 reflects the OUTER, cross-compiling configuration's
    # TARGET (true here, we are cross-building FOR Windows) - the
    # nested build below is never cross-compiled, it always targets
    # THIS machine, so its own executable only gets a .exe suffix when
    # the HOST this configure is running on is Windows, which
    # CMAKE_HOST_WIN32 is the one variable that actually says.
    if(CMAKE_HOST_WIN32)
        set(_nested_tool_path "${_nested_bin}/tools/gl_registry_codegen/gl_registry_codegen.exe")
    else()
        set(_nested_tool_path "${_nested_bin}/tools/gl_registry_codegen/gl_registry_codegen")
    endif()

    set("${out_tool_path_var}" "${_nested_tool_path}" PARENT_SCOPE)
    set("${out_depend_target_var}" "glintfx_gl_codegen_host_nested" PARENT_SCOPE)
endfunction()

# Idempotent, same pattern as glintfx_generate_xdg_shell_binding(): safe
# to call more than once, the custom command/target are only declared
# the first time.
#
# Branches on GLINTFX_GL_CODEGEN_HOST_MODE
# (cmake/GlintfxGlCodegenHostTool.cmake, resolved once in the ROOT
# CMakeLists.txt before add_subdirectory(tools/gl_registry_codegen) or
# add_subdirectory(src) ever run) - GL-CODEGEN-HOST-TOOL's whole point
# is that WHICH binary this add_custom_command() invokes, and HOW it
# names it, differs per mode; everything else about the generated
# OUTPUT is identical across all four (H-1/H-3's own fechamento: byte
# for byte the same artifact as the native build produces).
function(glintfx_generate_gl_functions)
    if(TARGET glintfx_gl_functions_generated)
        return()
    endif()

    set(_glintfx_codegen_args
        "${GLINTFX_GL_XML_PATH}"
        "${GLINTFX_GENERATED_RENDER_DIR}"
        "3.3"
        "--expect-sha256=${GLINTFX_GL_XML_EXPECTED_SHA256}"
    )

    if(GLINTFX_GL_CODEGEN_HOST_MODE STREQUAL "variable")
        # PATH 1: the caller-supplied, ALREADY host-native binary -
        # cmake/GlintfxGlCodegenHostTool.cmake already validated it
        # exists and passed the --codegen-abi handshake at configure
        # time (H-1). Invoked by its literal path, never through
        # CMAKE_CROSSCOMPILING_EMULATOR (D-11: it is ALREADY
        # host-native, wrapping it in an emulator would be wrong even
        # when one happens to be configured).
        add_custom_command(
            OUTPUT "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
            COMMAND "${GLINTFX_GL_CODEGEN_EXECUTABLE}" ${_glintfx_codegen_args}
            DEPENDS "${GLINTFX_GL_XML_PATH}" "${GLINTFX_GL_CODEGEN_EXECUTABLE}"
            COMMENT "gl_registry_codegen (GLINTFX_GL_CODEGEN_EXECUTABLE): generating GL 3.3 core loader"
            VERBATIM
        )
    elseif(GLINTFX_GL_CODEGEN_HOST_MODE STREQUAL "emulator")
        # PATH 2: naming the bare TARGET (not $<TARGET_FILE:...>) is
        # the ENTIRE fix (H-2's own estreia vermelha proves the OLD
        # $<TARGET_FILE:...> form never triggers the emulator at all) -
        # CMake's own add_custom_command() documentation: "If COMMAND
        # specifies an executable target name, it is automatically
        # replaced by the location of the executable target [...] IF
        # CROSSCOMPILING_EMULATOR is set".
        add_custom_command(
            OUTPUT "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
            COMMAND gl_registry_codegen ${_glintfx_codegen_args}
            DEPENDS "${GLINTFX_GL_XML_PATH}" gl_registry_codegen
            COMMENT "gl_registry_codegen (CMAKE_CROSSCOMPILING_EMULATOR): generating GL 3.3 core loader"
            VERBATIM
        )
    elseif(GLINTFX_GL_CODEGEN_HOST_MODE STREQUAL "nested")
        # PATH 3: the tool this build actually runs comes from a
        # SEPARATE, nested, native build of this same checkout -
        # glintfx_setup_nested_gl_codegen_host_build() above sets that
        # up and hands back the path the finished binary lands at, plus
        # the nested build target name to order against.
        glintfx_setup_nested_gl_codegen_host_build(_nested_tool_path _nested_depend_target)
        add_custom_command(
            OUTPUT "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
            COMMAND "${_nested_tool_path}" ${_glintfx_codegen_args}
            DEPENDS "${GLINTFX_GL_XML_PATH}" "${_nested_depend_target}"
            COMMENT "gl_registry_codegen (nested native build): generating GL 3.3 core loader"
            VERBATIM
        )
    else()
        # "native" (not cross-compiling at all) - UNCHANGED from before
        # this fatia, the exact form H-1's own fechamento requires stay
        # byte-for-byte identical.
        add_custom_command(
            OUTPUT "${GLINTFX_GL_FUNCTIONS_HEADER}" "${GLINTFX_GL_FUNCTIONS_SOURCE}"
            COMMAND "$<TARGET_FILE:gl_registry_codegen>" ${_glintfx_codegen_args}
            DEPENDS "${GLINTFX_GL_XML_PATH}" gl_registry_codegen
            COMMENT "gl_registry_codegen: generating GL 3.3 core loader from third_party/khronos/gl.xml"
            VERBATIM
        )
    endif()

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
