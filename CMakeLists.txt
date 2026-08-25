# SPDX-License-Identifier: AGPL-3.0-or-later
#
# CMakeLists.txt - root of glintfx (FUND-1).
#
# Single subject: orchestrates the modules in cmake/, the library target
# in src/, the installable package and the test suite in tests/. No
# detail of option, warning, target property or install belongs here;
# each one has its own module in cmake/ (GODS_LAWS.md L-17: a file is
# an atom of subject).

cmake_minimum_required(VERSION 3.28)

# Four components, not three (GODS_LAWS.md L-26, VER-4C): TWEAK is the
# fourth, packaging-only field. write_basic_package_version_file's
# SameMinorVersion compatibility (cmake/GlintfxInstall.cmake) only
# compares MAJOR.MINOR, so a bare `find_package(glintfx 0.1)` still
# resolves this exact release under the current pre-1.0 policy.
project(glintfx VERSION 0.1.0.0 LANGUAGES CXX)

# PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7): when
# glintfx is embedded via add_subdirectory/FetchContent, CMAKE_SOURCE_DIR
# is the CONSUMER's root, not glintfx's own; PROJECT_SOURCE_DIR always
# tracks the most recent project() call, which is the `glintfx` one
# right above, regardless of who embeds it.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Single directory where the generated headers (export.hpp,
# version_macros.hpp) are born in the build. Kept in the root directory
# scope so that src/ and the install step read the same path without
# redeclaring it.
#
# PROJECT_BINARY_DIR, not CMAKE_BINARY_DIR (FIX-CONSUMO achado A7): the
# latter is always the consumer's own top-level build dir when embedded,
# so generated/ would land there and could collide with a `generated/`
# directory of the consumer's own. PROJECT_BINARY_DIR scopes it under
# glintfx's own subdirectory of the consumer's build tree instead.
set(GLINTFX_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")

include(GlintfxOptions)
include(GlintfxCompileOptions)
include(GlintfxLibrary)
include(GlintfxInstall)

add_subdirectory(src)

# Guarded by GLINTFX_INSTALL (FIX-CONSUMO achado A7): off by default when
# glintfx is embedded, so an add_subdirectory/FetchContent consumer does
# not get glintfx's headers and CMake package injected into its own
# `install` target as a side effect of vendoring the source.
if(GLINTFX_INSTALL)
    # glintfx_library, not the bare glintfx (FIX-CONSUMO-2, achado QA-3):
    # see the comment at add_library() in src/CMakeLists.txt.
    glintfx_install_library_target(glintfx_library)
    glintfx_install_public_headers()
    glintfx_install_cmake_package()
endif()

if(GLINTFX_BUILD_TESTS)
    # GlintfxTest before CTest, on purpose (WIN-HANG): the default
    # timeout it sets has to exist before include(CTest) generates
    # DartConfiguration.tcl, or CTest falls back to its own built-in
    # 1500s default, which is exactly what let a hung Windows test run
    # for 25 minutes before failing (GODS_LAWS.md L-04, five-platform CI).
    include(GlintfxTest)
    glintfx_set_default_test_timeout()
    include(CTest)
    add_subdirectory(tests)
endif()
