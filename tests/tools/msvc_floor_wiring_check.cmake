# SPDX-License-Identifier: AGPL-3.0-or-later
#
# msvc_floor_wiring_check.cmake - S5-A-fix5 (D-W8-482): the top-level CMakeLists.txt includes
# GlintfxToolchainFloor and, AFTER it, calls glintfx_require_msvc_floor("${MSVC_VERSION}") under
# if(MSVC), each one as whole lines (a commented line does not count). It READS the file: it
# proves the wiring is written, not that an MSVC configure runs it. DECLARED LIMIT: no MSVC below
# the floor runs anywhere, so no configure in this project refuses one.
#   cmake -DTOP_LEVEL_LISTS=<top-level CMakeLists.txt> -P msvc_floor_wiring_check.cmake

if(NOT DEFINED TOP_LEVEL_LISTS)
    message(FATAL_ERROR "msvc_floor_wiring_check: TOP_LEVEL_LISTS is required")
endif()
file(READ "${TOP_LEVEL_LISTS}" text)
string(REPLACE "\r" "" text "\n${text}\n")
set(include_line "\ninclude(GlintfxToolchainFloor)\n")
set(call_block "\nif(MSVC)\n    glintfx_require_msvc_floor(\"\${MSVC_VERSION}\")\nendif()\n")
string(FIND "${text}" "${include_line}" include_at)
if(include_at EQUAL -1)
    message(FATAL_ERROR
        "msvc_floor_wiring_check: no whole line include(GlintfxToolchainFloor)")
endif()
string(SUBSTRING "${text}" ${include_at} -1 after_include)
string(FIND "${after_include}" "${call_block}" call_at)
if(call_at EQUAL -1)
    message(FATAL_ERROR
        "msvc_floor_wiring_check: no call under if(MSVC), as three whole lines, after the include")
endif()
message(STATUS "msvc_floor_wiring_check: the include, then the call under if(MSVC): wired")
