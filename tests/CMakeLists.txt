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
