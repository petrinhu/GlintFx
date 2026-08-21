# CMakeLists.txt - root of glintfx (FUND-1).
#
# Single subject: orchestrates the modules in cmake/, the library target
# in src/, the installable package and the test suite in tests/. No
# detail of option, warning, target property or install belongs here;
# each one has its own module in cmake/ (GODS_LAWS.md L-17: a file is
# an atom of subject).

cmake_minimum_required(VERSION 3.28)

project(glintfx VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Single directory where the generated headers (export.hpp,
# version_macros.hpp) are born in the build. Kept in the root directory
# scope so that src/ and the install step read the same path without
# redeclaring it.
set(GLINTFX_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")

include(GlintfxOptions)
include(GlintfxCompileOptions)
include(GlintfxLibrary)
include(GlintfxInstall)

add_subdirectory(src)

glintfx_install_library_target(glintfx)
glintfx_install_public_headers()
glintfx_install_cmake_package()

if(GLINTFX_BUILD_TESTS)
    include(CTest)
    include(GlintfxTest)
    add_subdirectory(tests)
endif()
