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

# Wayland protocol binding link gate (WL-PROTO, GODS_LAWS.md L-05/L-07):
# proves the wayland-scanner build step (cmake/GlintfxWaylandProtocols.cmake,
# src/platform/wayland/) actually turns the SYSTEM xdg-shell.xml into a
# header this TU can #include and a symbol it can reference. Linux/
# Wayland-only slice, same declared downgrade as the other if(UNIX)
# gates in this file: Windows has no Wayland backend by design
# (GODS_LAWS.md L-05).
#
# glintfx_add_wayland_xdg_shell_binding() is called a SECOND time here,
# on wl_proto_link_test itself, not only on glintfx_library
# (src/platform/wayland/CMakeLists.txt) - deliberately, and NOT a
# duplicate of that call: wayland-scanner's private-code mode marks
# every generated interface WL_PRIVATE, i.e.
# __attribute__((visibility("hidden"))) (measured live in the
# generated xdg-shell-protocol.c; wayland-scanner's own --help text
# calls private-code "strongly recommended" over public-code for
# exactly this reason - GODS_LAWS.md L-19 wants the same thing, the
# public surface must not move). A HIDDEN symbol never enters a shared
# library's dynamic symbol table, so a SEPARATE executable linking
# against glintfx::glintfx cannot resolve xdg_wm_base_interface across
# that .so boundary - proven live: the first cut of this gate linked
# only against the library and failed with "undefined reference to
# `xdg_wm_base_interface'" even though the symbol existed, HIDDEN,
# inside libglintfx.so's own object. The fix respects the hidden
# visibility instead of fighting it (no hand-written edit of the
# generated file, no switch to public-code, no export macro added to
# a 100%-generated .c - any of those would either violate "no
# hand-written C line" or reopen the exact cross-DSO symbol-collision
# risk private-code exists to avoid): the function is idempotent
# (cmake/GlintfxWaylandProtocols.cmake reuses the same generated
# .h/.c the moment they already exist) and simply compiles the SAME
# generated xdg-shell-protocol.c a second time, into THIS executable's
# own object set - exactly the shape a real Wayland application takes
# when it wants to unit-test its own protocol binding: header and
# private-code compiled into the SAME link unit, no DSO boundary
# involved. This still proves everything the fatia's scope calls for:
# the generation step runs from the system XML, the header is reachable
# from a plain C++ TU wrapped in extern "C", and the compiled data is
# correct (name "xdg_wm_base") - it does not, and should not, prove
# that a hidden private-code symbol crosses a library boundary, because
# GODS_LAWS.md L-19 does not want it to.
if(UNIX)
    glintfx_add_test(wl_proto_link_test)
    glintfx_add_wayland_xdg_shell_binding(wl_proto_link_test)
endif()

# Header hygiene gate (HDR-HYGIENE-FIX, reproving HDR-HYGIENE): proves
# a public header still compiles and behaves after hostile system
# headers are included first (sys/sysmacros.h on Linux, windows.h
# without NOMINMAX/WIN32_LEAN_AND_MEAN on Windows). See the
# CONTRACT comment at the top of header_hygiene_test.cpp.
glintfx_add_test(header_hygiene_test)

# Header hygiene COVERAGE gate (HDR-HYGIENE-FIX-2): proves the
# "every *.hpp under include/glintfx/ has a literal #include line in
# header_hygiene_test.cpp" contract mechanically, instead of trusting
# a human to read the comment at the top of that file - the previous
# version of that comment made exactly that claim and nothing ever
# enforced it (see tests/tools/check_hygiene_coverage.sh's own header
# for the declared generated-header limitation). POSIX sh; same
# declared downgrade as every other gate in this file.
if(UNIX)
    add_test(
        NAME hygiene_coverage_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_hygiene_coverage.sh"
            "${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/header_hygiene_test.cpp"
    )
    set_tests_properties(hygiene_coverage_test PROPERTIES LABELS consume)

    add_test(
        NAME hygiene_coverage_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_hygiene_coverage.sh" --selftest
    )
    set_tests_properties(hygiene_coverage_selftest PROPERTIES LABELS selftest)
endif()

# Visibility test: only makes sense with dynamic symbols (.so) and the
# Unix `nm -D` tool. On Windows the export is already controlled by
# __declspec via generate_export_header; proving that would require
# dumpbin, out of scope for this slice.
if(BUILD_SHARED_LIBS AND UNIX)
    add_test(
        NAME visibility_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_exports.sh" "$<TARGET_FILE:glintfx_library>"
    )
    set_tests_properties(visibility_test PROPERTIES LABELS consume)
