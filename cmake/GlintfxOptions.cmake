# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxOptions.cmake
#
# Single subject: the configuration options that a user of the glintfx
# build can turn on or off. No target logic belongs here.

# Leader's order (FUND-1 service order): shared is the default.
option(BUILD_SHARED_LIBS "Build glintfx as a shared library" ON)

option(GLINTFX_WERROR "Treat glintfx compiler warnings as errors" OFF)

# GODS_LAWS.md L-23 portao 2 ("ASan e UBSan a cada fatia fechada"). Empty
# string means "no sanitizer" (the normal Release/local build); a
# comma-separated sanitizer list (e.g. "address,undefined") turns on
# -fsanitize=<value> on every glintfx target - the library AND every test
# executable, via glintfx_apply_compile_options in
# GlintfxCompileOptions.cmake, because ASan requires its runtime present in
# BOTH the shared library and whatever links it. FUND-4's own CI sanitizer
# job configures with this set and runs only the `unit` label (see
# tests/CMakeLists.txt): the consumption gates below link a plain,
# non-sanitized consumer against the sanitized library on purpose (that is
# what they prove), and ASan's runtime interposition requires coming first
# in a process - the two facts together make those gates fail by
# construction under this option, not by a real defect. Declared downgrade,
# not silently worked around.
set(GLINTFX_SANITIZE "" CACHE STRING
    "Comma-separated sanitizer list for -fsanitize (e.g. address,undefined); empty = none"
)

# FUND-4 achado: CMake's automatic C++ module dependency scanning
# (default-on for Ninja + a compiler that supports it, which GCC 14+ does)
# makes Ninja inject GCC-only flags (-fdeps-format=p1689r5,
# -fmodule-mapper=..., -fmodules-ts) into every compile command in
# compile_commands.json, on EVERY build, even though this project has zero
# C++20 module code (L-03 is C++23, not modules). clang-tidy's own argument
# parser rejects those three flags outright ("unknown argument"), which
# broke the FUND-4 lint stage against every single translation unit before
# this line existed - measured live building this slice, not theoretical:
# `run-clang-tidy` errored on 3 of 5 project .cpp files with that exact
# message until this option went in. Turning the scanner off costs nothing
# (nothing here uses `import`/`export module`) and unblocks GODS_LAWS.md
# L-23 portao 3 (clang-tidy) for real.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(GLINTFX_BUILD_TESTS
    "Build the glintfx test suite"
    ${PROJECT_IS_TOP_LEVEL}
)

# FIX-CONSUMO achado A7: when a consumer embeds glintfx via
# add_subdirectory/FetchContent, glintfx's own install() rules must not
# run by default under the consumer's `install` target - the consumer
# did not ask for glintfx's headers or CMake package to appear in its
# own install prefix just because it vendors the source. Same default
# pattern as GLINTFX_BUILD_TESTS above: on by default only when glintfx
# is the top-level project; a consumer that DOES want glintfx installed
# alongside its own artifacts can still opt in explicitly.
option(GLINTFX_INSTALL
    "Install the glintfx target, headers and CMake package"
    ${PROJECT_IS_TOP_LEVEL}
)

# compile_commands.json only makes sense when glintfx is the top-level
# project: a consumer that embeds glintfx via add_subdirectory has its
# own compile_commands.json and must not have it overwritten by ours.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
        "Export compile_commands.json" FORCE)
endif()

# EMBED-DLL (found live, CI-CONSUME - GODS_LAWS.md L-27, fact before
# fix): an embedded (add_subdirectory), shared, Windows build configures,
# builds and links cleanly, then the produced executable fails to even
# start - confirmed by the real CI run's raw log (no Windows/pwsh
# available on this machine to reproduce end to end; see the fact-
# gathering in the commit this option ships in). Root cause: the DLL
# lands in glintfx's OWN nested build subdirectory
# (<embed-build>/glintfx-build/src/<Config>/glintfx.dll), while an
# embedding consumer's own executable, un-customized, lands at the
# OUTERMOST project's own runtime directory
# (<embed-build>/<Config>/consumer.exe) - two different directories.
# Windows has no build-tree equivalent of the ELF RPATH the Linux side
# of this problem never needed (CMake auto-embeds a build-tree RPATH
# into a Linux executable, so an embedding Linux consumer never hits
# this), and the default Windows DLL search order does not search the
# rest of the build tree, only the executable's own folder (confirmed
# against learn.microsoft.com/windows/win32/dlls/
# dynamic-link-library-search-order, not assumed).
#
# On by default ONLY in the exact scenario that produces the failure:
# embedded (glintfx is not the top-level project), built shared, on
# Windows. A standalone glintfx build, a static build, and a Linux
# build are left untouched - none of them ever hit this failure mode,
# so none of them need the mechanism (GODS_LAWS.md LEI ZERO: the fix
# is scoped to the actual gap, not applied everywhere out of caution).
#
# A named function, not inlined (GODS_LAWS.md L-17: "se você consegue
# extrair uma sub-funcao com nome proprio e honesto, ela nao era um
# atomo") - the ONLY reason this is a function and not a plain if/else
# is so tests/embed_dll_colocation/CMakeLists.txt can call it directly,
# once per PROJECT_IS_TOP_LEVEL x WIN32 x BUILD_SHARED_LIBS combination,
# from a SINGLE configure: option() below only computes its default
# ONCE per build directory (it caches), so testing all eight
# combinations without this function would need eight separate `cmake
# -S/-B` processes instead of one.
function(glintfx_embedded_runtime_colocate_default out_var)
    if((NOT PROJECT_IS_TOP_LEVEL) AND WIN32 AND BUILD_SHARED_LIBS)
        set(${out_var} ON PARENT_SCOPE)
    else()
        set(${out_var} OFF PARENT_SCOPE)
    endif()
endfunction()

glintfx_embedded_runtime_colocate_default(GLINTFX_EMBEDDED_RUNTIME_COLOCATE_DEFAULT)

# The escape valve the mechanism needs by design, not bolted on
# afterward: a consumer with an existing, deliberate output layout must
# not have it silently overridden. glintfx_colocate_embedded_runtime_dll()
# (GlintfxLibrary.cmake) additionally never acts at all when the consumer
# has already set CMAKE_RUNTIME_OUTPUT_DIRECTORY (the CMake-blessed
# convention for this exact problem, which glintfx's own default output
# directory already inherits like any other un-customized target) - this
# option is the valve for every OTHER kind of custom layout, e.g. a
# consumer that sets per-target RUNTIME_OUTPUT_DIRECTORY properties by
# hand instead of the shared variable.
option(GLINTFX_EMBEDDED_RUNTIME_COLOCATE
    "When glintfx is embedded and built shared on Windows, place glintfx's own DLL next to the outermost project's default runtime output location, so an embedding consumer's executable finds it without any extra step"
    ${GLINTFX_EMBEDDED_RUNTIME_COLOCATE_DEFAULT}
)
