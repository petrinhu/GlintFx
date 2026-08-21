# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/CMakeLists.txt
#
# Single subject: the glintfx test suite, its own harness and the
# cases that link against it. tests/package/ is a standalone,
# purpose-built CMake project (proves find_package outside the tree);
# it does NOT enter here, by design.

add_library(glintfx_test_harness STATIC
    harness/test_registry.cpp
    harness/check.cpp
    harness/harness_main.cpp
)
target_include_directories(glintfx_test_harness PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
glintfx_apply_compile_options(glintfx_test_harness)

glintfx_add_test(version_test)

# Visibility test: only makes sense with dynamic symbols (.so) and the
# Unix `nm -D` tool. On Windows the export is already controlled by
# __declspec via generate_export_header; proving that would require
# dumpbin, out of scope for this slice.
if(BUILD_SHARED_LIBS AND UNIX)
    add_test(
        NAME visibility_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_exports.sh" "$<TARGET_FILE:glintfx>"
    )
endif()

# Layer gate (GODS_LAWS.md L-19, item 1: "a CI gate reproves the
# violation"). POSIX sh; declared downgrade: runs in the Linux jobs of
# the matrix, not on Windows, which does not have `sh` by default.
if(UNIX)
    add_test(
        NAME layers_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.sh" "${CMAKE_SOURCE_DIR}"
    )
endif()

# Consumption gate (FIX-CONSUMO, achados A2/A3/A4/A6): proves the
# INSTALLED package, not the in-tree build. Installs whatever this
# build's BUILD_SHARED_LIBS mode already produced into a scratch prefix,
# then configures/builds/runs the standalone tests/package/ consumer
# against it via find_package(glintfx). POSIX sh; same declared downgrade
# as layers_test above (Windows coverage is a dedicated CI step, not a
# ctest case, because it does not have `sh` by default).
if(UNIX)
    add_test(
        NAME consume_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_consume.sh"
            "${PROJECT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/package"
            "${CMAKE_CXX_COMPILER}"
    )
endif()

# Embedding gate (FIX-CONSUMO, achado A7; FIX-CONSUMO-2, achado QA-2):
# proves glintfx works consumed via add_subdirectory (the shape
# FetchContent takes after populating the tree), not just via the
# installed package above, and that the GLINTFX_INSTALL=ON opt-in path
# genuinely installs glintfx's headers and CMake package when an
# embedding consumer asks for it. POSIX sh; same declared downgrade as
# layers_test/consume_test (no Windows-specific code path is involved in
# the embedding fix, so Linux coverage across the four Linux CI targets
# is the proof; see fix-consumo.md).
if(UNIX)
    add_test(
        NAME embed_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_embed.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/embed"
            "${CMAKE_CXX_COMPILER}"
    )
endif()

# Non-default CMAKE_INSTALL_INCLUDEDIR gate (FIX-CONSUMO-2, achado QA-1):
# proves the exported CMake target set (glintfxTargets.cmake) tracks a
# non-default include directory on its own, with no other install() rule
# masking a regression of it. Configures and installs a FRESH glintfx
# build of its own (the include dir is fixed at glintfx's own configure
# time, unlike consume_test/embed_test above, which reuse whatever build
# this ctest run is already part of). POSIX sh; same declared downgrade
# as the other consumption gates above.
if(UNIX)
    add_test(
        NAME install_includedir_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_install_includedir.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CXX_COMPILER}"
    )
endif()
