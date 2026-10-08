# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/examples_pass/CMakeLists.txt
#
# Single subject: exercises the examples pass (cmake/GlintfxExamples.cmake)
# on ONE planta per configure, WITHOUT configuring the library (D-W8-93).
# Each planta under plantas/<name>/ is a tree with its own examples/, which
# the pass reads through its ARGUMENTS, never through PROJECT_SOURCE_DIR.
#
# The pass ends in FATAL_ERROR, which ends the configure, so the driver
# tests/tools/check_examples_pass.py runs one configure per planta and
# checks the exit code and the message of each. LANGUAGES CXX only so that
# add_executable() exists; nothing is built.
#
# NOT add_subdirectory()'d by the main build: an independent configure,
# run via add_test() in tests/CMakeLists.txt.

cmake_minimum_required(VERSION 3.28)
project(glintfx_examples_pass_check LANGUAGES CXX)

if(NOT DEFINED GLINTFX_SOURCE_DIR)
    message(FATAL_ERROR
        "GLINTFX_SOURCE_DIR must be set to the glintfx source tree "
        "(-DGLINTFX_SOURCE_DIR=<path>)")
endif()
if(NOT DEFINED PLANTA)
    message(FATAL_ERROR "PLANTA must be set (-DPLANTA=<name>)")
endif()
set(_planta_dir "${CMAKE_CURRENT_SOURCE_DIR}/plantas/${PLANTA}")
if(NOT EXISTS "${_planta_dir}")
    message(FATAL_ERROR "planta ${PLANTA} does not exist under plantas/")
endif()

# The two options the compile-option functions read (D-W8-96). Defined
# here, explicitly, as the root's defaults are: a variable that is NOT
# defined makes `if(GLINTFX_SANITIZE STREQUAL "")` compare the literal
# name, which is false, so the sanitizer flags would be applied with an
# empty value, and on MSVC a non-"address" value would be a FATAL_ERROR.
# Same convention as tests/examples_option_default/: nothing is inherited
# implicitly from the parent.
set(GLINTFX_SANITIZE "")
set(GLINTFX_WERROR OFF)

include("${GLINTFX_SOURCE_DIR}/cmake/GlintfxCompileOptions.cmake")
include("${GLINTFX_SOURCE_DIR}/cmake/GlintfxExamples.cmake")

glintfx_add_examples("${_planta_dir}" "${CMAKE_CURRENT_BINARY_DIR}")
