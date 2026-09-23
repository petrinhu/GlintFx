# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxGlCodegenHostTool.cmake
#
# Single subject (GODS_LAWS.md L-17): GL-CODEGEN-HOST-TOOL (TODO.md,
# D-11 of docs/plano-fecho-w7b.md) - resolves, at CONFIGURE time, which
# of four ways src/render/CMakeLists.txt's `gl_registry_codegen` HOST
# tool gets to run when the build itself is CROSS-compiling glintfx.
#
# THE DOR THIS FILE EXISTS FOR (measured 06/09/2026, re-confirmed
# 23/09/2026, H-0 of docs/plano-fecho-w7b.md): `gl_registry_codegen` is
# a build-time code generator that must run ON THE MACHINE DOING THE
# BUILD, producing input the COMPILER then reads - the exact "compile
# time tool" category the CMake discourse thread cited below names.
# When CMAKE_CXX_COMPILER is a CROSS compiler (this project's own
# proof: /usr/share/mingw/toolchain-mingw64.cmake, the literal Fedora
# packager scenario), the `gl_registry_codegen` target
# tools/gl_registry_codegen/CMakeLists.txt builds is ALSO cross-built -
# a Windows .exe this Linux host cannot execute
# ("/bin/sh: ... impossivel executar o arquivo binario: Erro no
# formato exec", exit code 126, H-0's own measured red). CMake's own
# manual promises an emulator gets prepended automatically ONLY when
# COMMAND names a target by its bare NAME, not the
# `$<TARGET_FILE:...>` generator expression src/render/CMakeLists.txt
# used before this file existed - so even setting
# CMAKE_CROSSCOMPILING_EMULATOR alone would not have fixed this.
#
# FOUR PATHS, IN THIS ORDER, MATCHING WHAT THE ECOSYSTEM ALREADY
# SETTLED ON (D-11, sources in this file's own comments below, not
# re-invented here):
#   1. GLINTFX_GL_CODEGEN_EXECUTABLE (cache, explicit path to an
#      ALREADY-BUILT host-native binary) - checked in BOTH build
#      modes, native and cross (the protobuf dor: a tool-path variable
#      silently ignored in one mode -
#      https://github.com/protocolbuffers/protobuf/issues/14576).
#   2. CMAKE_CROSSCOMPILING_EMULATOR, CMake's own standard escape
#      hatch (Wine, qemu-user, etc.) - reached by naming the TARGET,
#      not $<TARGET_FILE:...>, in the generated add_custom_command().
#   3. A nested, native build of this SAME source tree - a SEPARATE
#      `cmake -S/-B` plus `cmake --build` invocation over our OWN
#      tools/gl_registry_codegen/ subtree (src/render/CMakeLists.txt's
#      own glintfx_setup_nested_gl_codegen_host_build(), an
#      add_custom_target() with no OUTPUT, unconditionally rebuilt),
#      NOT CMake's ExternalProject module (GODS_LAWS.md L-07 forbids
#      it categorically - dep_zero_test caught the first draft's
#      ExternalProject_Add() live, 23/09/2026, and that module's own
#      mechanics are the plain `cmake -S/-B`/`--build` pair this file
#      spells out by hand instead). The LLVM precedent this path
#      follows: https://llvm.org/docs/CMake.html builds TableGen
#      natively on the side when no native tool/emulator was given;
#      its own dor - https://github.com/llvm/llvm-project/issues/125402
#      - is untracked incremental dependencies across the nested
#      process boundary, which is why the nested target always
#      reruns, not an optimization left for later.
#   4. A loud, named FATAL_ERROR at CONFIGURE time - never the
#      "Exec format error" mid-build H-0 measured. Every message this
#      file raises names GLINTFX_GL_CODEGEN_EXECUTABLE explicitly: it
#      is the one escape hatch that always works, whatever else this
#      machine is missing.
#
# TWO GUARANTEES BORN FROM OTHER PROJECTS' PAIN, NOT THIS PROJECT'S
# CONVENIENCE (D-11):
#   - ABI handshake: `gl_registry_codegen --codegen-abi` (main.cpp)
#     prints a bare integer this file compares against
#     GLINTFX_GL_CODEGEN_ABI_EXPECTED below - the protobuf/onnxruntime
#     dor of a version-mismatched tool silently generating incompatible
#     code (https://github.com/protocolbuffers/protobuf/issues/14576,
#     https://github.com/microsoft/onnxruntime/issues/8413). Only
#     meaningful, and only run, for PATH 1 (an externally-supplied
#     binary this file did not just build from the current checkout) -
#     paths 2/3/4 build (or fail to build) the tool from THIS
#     checkout's own source, so there is nothing to mismatch.
#   - Nothing of this leaks into the installed package (the Qt 6 dor:
#     QT_HOST_PATH became a requirement even a NATIVE consumer had to
#     satisfy - https://github.com/conda-forge/qt-main-feedstock/issues/273):
#     none of the variables this file reads or sets are referenced by
#     cmake/glintfx-config.cmake.in or cmake/glintfx.pc.in, and
#     tests/tools/check_gl_codegen_host_leak.py (H-5) proves it stays
#     that way.
#
# https://cmake.org/cmake/help/latest/command/add_custom_command.html
# https://discourse.cmake.org/t/building-compile-time-tools-when-cross-compiling/601
# https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html