endif()

# Layer gate (GODS_LAWS.md L-19, item 1: "a CI gate reproves the
# violation"). POSIX sh; declared downgrade: runs in the Linux jobs of
# the matrix, not on Windows, which does not have `sh` by default.
if(UNIX)
    add_test(
        NAME layers_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.sh" "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(layers_test PROPERTIES LABELS consume)
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
    set_tests_properties(consume_test PROPERTIES LABELS consume)
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
    set_tests_properties(embed_test PROPERTIES LABELS consume)
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
    set_tests_properties(install_includedir_test PROPERTIES LABELS consume)
endif()

# Packager layout gate (FIX-CONSUMO-2, achado QA-4): proves
# find_package(glintfx) resolves a Debian-multiarch-style install
# (CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR both non-default),
# the scenario the original auditoria (A2/A6) named. Configures and
# installs a FRESH glintfx build of its own, same reason as
# install_includedir_test above. POSIX sh; same declared downgrade as
# the other consumption gates above.
if(UNIX)
    add_test(
        NAME install_packager_layout_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_install_packager_layout.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/package"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(install_packager_layout_test PROPERTIES LABELS consume)
endif()

# Output name gate (FIX-CONSUMO-3, achado QA-2-2): proves the
# OUTPUT_NAME contract in cmake/GlintfxLibrary.cmake - the compiled
# artifact basename (libglintfx.so*/libglintfx.a), not the internal
# CMake target name glintfx_library - by installing whatever this
# build's BUILD_SHARED_LIBS mode already produced into a scratch prefix
# and raw-linking (-lglintfx, no CMake, no find_package, no
# glintfx::glintfx target) against it. None of the other consumption
# gates above go through this path: they all resolve the library via
# the CMake target, so none of them would notice OUTPUT_NAME regressing.
# POSIX sh; same declared downgrade as the other consumption gates above.
if(UNIX)
    add_test(
        NAME output_name_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_output_name.sh"
            "${PROJECT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/raw_link/main.cpp"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(output_name_test PROPERTIES LABELS consume)
endif()

# Target name collision gate (FIX-CONSUMO-2, achado QA-3): proves an
# embedding consumer can declare its own `glintfx` target without
# hitting CMake Error ... policy CMP0002 against glintfx's own internal
# library target (renamed to glintfx_library; see src/CMakeLists.txt).
# POSIX sh; same declared downgrade as the other consumption gates above.
if(UNIX)
    add_test(
        NAME no_target_collision_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_target_collision.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/embed_name_collision"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(no_target_collision_test PROPERTIES LABELS consume)
endif()

# preci.sh selftest gate (FUND-4): proves the LOCAL quality gate proves
# itself (three controls - clean fixture passes, dirty fixture is
# reproved, empty directory fails the enumeration floor - see
# tools/preci.sh and tests/preci_fixtures/). Registered ONLY when the
# three tools tools/preci.sh's cheap stages need are actually present
# (clang-format, clang-tidy, cppcheck): the four Linux CI matrix jobs in
# `linux` (Fedora/Ubuntu/Arch/CachyOS) install a plain C++ toolchain, not
# the LLVM tooling - registering this test unconditionally would turn a
# missing tool on those jobs into a spurious ctest failure that has
# nothing to do with the code. The dedicated `lint` CI job (Fedora 44,
# GODS_LAWS.md L-04 primary target) DOES install the three tools, and a
# developer's own machine typically has them too (TESTES.md T15.0); both
# get this test for free. POSIX sh (bash); same declared Windows
# downgrade as every other gate in this file.
if(UNIX)
    find_program(GLINTFX_CLANG_FORMAT_EXE clang-format)
    find_program(GLINTFX_CLANG_TIDY_EXE clang-tidy)
    find_program(GLINTFX_CPPCHECK_EXE cppcheck)
    if(GLINTFX_CLANG_FORMAT_EXE AND GLINTFX_CLANG_TIDY_EXE AND GLINTFX_CPPCHECK_EXE)
        add_test(
            NAME preci_selftest
            COMMAND "${PROJECT_SOURCE_DIR}/tools/preci.sh" --selftest
        )
        set_tests_properties(preci_selftest PROPERTIES LABELS selftest)
    else()
        message(STATUS
            "preci_selftest not registered: clang-format/clang-tidy/cppcheck "
            "not all found (TESTES.md T15.0 installs them)."
        )
    endif()
endif()
