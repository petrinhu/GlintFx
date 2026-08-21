# tests/package/CMakeLists.txt
#
# Single subject: standalone CMake project that proves
# find_package(glintfx) outside the glintfx tree (FUND-1, item 9 of the
# service order). Is NOT included by the main build via
# add_subdirectory; runs as an independent configure/build, pointing
# CMAKE_PREFIX_PATH at the installed prefix.

cmake_minimum_required(VERSION 3.28)

project(glintfx_consumer LANGUAGES CXX)

find_package(glintfx 0.1 REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE glintfx::glintfx)
set_target_properties(consumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
