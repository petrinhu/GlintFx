# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxCompileOptions.cmake
#
# Single subject: the C++23 language standard, the warning flags and the
# sanitizer flags applied to a glintfx target (library, test harness or
# test executable). No user option and no install property belongs here;
# that is GlintfxOptions.cmake and GlintfxLibrary.cmake.

function(glintfx_apply_cxx_standard target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

function(glintfx_apply_warning_flags target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
    if(GLINTFX_WERROR)
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
        )
    endif()
endfunction()

# GODS_LAWS.md L-23 portao 2: ASan/UBSan is applied to EVERY glintfx
# target through this same function, not just the library - a sanitized
# shared library requires its own runtime present in the process, so the
# test harness and test executables linking against it must be built
# with the identical -fsanitize set, or the mix aborts at load/link time.
# No-op (GLINTFX_SANITIZE empty) is the default: this function costs
# nothing in the normal Release/local build.
#
# GATE-ASAN-HALT (GODS_LAWS.md L-40, piso de varredura nao-vazia,
# aplicado aqui ao caso "o portao roda e nao reprova nada"): most UBSan
# checks are RECOVERABLE by default - the process prints the diagnostic
# to stderr and keeps running, exiting 0 if nothing else crashes it.
# Measured live before this line existed: a throwaway signed-integer-
# overflow program built with exactly the flags below MINUS
# -fno-sanitize-recover printed the runtime error and exited status 0.
# -fno-sanitize-recover=all is the compile-time switch documented by
# both GCC and Clang that turns every sanitizer finding (ASan's too,
# belt-and-suspenders - ASan is fatal by default already, but this
# removes the dependency on that default never changing) into a hard
# exit, so `ctest`'s pass/fail reading of the process exit code stops
# lying. This is NOT redundant with runtime ASAN_OPTIONS/UBSAN_OPTIONS
# halt_on_error=1: an environment variable has to be set correctly at
# every single invocation site (this repo's tools/preci.sh, CI, any
# future caller, a developer's own shell) to take effect, while a
# compile flag baked into the binary cannot be forgotten by whoever
# runs it next.
#
# LIMIT this fix does NOT cover, and does not claim to (GODS_LAWS.md
# L-40 "portao que promete o que nao entrega e como o que estamos
# consertando"): undefined behavior that happens INSIDE the system's
# math library (libm) is NOT instrumented by ASan/UBSan compiled into
# glintfx's own translation units - the sanitizer only sees code it
# compiled, and libm ships as a prebuilt system library. A UB finding
# there stays invisible to this gate before and after this change, and
# needs its own dedicated test (e.g. a valgrind/libm-specific check),
# not a stronger flag on our own compile.
#
# MSVC guard, declared downgrade: this project's sanitizer CI job builds
# on Fedora with GNU/Clang only (see .github/workflows/ci.yml); MSVC's
# /fsanitize=address does not share GCC/Clang's -fsanitize=<comma-list>
# syntax and is out of scope for this slice.
#
# SAN-PARITY-WIN (TODO.md; achado de 03/09/2026): the guard above ONLY
# arms the sanitizer flags under GNU/Clang - before this fatal_error,
# -DGLINTFX_SANITIZE=address on an MSVC configure was silently
# ACCEPTED and produced a build with no sanitizer instrumentation at
# all, the exact "portao mudo" GODS_LAWS.md L-40 forbids: whoever asked
# for the protection got a plain build instead, with no diagnostic
# telling them why. This function now REFUSES that combination at
# configure time instead: MSVC plus a non-empty GLINTFX_SANITIZE is a
# fatal configure error, naming what was requested, that this compiler
# is not supported by this project's current sanitizer plumbing, and
# which TODO.md item (SAN-PARITY-WIN) tracks adding MSVC's own
# /fsanitize=address support - that support is NOT implemented here,
# deliberately, as a separate fatia.
function(glintfx_apply_sanitizer_flags target)
    if(GLINTFX_SANITIZE STREQUAL "")
        return()
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "GLINTFX_SANITIZE=${GLINTFX_SANITIZE} was requested for target "
            "'${target}', but MSVC is not supported by this project's "
            "sanitizer flags (they are wired for GNU/Clang's "
            "-fsanitize=<comma-list> syntax only, see this function's own "
            "header comment). Build with GCC or Clang to use "
            "GLINTFX_SANITIZE, or track TODO.md item SAN-PARITY-WIN, which "
            "covers adding MSVC's own /fsanitize=address support.")
    endif()
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${GLINTFX_SANITIZE};-fno-sanitize-recover=all;-fno-omit-frame-pointer;-g>
    )
    target_link_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${GLINTFX_SANITIZE};-fno-sanitize-recover=all>
    )
endfunction()

# Single entry point that the target files (src/CMakeLists.txt,
# tests/CMakeLists.txt, GlintfxTest.cmake) call. Composition of the
# three functions above, with no logic of its own.
function(glintfx_apply_compile_options target)
    glintfx_apply_cxx_standard(${target})
    glintfx_apply_warning_flags(${target})
    glintfx_apply_sanitizer_flags(${target})
endfunction()
