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
# SAN-PARITY-WIN (TODO.md; achado de 03/09/2026, fechado parcialmente
# nesta fatia): MSVC's /fsanitize=address (memory-safety detection,
# available since Visual Studio 2019 16.9) is now wired below - the
# fatal_error guard used to refuse EVERY non-empty GLINTFX_SANITIZE
# under MSVC, silently-would-have-been-wrong turned into a named
# refusal that stayed permanent. It is now permanent for a narrower,
# TRUE reason instead: MSVC has never shipped, in any version, an
# equivalent to GCC/Clang's UBSan (undefined-behavior detection) - that
# is not a gap this project's plumbing can close, it is a gap in the
# compiler itself (learn.microsoft.com/cpp/sanitizers/asan, "Overview" -
# MSVC's sanitizer story is AddressSanitizer only). So
# GLINTFX_SANITIZE=address now builds and runs under MSVC exactly like
# it does under GNU/Clang; any OTHER value (address,undefined; undefined
# alone; a typo) still refuses at configure time, naming what was asked
# and why it can never be satisfied here, never silently dropping the
# undefined-behavior half.
#
# Incompatible-options guard (learn.microsoft.com/cpp/sanitizers/asan-
# known-issues, "Incompatible options and functionality"): /RTC (runtime
# checks) and incremental linking are both incompatible with
# /fsanitize=address and MUST be disabled. Neither is ever present in
# this project's own Release configure (CMAKE_CXX_FLAGS_RELEASE has no
# /RTC - that is a CMake Debug-only default this project never touches
# in GlintfxCompileOptions.cmake; incremental linking needs /DEBUG at
# link time to even engage, which a plain Release build never passes),
# so nothing here needs to be stripped OUT of an existing flag - but
# /INCREMENTAL:NO is still added explicitly, matching Microsoft's own
# "should be disabled" wording literally instead of relying on a
# default that depends on flags this file does not control staying
# absent forever. /Zi (debug info) and /DEBUG (linker) are added too:
# Microsoft's own docs say call-stack formatting needs it, and the
# GATE-ASAN-HALT halting behaviour below does not depend on either -
# MSVC's AddressSanitizer runtime already treats most findings as
# non-continuable by default (learn.microsoft.com/cpp/sanitizers/asan-
# runtime, "Runtime options" note), so there is no MSVC equivalent of
# -fno-sanitize-recover=all to add here.
function(glintfx_apply_sanitizer_flags target)
    if(GLINTFX_SANITIZE STREQUAL "")
        return()
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(NOT GLINTFX_SANITIZE STREQUAL "address")
            message(FATAL_ERROR
                "GLINTFX_SANITIZE=${GLINTFX_SANITIZE} was requested for "
                "target '${target}', but MSVC only ever supports "
                "'address' (AddressSanitizer, memory-safety detection) - "
                "no version of MSVC has an equivalent to GCC/Clang's "
                "UBSan (undefined-behavior detection), so 'undefined' in "
                "the list can never be satisfied on this compiler. Build "
                "with GCC or Clang for the full address,undefined set, or "
                "configure with GLINTFX_SANITIZE=address alone for MSVC.")
        endif()
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        target_link_options(${target} PRIVATE /DEBUG /INCREMENTAL:NO)
        return()
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
