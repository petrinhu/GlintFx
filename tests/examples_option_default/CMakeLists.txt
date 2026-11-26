# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/examples_option_default/CMakeLists.txt
#
# Single subject: proves the DEFAULT of GLINTFX_BUILD_EXAMPLES
# (glintfx_examples_default(), cmake/GlintfxOptions.cmake) against both
# branches that matter - glintfx as the top-level project (examples ON)
# and glintfx embedded in a consumer's build (examples OFF) - WITHOUT
# configuring the whole library.
#
# Same technique and same reason as tests/embed_dll_colocation/: the
# option() in GlintfxOptions.cmake computes its default ONCE per build
# directory (it caches), so the two branches could not both be seen from
# a single configure without calling the function directly. LANGUAGES
# NONE means nothing here ever invokes a compiler.
#
# What this fixture does NOT prove, on purpose: that the REAL option()
# reacts when glintfx is embedded. That is tests/tools/check_embed.sh
# (embed_test), which reads GLINTFX_BUILD_EXAMPLES out of the cache of a
# really embedded glintfx. The assertion on the cache value at the end of
# this file covers only the top-level branch of the real option().
#
# NOT add_subdirectory()'d by the main build: an independent configure,
# run via add_test() in tests/CMakeLists.txt.
#
# Each assert_* macro asks exactly one question; a failed one calls
# message(FATAL_ERROR ...), which fails THIS cmake -S/-B invocation - the
# test's pass/fail signal.

cmake_minimum_required(VERSION 3.28)
project(glintfx_examples_option_default_check LANGUAGES NONE)

if(NOT DEFINED GLINTFX_SOURCE_DIR)
    message(FATAL_ERROR
        "GLINTFX_SOURCE_DIR must be set to the glintfx source tree "
        "(-DGLINTFX_SOURCE_DIR=<path>)")
endif()

include("${GLINTFX_SOURCE_DIR}/cmake/GlintfxOptions.cmake")

macro(assert_examples_default is_top_level expected message)
    glintfx_examples_default(${is_top_level} _glintfx_examples_actual)
    if(NOT "${_glintfx_examples_actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "FAIL: ${message} - expected default '${expected}', "
            "got '${_glintfx_examples_actual}'")
    endif()
    message(STATUS "ok: ${message}")
endmacro()

# --- the function, both branches --------------------------------------
assert_examples_default(TRUE ON
    "examples are built by default when glintfx is the top-level project")
assert_examples_default(FALSE OFF
    "examples are NOT built by default when glintfx is embedded")

# The two spellings CMake itself uses for PROJECT_IS_TOP_LEVEL ("ON"/"OFF"
# on some versions, TRUE/FALSE on others): the function must not depend
# on which one it is handed.
assert_examples_default(ON ON
    "top-level spelled ON gives the top-level default")
assert_examples_default(OFF OFF
    "embedded spelled OFF gives the embedded default")

# --- the real option(), top-level branch -------------------------------
# This fixture IS a top-level project, so the option() that
# GlintfxOptions.cmake declared above, through the same function, must
# have landed on ON in the cache.
if(NOT GLINTFX_BUILD_EXAMPLES)
    message(FATAL_ERROR
        "FAIL: the real option(GLINTFX_BUILD_EXAMPLES) is not ON in a "
        "top-level project - option() does not take its default from "
        "glintfx_examples_default()")
endif()
message(STATUS "ok: the real option(GLINTFX_BUILD_EXAMPLES) is ON at top level")
