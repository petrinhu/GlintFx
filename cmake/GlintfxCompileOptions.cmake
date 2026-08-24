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
# MSVC guard, declared downgrade: this project's sanitizer CI job builds
# on Fedora with GNU/Clang only (see .github/workflows/ci.yml); MSVC's
# /fsanitize=address does not share GCC/Clang's -fsanitize=<comma-list>
# syntax and is out of scope for this slice.
function(glintfx_apply_sanitizer_flags target)
    if(GLINTFX_SANITIZE STREQUAL "")
        return()
    endif()
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${GLINTFX_SANITIZE};-fno-omit-frame-pointer;-g>
    )
    target_link_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fsanitize=${GLINTFX_SANITIZE}>
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
