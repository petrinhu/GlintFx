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

# compile_commands.json only makes sense when glintfx is the top-level
# project: a consumer that embeds glintfx via add_subdirectory has its
# own compile_commands.json and must not have it overwritten by ours.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
        "Export compile_commands.json" FORCE)
endif()
