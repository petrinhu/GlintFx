# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxToolchainFloor.cmake
#
# Single subject: the oldest compiler of a family this project accepts,
# refused at configure time with a message, never left to fail later in a
# header nobody expected (S5-A-fix4, D-W8-465).
#
# MSVC: Visual Studio 2022, MSVC_VERSION 1930 or newer. Two reasons, both
# load-bearing: the C++23 the library is written in, and the floating-point
# rule of GlintfxCompileOptions.cmake (from Visual Studio 2022 on,
# /fp:precise generates no contraction; Visual Studio 2019 could contract
# by default, learn.microsoft.com/cpp/build/reference/fp-specify-floating-
# point-behavior), which keeps the bits the same on every system
# (GODS_LAWS.md L-04). tests/tools/msvc_floor_check.cmake runs this
# function on both sides of the floor.

function(glintfx_require_msvc_floor msvc_version)
    if(NOT msvc_version MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "glintfx: MSVC_VERSION '${msvc_version}' is not a number, so the "
            "floor of Visual Studio 2022 (1930) cannot be checked.")
    endif()
    if(msvc_version LESS 1930)
        message(FATAL_ERROR
            "glintfx needs Visual Studio 2022 or newer (MSVC_VERSION 1930 or "
            "more); this compiler is MSVC_VERSION ${msvc_version}. Older "
            "versions may fuse a * b + c under /fp:precise, which would "
            "change the library's results from one system to another.")
    endif()
endfunction()
