# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/package/CMakeLists.txt
#
# Single subject: standalone CMake project that proves
# find_package(glintfx) outside the glintfx tree (FUND-1, item 9 of the
# service order). Is NOT included by the main build via
# add_subdirectory; runs as an independent configure/build, pointing
# CMAKE_PREFIX_PATH at the installed prefix.

cmake_minimum_required(VERSION 3.28)

project(glintfx_consumer LANGUAGES CXX)

# VERSAO PEDIDA: acompanha o segundo numero do projeto, e NAO e um detalhe.
# A politica instalada e SameMinorVersion enquanto o primeiro numero for
# zero (cmake/GlintfxInstall.cmake, GODS_LAWS.md L-26: "B e onde a quebra
# mora"), entao pedir 0.1 contra uma biblioteca 0.2 e CORRETAMENTE recusado
# -- foi assim que a subida para 0.2.0.0 em 05/09/2026 reprovou este portao,
# provando que a politica de versao esta de fato implementada e mordendo, e
# nao apenas declarada num comentario. Ao subir o segundo numero de novo,
# esta linha sobe junto; ao chegar na 1.0, a politica vira SameMajorVersion
# e esta linha passa a acompanhar o PRIMEIRO numero.
find_package(glintfx 0.2 REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE glintfx::glintfx)
set_target_properties(consumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# WIN-HANG: this project stands in for an EXTERNAL consumer of the
# installed glintfx package (tools/ci/check-consume.ps1 runs it after
# `cmake --install`). On Windows, glintfx.dll installs under
# <prefix>/bin/ while consumer.exe builds under its own build tree - two
# different directories, same root cause as cmake/GlintfxTest.cmake's
# glintfx_copy_runtime_dlls_after_build() for the in-tree tests, and the
# fix a real external consumer would also need to write: copy the
# runtime DLLs $<TARGET_RUNTIME_DLLS:consumer> resolves to (glintfx::glintfx
# is an IMPORTED target here, with IMPORTED_LOCATION set by
# glintfxTargets.cmake, which is exactly what this generator expression
# needs) next to consumer.exe.
#
# Guard is the imported target's OWN type, not a BUILD_SHARED_LIBS
# variable: this is a standalone top-level project (its own
# BUILD_SHARED_LIBS, if any, describes ITS OWN targets, not whatever
# mode the glintfx package it just found_package()'d was actually built
# and installed as). Querying TYPE on glintfx::glintfx directly is the
# only way from here to know if there is a DLL at all - an empty
# $<TARGET_RUNTIME_DLLS:...> list would make copy_if_different itself
# fail with no source files, so the static-package case must be skipped
# outright, not just left to resolve to nothing.
get_target_property(_glintfx_consumer_target_type glintfx::glintfx TYPE)
if(WIN32 AND _glintfx_consumer_target_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:consumer>" "$<TARGET_FILE_DIR:consumer>"
        COMMAND_EXPAND_LISTS
    )
endif()
unset(_glintfx_consumer_target_type)