# The OTHER half of the double-key literal
# tools/gl_registry_codegen/loader_codegen.hpp's own
# `codegen_abi_version` carries (see that header's comment for why
# this is a literal here too, not derived from the tool binary this
# very file might not even be able to run yet). Bump BOTH in the SAME
# commit if the generated file FORMAT ever changes incompatibly.
set(GLINTFX_GL_CODEGEN_ABI_EXPECTED "1")

# The single, named way out of every failure path below - GLINTFX_GL_
# CODEGEN_EXECUTABLE always works, whatever else is missing (a
# pre-built host binary from ANY prior native build of this exact
# checkout, including one this machine already has lying around).
function(glintfx_fail_no_gl_codegen_host_tool)
    # ${ARGN}, joined, NOT a single positional parameter (bug caught
    # live, H-4's own first red run): CMake does not auto-concatenate
    # adjacent quoted-string ARGUMENTS at a call site the way C++ does
    # adjacent string LITERALS - "a" "b" passed to a function is TWO
    # distinct list elements, and a one-parameter function silently
    # drops everything past the first. list(JOIN) makes every call
    # site's multi-line message literal actually reach this point
    # whole, instead of truncated to its own first line.
    list(JOIN ARGN "" reason)
    message(FATAL_ERROR
        "GlintfxGlCodegenHostTool: nao foi possivel resolver a ferramenta HOST "
        "gl_registry_codegen para esta construcao cruzada (${reason}). Aponte "
        "-DGLINTFX_GL_CODEGEN_EXECUTABLE=<caminho para um binario HOST ja "
        "construido deste MESMO checkout> (GODS_LAWS.md L-40: nunca o \"Exec "
        "format error\" no meio da construcao - D-11 de docs/plano-fecho-w7b.md).")
endfunction()

# PATH 1's own ABI handshake, isolated so H-1's own estreia vermelha
# (a stub binary with a MISMATCHED --codegen-abi) exercises exactly
# this function, nothing else in the resolution chain.
function(glintfx_check_gl_codegen_host_tool_abi tool_path)
    execute_process(
        COMMAND "${tool_path}" --codegen-abi
        OUTPUT_VARIABLE _glintfx_codegen_abi_actual
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _glintfx_codegen_abi_probe_result
    )
    if(NOT _glintfx_codegen_abi_probe_result EQUAL 0)
        message(FATAL_ERROR
            "GlintfxGlCodegenHostTool: GLINTFX_GL_CODEGEN_EXECUTABLE "
            "('${tool_path}') nao respondeu a --codegen-abi (codigo de saida "
            "${_glintfx_codegen_abi_probe_result}) - nao roda neste hospedeiro, ou "
            "nao e o gl_registry_codegen esperado")
    endif()
    if(NOT _glintfx_codegen_abi_actual STREQUAL GLINTFX_GL_CODEGEN_ABI_EXPECTED)
        message(FATAL_ERROR
            "GlintfxGlCodegenHostTool: GLINTFX_GL_CODEGEN_EXECUTABLE "
            "('${tool_path}') respondeu --codegen-abi=${_glintfx_codegen_abi_actual}, "
            "esperado ${GLINTFX_GL_CODEGEN_ABI_EXPECTED} - a ferramenta de "
            "hospedeiro esta desalinhada com este checkout (D-11 de "
            "docs/plano-fecho-w7b.md: aperto de mao de versao reprovando ANTES de "
            "gerar codigo incompativel, a dor do protobuf/onnxruntime)")
    endif()
endfunction()

