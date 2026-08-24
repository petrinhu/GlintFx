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

# WIN-HANG, root cause: on the multi-config generator this project's
# Windows CI job uses (no -G Ninja, so windows-latest defaults to
# Visual Studio), each target keeps its OWN per-target output
# subdirectory unless CMAKE_RUNTIME_OUTPUT_DIRECTORY says otherwise. The
# glintfx SHARED library lands in src/Release/, a test executable in
# tests/Release/ - siblings, not the same directory - and the Windows
# loader only searches the executable's own directory (then system
# dirs, then PATH), never a sibling one. Confirmed live on CI (run
# 32525937090): the missing DLL makes the process exit with
# STATUS_DLL_NOT_FOUND (-1073741515 / 0xC0000135), and under ctest's
# output capture that crash goes on to hang instead of failing fast
# (Windows Error Reporting inheriting the redirected stdout/stderr
# handle is the suspected mechanism - not confirmed, and not needed to
# be: removing the missing DLL removes the crash that triggers it).
#
# $<TARGET_RUNTIME_DLLS:tgt> is CMake's own generator expression for
# this exact problem (added 3.21; this project requires 3.28): it
# resolves, at build time, to the SHARED/MODULE libraries `tgt` actually
# links against - not a hardcoded "glintfx.dll" name, so it keeps
# working the day this library grows a second internal DLL. Guarded by
# WIN32 AND BUILD_SHARED_LIBS, and both halves of that guard matter:
# the expression resolves to an EMPTY list on any non-Windows build,
# shared or static (ELF and Mach-O have no runtime DLL to place next to
# the executable), and also on a Windows static build. An empty file
# list makes `copy_if_different` itself fail, so the guard is what keeps
# the Linux build working, not just the static one.
#
# Deliberately NOT fixed via a global CMAKE_RUNTIME_OUTPUT_DIRECTORY:
# that variable is read by every target created afterward, including an
# embedding consumer's OWN targets when glintfx is pulled in via
# add_subdirectory/FetchContent (GODS_LAWS.md LEI ZERO - the consumer
# base is open and unknown, and is not this project's output layout to
# dictate). This function is scoped to test executables only.
function(glintfx_copy_runtime_dlls_after_build target)
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${target}>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

# LABELS unit (FUND-4): the CI sanitizer job (GODS_LAWS.md L-23 portao 2)
# runs `ctest -L unit` under GLINTFX_SANITIZE, never the full suite - the
# shell-script consumption gates in tests/CMakeLists.txt link a plain,
# non-sanitized consumer against the sanitized library on purpose, and
# that fails by construction under ASan (see the comment on
# GLINTFX_SANITIZE in GlintfxOptions.cmake). Only a case registered
# through THIS function - the in-tree C++ harness cases, today just
# version_test - is safe to run sanitized, so only this function sets
# the label; tests/CMakeLists.txt sets LABELS consume on its own
# add_test() calls instead.
function(glintfx_add_test name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_link_libraries(${name} PRIVATE glintfx::glintfx glintfx_test_harness)
    glintfx_apply_compile_options(${name})
    glintfx_copy_runtime_dlls_after_build(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS unit)
endfunction()
