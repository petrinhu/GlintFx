# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxOptions.cmake
#
# Single subject: the configuration options that a user of the glintfx
# build can turn on or off. No target logic belongs here.

# Leader's order (FUND-1 service order): shared is the default.
option(BUILD_SHARED_LIBS "Build glintfx as a shared library" ON)

option(GLINTFX_WERROR "Treat glintfx compiler warnings as errors" OFF)

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
