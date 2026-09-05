# SPDX-License-Identifier: AGPL-3.0-or-later
#
# CMakeLists.txt - root of glintfx (FUND-1).
#
# Single subject: orchestrates the modules in cmake/, the library target
# in src/, the installable package and the test suite in tests/. No
# detail of option, warning, target property or install belongs here;
# each one has its own module in cmake/ (GODS_LAWS.md L-17: a file is
# an atom of subject).

# 4.1, not 3.28 (PKG-NATIVE, 27/08/2026, ESCOPO.md paragraph 7 - "A
# exigencia minima da ferramenta de build SOBE, e o leitor caseiro
# morre"): cmake_pkg_config(), CMake's own native pkg-config format
# parser, needs its IMPORT/POPULATE subcommands only reached CMake 4.1
# (EXTRACT alone shipped in CMake 3.31 - see
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in's own header for
# which subcommand this project actually uses and why the full 4.1
# floor is still the honest one to declare). MEASURED, not assumed,
# against every one of the five supported platforms (GODS_LAWS.md
# L-04) before this line changed: Fedora 44 ships 4.3.0, Arch and
# CachyOS ship 4.4.2 (all three comfortably above the floor, nothing
# to install in CI); Ubuntu 24.04's own apt repository ships 3.28.3 -
# the EXACT value this line used to require, which is why raising it
# is a real cost, not a free move; and the windows-latest GitHub
# Actions runner ships 3.31.6 (confirmed against
# actions/runner-images' own Windows2025-Readme.md, not assumed from
# the Linux-only text of ESCOPO.md's own decision - a fact this
# project's own decision record did not anticipate). Both of those
# last two get an explicit CMake-install step in
# .github/workflows/ci.yml, and the ressalva is written where a
# packager on either platform actually reads it: PACKAGING.md's "CMake
# version requirement" section.
cmake_minimum_required(VERSION 4.1)

# Four components, not three (GODS_LAWS.md L-26, VER-4C): TWEAK is the
# fourth, packaging-only field. write_basic_package_version_file's
# SameMinorVersion compatibility (cmake/GlintfxInstall.cmake) only
# compares MAJOR.MINOR, so a bare `find_package(glintfx 0.1)` still
# resolves this exact release under the current pre-1.0 policy.
project(glintfx VERSION 0.2.0.0 LANGUAGES CXX)

# C, under UNIX only (WL-PROTO, GODS_LAWS.md L-05): the private-code
# half of a wayland-scanner binding (cmake/GlintfxWaylandProtocols.cmake)
# is generated C, never C++ - enable_language() has to run before ANY
# target compiles a .c source, so it belongs here, at the top of the
# root file, not buried inside the module that first needs it. Windows
# has no Wayland backend by design, so the C toolchain this project
# needs stays opt-in to the platform that actually uses it.
if(UNIX)
    enable_language(C)
endif()

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
include(GlintfxPkgConfigValidate)

# UNIX only (GODS_LAWS.md L-05): pkg_check_modules(... REQUIRED ...)
# inside this module would hard-fail configure on a platform with no
# libwayland-client, which is correct on Linux/Wayland and wrong
# everywhere else - Windows never reaches this include.
if(UNIX)
    include(GlintfxWaylandProtocols)
endif()

# Built unconditionally, not gated by GLINTFX_BUILD_TESTS (GL-LOADER,
# GODS_LAWS.md L-07 EXCECAO No 1): src/render/CMakeLists.txt needs the
# gl_registry_codegen executable target to exist to generate the GL 3.3
# core function table at build time, whether or not the test suite is
# also being built.
add_subdirectory(tools/gl_registry_codegen)

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

    # PKG-WIN-SCOPE, revertido por ordem do lider: a fatia original
    # (decisao do CTO, ja assinada) tinha guardado as duas chamadas
    # abaixo atras de if(UNIX), reagindo ao fato de que
    # cmake/GlintfxPkgConfigValidateInstalled.cmake.in so conhecia o
    # glob libglintfx.so*/libglintfx.a e reprovaria falso contra
    # glintfx.lib no Windows. O lider derrubou essa decisao ao acordar
    # e ordenou "o .pc volta a ser instalado no Windows" - o glob do
    # validador foi ensinado a reconhecer glintfx.lib (proximo
    # comentario, cmake/GlintfxPkgConfigValidateInstalled.cmake.in), e
    # esta guarda de plataforma saiu por inteiro: as duas chamadas
    # rodam em TODA plataforma, sem excecao, de novo.
    #
    # After include(GlintfxWaylandProtocols) above (UNIX only): PKG-DIST's
    # glintfx_install_pkgconfig() reads GlintfxWaylandClient_LIBRARIES,
    # which that include() populates at its own include-time
    # pkg_check_modules() call - but only under if(UNIX), well before
    # this line runs; glintfx_compute_pkgconfig_libs_private()
    # (cmake/GlintfxInstall.cmake) already guards its OWN read of that
    # variable with if(UNIX) internally, so calling
    # glintfx_install_pkgconfig() unconditionally here is safe on
    # Windows too - it just emits an empty Libs.private there, same as
    # it always would have if this outer guard had never existed.
    glintfx_install_pkgconfig()
    # After glintfx_install_pkgconfig(), on purpose (PKG-VALIDATE):
    # install() rules run in the order they are registered, and this
    # step has to find glintfx.pc already written to disk when it runs
    # at `cmake --install` time.
    glintfx_register_pkgconfig_validation()
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