# The one entry point this whole file exists to provide. Called from
# the ROOT CMakeLists.txt, BEFORE add_subdirectory(tools/gl_registry_codegen)
# - tools/gl_registry_codegen/CMakeLists.txt itself reads
# GLINTFX_GL_CODEGEN_HOST_MODE to decide whether the CROSS-built
# `gl_registry_codegen` executable target is worth building at all
# (only PATH 2/emulator needs it; PATH 1/variable and PATH 3/nested
# both resolve to a HOST-native binary this project never asks the
# cross toolchain to produce).
#
# Idempotent by construction (GLINTFX_GL_CODEGEN_HOST_MODE is a CACHE
# INTERNAL variable): a second call in the SAME configure, or a
# reconfigure that reuses the cache, is a no-op - this matters because
# add_subdirectory(tools/gl_registry_codegen) and
# src/render/CMakeLists.txt's own glintfx_generate_gl_functions() both
# need the resolved mode, and neither should re-run execute_process()
# (PATH 1's ABI probe) a second time.
function(glintfx_resolve_gl_codegen_host_mode)
    if(DEFINED GLINTFX_GL_CODEGEN_HOST_MODE)
        return()
    endif()

    if(NOT CMAKE_CROSSCOMPILING)
        # Native build: UNCHANGED from before this file existed - H-1's
        # own fechamento requires it stay the SAME artifact as before.
        set(GLINTFX_GL_CODEGEN_HOST_MODE "native" CACHE INTERNAL
            "How the src/render GL loader codegen HOST tool is resolved (GL-CODEGEN-HOST-TOOL)")
        return()
    endif()

    # ---- PATH 1: explicit, already-built host binary ----
    if(GLINTFX_GL_CODEGEN_EXECUTABLE)
        if(NOT EXISTS "${GLINTFX_GL_CODEGEN_EXECUTABLE}")
            glintfx_fail_no_gl_codegen_host_tool(
                "GLINTFX_GL_CODEGEN_EXECUTABLE aponta para "
                "'${GLINTFX_GL_CODEGEN_EXECUTABLE}', que nao existe")
        endif()
        glintfx_check_gl_codegen_host_tool_abi("${GLINTFX_GL_CODEGEN_EXECUTABLE}")
        set(GLINTFX_GL_CODEGEN_HOST_MODE "variable" CACHE INTERNAL
            "How the src/render GL loader codegen HOST tool is resolved (GL-CODEGEN-HOST-TOOL)")
        return()
    endif()

    # ---- PATH 2: CMake's own cross-compiling emulator ----
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(GLINTFX_GL_CODEGEN_HOST_MODE "emulator" CACHE INTERNAL
            "How the src/render GL loader codegen HOST tool is resolved (GL-CODEGEN-HOST-TOOL)")
        return()
    endif()

    # ---- PATH 3: nested native build of this same checkout ----
    # GLINTFX_HOST_CXX_COMPILER is OPTIONAL (D-11): when absent, this
    # file itself looks for a host compiler on PATH - safe to do with a
    # plain find_program() because the mingw toolchain file this
    # project's own D-12 pins for cross-compile proof sets
    # CMAKE_FIND_ROOT_PATH_MODE_PROGRAM to NEVER (measured,
    # /usr/share/mingw/toolchain-mingw64.cmake), i.e. find_program()
    # NEVER resolves into the target sysroot - it only ever finds HOST
    # programs, which is exactly what this path needs.
    if(GLINTFX_HOST_CXX_COMPILER)
        if(NOT EXISTS "${GLINTFX_HOST_CXX_COMPILER}")
            glintfx_fail_no_gl_codegen_host_tool(
                "GLINTFX_HOST_CXX_COMPILER aponta para "
                "'${GLINTFX_HOST_CXX_COMPILER}', que nao existe")
        endif()
        set(_glintfx_resolved_host_cxx "${GLINTFX_HOST_CXX_COMPILER}")
    else()
        find_program(GLINTFX_GL_CODEGEN_DETECTED_HOST_CXX NAMES c++ g++ clang++)
        if(NOT GLINTFX_GL_CODEGEN_DETECTED_HOST_CXX)
            glintfx_fail_no_gl_codegen_host_tool(
                "nenhum compilador C++ de hospedeiro (c++/g++/clang++) foi encontrado no "
                "PATH, e GLINTFX_HOST_CXX_COMPILER nao foi definida")
        endif()
        set(_glintfx_resolved_host_cxx "${GLINTFX_GL_CODEGEN_DETECTED_HOST_CXX}")
    endif()

    set(GLINTFX_GL_CODEGEN_RESOLVED_HOST_CXX_COMPILER "${_glintfx_resolved_host_cxx}" CACHE INTERNAL
        "Host C++ compiler the nested gl_registry_codegen native build uses (GL-CODEGEN-HOST-TOOL)")
    set(GLINTFX_GL_CODEGEN_HOST_MODE "nested" CACHE INTERNAL
        "How the src/render GL loader codegen HOST tool is resolved (GL-CODEGEN-HOST-TOOL)")
endfunction()
