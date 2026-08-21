# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxTest.cmake
#
# Single subject: the function that registers a test executable linked
# against glintfx's own harness (GODS_LAWS.md L-07: no
# Catch2/GoogleTest), and the default timeout every ctest case gets when
# it does not set its own.

# WIN-HANG: a hung test used to cost the full CTest built-in default of
# 1500s (measured on the CI run e3f6222 - "Timeout 1499.99 sec" is that
# default, not a value this project ever set). Setting
# DART_TESTING_TIMEOUT before include(CTest) changes that default for
# EVERY test that does not declare its own TIMEOUT property, in one
# place, on all five platforms - so a genuinely hung process now fails
# in minutes, not in half an hour, without having to duplicate a TIMEOUT
# property on each add_test() call across tests/CMakeLists.txt. 120s is
# generous for the lightweight in-tree unit tests and for the heavier
# consumption gates (which each configure+build+run a small standalone
# CMake project) alike; a legitimate run finishes in a small fraction of
# it.
#
# Must run BEFORE include(CTest): that module reads the variable once,
# at include time, to generate DartConfiguration.tcl.
function(glintfx_set_default_test_timeout)
    set(DART_TESTING_TIMEOUT 120 PARENT_SCOPE)
endfunction()

function(glintfx_add_test name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_link_libraries(${name} PRIVATE glintfx::glintfx glintfx_test_harness)
    glintfx_apply_compile_options(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
