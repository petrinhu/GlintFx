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
#
# DLL-EMPTY (CI run 34118301960, Windows shared, win32_facade_pin_test):
# the WIN32 AND BUILD_SHARED_LIBS guard above is necessary but not
# sufficient - it only proves `target` is a Windows target built shared,
# not that `target` itself links a SHARED library. win32_facade_pin_test
# links no library at all (tests/CMakeLists.txt: it recompiles the
# facade sources standalone, on purpose, to prove the packaged facade
# survives without glintfx::glintfx), so $<TARGET_RUNTIME_DLLS:target>
# resolves to an EMPTY list for it. COMMAND_EXPAND_LISTS then drops that
# empty list from the command line entirely rather than passing an empty
# argument, degenerating the line to `copy_if_different <dest-dir>` -
# `copy_if_different` requires at least one source, so CMake's own `-E`
# dispatcher rejects it as a usage error (exit 1), and the whole target
# fails to build even though its actual compile+link already succeeded.
# The fix swaps the SUBCOMMAND itself, not just its arguments: when the
# DLL list is non-empty run copy_if_different as before, when it is empty
# run `true` (CMake's own -E no-op, always exit 0) instead - so a target
# with nothing to copy compiles clean rather than tripping over a
# malformed copy. This is the idiom CMake users reach for on this exact
# generator-expression pitfall (no single blessed recipe exists in the
# upstream docs for it).
# WIN-DLL-COPY-RACE (D-W8-363 to D-W8-365): the copy is ONE step per destination folder, never one per test.
# The old POST_BUILD put the same copy of glintfx.dll into the same tests/ folder from every test target, and
# ninja runs those steps in parallel: one copy failed while another was still writing the file ("Error copying
# file (if different)", CI run 38171206274). Now the FIRST target of a folder that links glintfx::glintfx gets a
# copy target, glintfx_runtime_dlls_<n>, that copies $<TARGET_RUNTIME_DLLS:<that target>> into
# $<TARGET_FILE_DIR:<that target>> once; every later target of the folder only depends on it. A target that links
# no glintfx::glintfx (win32_facade_pin_test recompiles its sources standalone) needs no copy and gets none.
# A target with its own RUNTIME_OUTPUT_DIRECTORY is refused at configure time: its DLL would go nowhere.
function(glintfx_runtime_dll_copy_target out_name anchor)
    get_property(_count GLOBAL PROPERTY GLINTFX_RUNTIME_DLL_COPY_COUNT)
    if(NOT _count)
        set(_count 0)
    endif()
    math(EXPR _count "${_count} + 1")
    set_property(GLOBAL PROPERTY GLINTFX_RUNTIME_DLL_COPY_COUNT ${_count})
    set(_copy glintfx_runtime_dlls_${_count})
    add_custom_target(${_copy}
        COMMAND "${CMAKE_COMMAND}" -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${anchor}>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:${anchor}>" "$<TARGET_FILE_DIR:${anchor}>"
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(${_copy} glintfx_library)
    set(${out_name} ${_copy} PARENT_SCOPE)
endfunction()

function(glintfx_copy_runtime_dlls_after_build target)
    if(NOT (WIN32 AND BUILD_SHARED_LIBS))
        return()
    endif()
    get_target_property(_linked ${target} LINK_LIBRARIES)
    if(NOT "glintfx::glintfx" IN_LIST _linked)
        return()
    endif()
    get_target_property(_output_dir ${target} RUNTIME_OUTPUT_DIRECTORY)
    if(_output_dir)
        message(FATAL_ERROR "glintfx_copy_runtime_dlls_after_build: ${target} tem RUNTIME_OUTPUT_DIRECTORY proprio "
            "(${_output_dir}); a copia unica da DLL so chega a pasta do primeiro alvo da pasta (WIN-DLL-COPY-RACE)")
    endif()
    get_property(_copy DIRECTORY PROPERTY GLINTFX_RUNTIME_DLL_COPY)
    if(NOT _copy)
        glintfx_runtime_dll_copy_target(_copy ${target})
        set_property(DIRECTORY PROPERTY GLINTFX_RUNTIME_DLL_COPY ${_copy})
    endif()
    add_dependencies(${target} ${_copy})
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
