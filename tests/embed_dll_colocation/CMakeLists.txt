# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/embed_dll_colocation/CMakeLists.txt
#
# Single subject: proves the DECISION LOGIC of EMBED-DLL -
# glintfx_colocate_embedded_runtime_dll() and
# glintfx_embedded_runtime_colocate_default() (both cmake/GlintfxLibrary
# .cmake / cmake/GlintfxOptions.cmake) - against every branch that
# matters, WITHOUT an actual Windows build or toolchain.
#
# LANGUAGES NONE below is load-bearing, not a stylistic choice:
# configuring this project never invokes a compiler, which is what
# makes add_library(... IMPORTED) scriptable and WIN32/
# BUILD_SHARED_LIBS/PROJECT_IS_TOP_LEVEL/CMAKE_RUNTIME_OUTPUT_DIRECTORY
# settable BY HAND to rehearse the embedded-Windows-shared scenario on
# ANY host - every Linux CI job and this developer's own machine
# included, none of which has a Windows toolchain to prove the DLL move
# end to end for real. That end-to-end proof is, and stays,
# check-embed.ps1's Invoke-RunEmbed job (it runs the real produced
# executable and reads its real exit code - GODS_LAWS.md L-20, "não um
# teste que só confere se um arquivo existe"). This fixture's job is
# narrower and earlier: proving WHICH target property gets touched,
# under WHICH variables, before that binary is ever built - the
# decision a Windows-only test could not rehearse locally at all.
#
# NOT add_subdirectory()'d by the main build (same convention as
# tests/embed/ and tests/embed_name_collision/): an independent
# configure, run via add_test() in tests/CMakeLists.txt.
#
# GODS_LAWS.md L-17: each assert_* macro below asks exactly one
# question; a failed one calls message(FATAL_ERROR ...), which fails
# THIS cmake -S/-B invocation itself - the test's pass/fail signal.

cmake_minimum_required(VERSION 3.28)
project(glintfx_embed_dll_colocation_check LANGUAGES NONE)

if(NOT DEFINED GLINTFX_SOURCE_DIR)
    message(FATAL_ERROR
        "GLINTFX_SOURCE_DIR must be set to the glintfx source tree "
        "(-DGLINTFX_SOURCE_DIR=<path>)")
endif()

include("${GLINTFX_SOURCE_DIR}/cmake/GlintfxOptions.cmake")
include("${GLINTFX_SOURCE_DIR}/cmake/GlintfxLibrary.cmake")

# "expected" is either a real path string, or the literal keyword
# UNSET meaning "the function must not have touched the property at
# all". Not compared against a hardcoded "<target>-NOTFOUND" string:
# get_target_property()'s own NOTFOUND sentinel is suffixed to the
# OUTPUT VARIABLE's name (_glintfx_check_actual, below), never the
# target's - caught live, by running this exact macro, before this
# comment existed: the first version hardcoded "<target>-NOTFOUND" and
# failed on case1 with actual "_glintfx_check_actual-NOTFOUND".
macro(assert_runtime_output_directory target expected message)
    get_target_property(_glintfx_check_actual ${target} RUNTIME_OUTPUT_DIRECTORY)
    if("${expected}" STREQUAL "UNSET")
        if(_glintfx_check_actual)
            message(FATAL_ERROR
                "FAIL: ${message} - expected RUNTIME_OUTPUT_DIRECTORY to be "
                "UNSET, got '${_glintfx_check_actual}'")
        endif()
    elseif(NOT "${_glintfx_check_actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "FAIL: ${message} - expected RUNTIME_OUTPUT_DIRECTORY "
            "'${expected}', got '${_glintfx_check_actual}'")
    endif()
    message(STATUS "ok: ${message}")
endmacro()

macro(assert_default_is target_value expected message)
    if(NOT "${target_value}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "FAIL: ${message} - expected default '${expected}', "
            "got '${target_value}'")
    endif()
    message(STATUS "ok: ${message}")
endmacro()

# --- glintfx_colocate_embedded_runtime_dll(): the mechanism itself ---
#
# Each case below creates its OWN IMPORTED target instead of sharing
# one: a shared target would carry a property set by an earlier case
# into a later one, corrupting the very thing being asserted
# (GODS_LAWS.md L-17: a fixture that grows shared state per case is a
# monolith warning sign - this sidesteps it by making the target
# disposable, not by growing the setup).

