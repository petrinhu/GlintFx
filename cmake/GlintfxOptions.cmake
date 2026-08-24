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
