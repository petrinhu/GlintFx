# SPDX-License-Identifier: AGPL-3.0-or-later
#
# msvc_floor_check.cmake - S5-A-fix4 (D-W8-466): runs
# glintfx_require_msvc_floor() of cmake/GlintfxToolchainFloor.cmake on the
# MSVC_VERSION given as MSVC_VERSION_UNDER_TEST, in script mode, so the
# floor is proved on a machine without MSVC. Stops with an error (non-zero
# exit) exactly when the function refuses.
#   cmake -DGLINTFX_CMAKE_DIR=<cmake dir> -DMSVC_VERSION_UNDER_TEST=<n> -P msvc_floor_check.cmake

if(NOT DEFINED GLINTFX_CMAKE_DIR OR NOT DEFINED MSVC_VERSION_UNDER_TEST)
    message(FATAL_ERROR
        "msvc_floor_check: GLINTFX_CMAKE_DIR and MSVC_VERSION_UNDER_TEST are required")
endif()
include("${GLINTFX_CMAKE_DIR}/GlintfxToolchainFloor.cmake")
glintfx_require_msvc_floor("${MSVC_VERSION_UNDER_TEST}")
message(STATUS "msvc_floor_check: ${MSVC_VERSION_UNDER_TEST} accepted")