# Case 1: option OFF -> function must not touch the property at all,
# regardless of CMAKE_RUNTIME_OUTPUT_DIRECTORY or CMAKE_BINARY_DIR (the
# option is the single kill switch).
add_library(case1_option_off SHARED IMPORTED)
set(GLINTFX_EMBEDDED_RUNTIME_COLOCATE OFF)
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
set(CMAKE_BINARY_DIR "/fake/bin")
glintfx_colocate_embedded_runtime_dll(case1_option_off)
assert_runtime_output_directory(case1_option_off
    "UNSET"
    "option OFF leaves RUNTIME_OUTPUT_DIRECTORY untouched")

# Case 2: option ON, consumer already set CMAKE_RUNTIME_OUTPUT_DIRECTORY
# (their own layout convention) -> the escape valve: function must not
# override it, even though it otherwise would.
add_library(case2_consumer_layout SHARED IMPORTED)
set(GLINTFX_EMBEDDED_RUNTIME_COLOCATE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "/fake/consumer/bin")
set(CMAKE_BINARY_DIR "/fake/bin")
glintfx_colocate_embedded_runtime_dll(case2_consumer_layout)
assert_runtime_output_directory(case2_consumer_layout
    "UNSET"
    "consumer's own CMAKE_RUNTIME_OUTPUT_DIRECTORY is respected, not overridden")

# Case 3: option ON, consumer has NOT customized layout -> the actual
# fix: glintfx's DLL joins CMAKE_BINARY_DIR, the outermost project's own
# default runtime location.
add_library(case3_apply SHARED IMPORTED)
set(GLINTFX_EMBEDDED_RUNTIME_COLOCATE ON)
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
set(CMAKE_BINARY_DIR "/fake/bin")
glintfx_colocate_embedded_runtime_dll(case3_apply)
assert_runtime_output_directory(case3_apply
    "/fake/bin"
    "un-customized layout: glintfx.dll joins CMAKE_BINARY_DIR")

# --- glintfx_embedded_runtime_colocate_default(): the option's OWN
#     default, enumerated exhaustively (2x2x2 = 8, cheap and fully
#     enumerable - the "enumere o espaço pequeno" house discipline: a
#     hand-picked subset of these eight is exactly the kind of search
#     that misses the one combination nobody thought to suspect). ON in
#     EXACTLY one of the eight: embedded + Windows + shared.

set(_glintfx_bool_values FALSE TRUE)
set(_glintfx_case_number 0)
foreach(_glintfx_top_level IN LISTS _glintfx_bool_values)
    foreach(_glintfx_win32 IN LISTS _glintfx_bool_values)
        foreach(_glintfx_shared IN LISTS _glintfx_bool_values)
            math(EXPR _glintfx_case_number "${_glintfx_case_number} + 1")

            set(PROJECT_IS_TOP_LEVEL ${_glintfx_top_level})
            set(WIN32 ${_glintfx_win32})
            set(BUILD_SHARED_LIBS ${_glintfx_shared})

            if((NOT _glintfx_top_level) AND _glintfx_win32 AND _glintfx_shared)
                set(_glintfx_expected ON)
            else()
                set(_glintfx_expected OFF)
            endif()

            glintfx_embedded_runtime_colocate_default(_glintfx_actual)
            assert_default_is("${_glintfx_actual}" "${_glintfx_expected}"
                "combination ${_glintfx_case_number}/8 (top_level=${_glintfx_top_level} win32=${_glintfx_win32} shared=${_glintfx_shared})")
        endforeach()
    endforeach()
endforeach()

if(NOT _glintfx_case_number EQUAL 8)
    message(FATAL_ERROR
        "FAIL: enumerated ${_glintfx_case_number} combinations, expected exactly 8 "
        "(2x2x2 truth table) - the enumeration itself regressed")
endif()

message(STATUS "ok: all 8 PROJECT_IS_TOP_LEVEL x WIN32 x BUILD_SHARED_LIBS combinations checked")
