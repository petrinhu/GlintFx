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
glintfx_add_test(err_code_test)
glintfx_add_test(err_no_alloc_test)
glintfx_add_test(err_context_test)
glintfx_add_test(rslt_test)
glintfx_add_test(err_format_test)

# CORE-COLOR (TODO.md, GODS_LAWS.md L-19/L-26): frozen layout of
# gltfx_rgba plus the round-trip/premultiply conversions - see
# color_test.cpp's own header comment.
glintfx_add_test(color_test)

# GFSS-TOKEN (TODO.md, GODS_LAWS.md L-19/L-20/L-28): the gfss tokenizer's
# closed token-class vocabulary and its two value types - see
# gfss_token_test.cpp's own header comment.
glintfx_add_test(gfss_token_test)

# GFSS-TOKEN: the CSS-core scanning algorithm itself, on top of the
# types above - see gfss_tokenizer_test.cpp's own header comment.
#
# GODS_LAWS.md L-40 achado 2 of 26/08/2026 ("a guarda de progresso so
# protege em depuracao"): this test also needs
# src/gfss/token_progress_guard.hpp, a PRIVATE header, to exercise the
# zero-progress guard's own predicate directly - the SAME
# "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir
# gl_proc_address_assign_test.cpp already adds for render/
# gl_proc_address.hpp above, added here on top of the target
# glintfx_add_test() just created (the same pattern wl_proto_link_test
# uses right below, calling a second setup function on an
# already-created test target).
glintfx_add_test(gfss_tokenizer_test)
target_include_directories(gfss_tokenizer_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md L-19/L-20/L-28/L-40): hex/
# rgb()/rgba()/hsl()/hsla()/named-keyword color parsing built on top of
# GFSS-TOKEN's own token stream - see gfss_color_parse_test.cpp's own
# header comment. Needs src/gfss/color_parse.hpp and src/gfss/
# named_colors.hpp, both PRIVATE headers not yet promoted to
# include/glintfx/ (GODS_LAWS.md L-19: "o header nasce interno") - the
# SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir gfss_tokenizer_test
# above already adds for its own private headers.
glintfx_add_test(gfss_color_parse_test)
target_include_directories(gfss_color_parse_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# color_parse.cpp/named_colors.cpp define glintfx::style::detail::
# parse_color()/named_color_count()/named_color_at()/
# lookup_named_color() with NO GLINTFX_API - deliberately (GODS_LAWS.md
# L-19: not yet a public, ABI-frozen symbol), which means glintfx_library
# (a SHARED library, hidden default visibility) never puts them in its
# dynamic symbol table. This test needs to call them DIRECTLY, so it
# compiles the SAME two .cpp files a SECOND time, straight into its own
# object set - the identical technique wl_proto_link_test above already
# uses for the generated Wayland protocol binding (see that test's own
# comment for why "compile it again into this executable" is correct
# here and not an ODR risk: two SEPARATE link units, the .so's hidden
# copy is never visible to the executable's linker pass at all).
target_sources(gfss_color_parse_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/color_parse.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/named_colors.cpp"
)

# GFSS-SEL-PARSE-CORE (TODO.md, GODS_LAWS.md L-19/L-20/L-28/L-40):
# selector grammar core (tag/.class/#id/*, compound, the four
# combinators, comma-separated list, every argument-less pseudo-class
# and the five functional ones recognized by name with a raw argument)
# built on top of GFSS-TOKEN's own token stream - see gfss_selector_
# parse_test.cpp's own header comment. Needs src/gfss/selector_parse.hpp
# and its own private siblings (selector_ast.hpp, selector_pseudo_
# vocabulary.hpp, selector_diagnostic_vocabulary.hpp), none promoted to
# include/glintfx/ (GODS_LAWS.md L-19: "o header nasce interno") - the
# SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir gfss_tokenizer_test/
# gfss_color_parse_test above already add for their own private headers.
glintfx_add_test(gfss_selector_parse_test)
target_include_directories(gfss_selector_parse_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# selector_parse.cpp defines glintfx::style::detail::parse_selector_list()
# with NO GLINTFX_API - deliberately (GODS_LAWS.md L-19: not yet a
# public, ABI-frozen symbol) - the SAME reason gfss_color_parse_test
# above compiles color_parse.cpp/named_colors.cpp a second time instead
# of linking the hidden copy inside glintfx_library's own .so. This
# test needs named_colors.hpp's own ascii_case_insensitive_equal() too
# (selector_pseudo_vocabulary.hpp reuses it), but that function is
# fully defined INLINE in the header (no .cpp counterpart to compile
# in) - only selector_parse.cpp itself needs a second compile here.
target_sources(gfss_selector_parse_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/selector_parse.cpp"
)

# GODS_LAWS.md L-40, INBOX achado 1 of 26/08/2026 ("o portao passa, mas
# nao diz o que verificou"): glintfx_test::format_patch_diagnostic()
# lives OUTSIDE harness/win_dll_alloc_hook.hpp's `#if defined(_WIN32)`
# guard on purpose, so this cross-platform gate can prove it red/green
# here - see win_dll_alloc_hook_format_test.cpp's own header comment
# for the declared downgrade on the Windows-only half it does not
# touch.
glintfx_add_test(win_dll_alloc_hook_format_test)

# GODS_LAWS.md L-40, INBOX achado 2 of 26/08/2026 ("restauracao de
# protecao de memoria sem conferir retorno"):
# glintfx_test::detail::format_protect_restore_failure() lives OUTSIDE
# the same guard for the same reason - see
# win_dll_alloc_hook_protect_restore_format_test.cpp's own header
# comment.
glintfx_add_test(win_dll_alloc_hook_protect_restore_format_test)

# GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1): the minimal XML
# pull reader tools/gl_registry_codegen/xml_reader.hpp/.cpp uses to walk
# the vendored gl.xml at build time - see gl_xml_reader_test.cpp's own
# header comment. Linked against gl_registry_codegen_lib, NOT
# glintfx::glintfx (this tool never enters the shipped library), so it
# is registered directly instead of through glintfx_add_test().
add_executable(gl_xml_reader_test gl_xml_reader_test.cpp)
target_link_libraries(gl_xml_reader_test PRIVATE gl_registry_codegen_lib glintfx_test_harness)
glintfx_apply_compile_options(gl_xml_reader_test)
add_test(NAME gl_xml_reader_test COMMAND gl_xml_reader_test)
set_tests_properties(gl_xml_reader_test PROPERTIES LABELS unit)

# GL-LOADER: gl.xml's own vocabulary (<commands>, <feature>, <require>,
# <remove>) resolved into the plain domain model - see
# gl_registry_parser_test.cpp's own header comment.
add_executable(gl_registry_parser_test gl_registry_parser_test.cpp)
target_link_libraries(gl_registry_parser_test PRIVATE gl_registry_codegen_lib glintfx_test_harness)
glintfx_apply_compile_options(gl_registry_parser_test)
add_test(NAME gl_registry_parser_test COMMAND gl_registry_parser_test)
set_tests_properties(gl_registry_parser_test PROPERTIES LABELS unit)

# GL-LOADER: the ONE hand-written, tested mechanism the GENERATED GL
# 3.3 core loader calls 344 times - see gl_proc_address_assign_test.cpp
# and src/render/gl_proc_address.hpp's own header comments. Header-only
# (no .cpp, no glintfx_library dependency): PROJECT_SOURCE_DIR/src is
# added directly so "render/gl_proc_address.hpp" resolves the same way
# the GENERATED gl_functions.cpp will reach it (src/render/CMakeLists.txt
# adds that same directory as a PRIVATE include dir on glintfx_library).
add_executable(gl_proc_address_assign_test gl_proc_address_assign_test.cpp)
target_include_directories(gl_proc_address_assign_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gl_proc_address_assign_test PRIVATE glintfx_test_harness)
glintfx_apply_compile_options(gl_proc_address_assign_test)
add_test(NAME gl_proc_address_assign_test COMMAND gl_proc_address_assign_test)
set_tests_properties(gl_proc_address_assign_test PROPERTIES LABELS unit)

# GL-LOADER: the resolved GL 3.3 core command list rendered into the
# TEXT of the two generated files - see gl_loader_codegen_test.cpp's own
# header comment.
add_executable(gl_loader_codegen_test gl_loader_codegen_test.cpp)
target_link_libraries(gl_loader_codegen_test PRIVATE gl_registry_codegen_lib glintfx_test_harness)
glintfx_apply_compile_options(gl_loader_codegen_test)
add_test(NAME gl_loader_codegen_test COMMAND gl_loader_codegen_test)
set_tests_properties(gl_loader_codegen_test PROPERTIES LABELS unit)

# GL-LOADER: hand-written SHA-256 (GODS_LAWS.md L-07: no OpenSSL/
# libcrypto dependency for this build-time tool either) - see
# gl_sha256_test.cpp's own header comment.
add_executable(gl_sha256_test gl_sha256_test.cpp)
target_link_libraries(gl_sha256_test PRIVATE gl_registry_codegen_lib glintfx_test_harness)
glintfx_apply_compile_options(gl_sha256_test)
add_test(NAME gl_sha256_test COMMAND gl_sha256_test)
set_tests_properties(gl_sha256_test PROPERTIES LABELS unit)

# GL-LOADER (GODS_LAWS.md L-40): the CLI itself, end to end, proving the
# three required controls at the process-exit-code level - positive
# (the real vendored file with its real, matching sha256 succeeds),
# negative (a deliberately wrong --expect-sha256 reproves, GODS_LAWS.md
# L-07 EXCECAO No 1 obligation 3), and empty-scan (a well-formed
# registry that resolves ZERO GL functions reproves, never a silent
# success - the exact defect class L-40 exists to forbid). WILL_FAIL
# TRUE is CTest's own mechanism for "this process MUST exit non-zero",
# no shell script needed. GLINTFX_GL_XML_SHA256 comes from src/render/
# CMakeLists.txt's own file(SHA256 ...) call (CACHE INTERNAL, computed
# once, single source of truth with the real build wiring).
add_test(
    NAME gl_registry_codegen_real_vendored_file_succeeds_test
    COMMAND gl_registry_codegen
        "${PROJECT_SOURCE_DIR}/third_party/khronos/gl.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/gl_registry_codegen_cli_out/real"
        "3.3"
        "--expect-sha256=${GLINTFX_GL_XML_SHA256}"
)
set_tests_properties(gl_registry_codegen_real_vendored_file_succeeds_test PROPERTIES LABELS unit)

add_test(
    NAME gl_registry_codegen_sha_mismatch_reproves_test
    COMMAND gl_registry_codegen
        "${PROJECT_SOURCE_DIR}/third_party/khronos/gl.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/gl_registry_codegen_cli_out/sha_mismatch"
        "3.3"
        "--expect-sha256=0000000000000000000000000000000000000000000000000000000000000000"
)
set_tests_properties(gl_registry_codegen_sha_mismatch_reproves_test PROPERTIES LABELS unit WILL_FAIL TRUE)

add_test(
    NAME gl_registry_codegen_empty_registry_reproves_test
    COMMAND gl_registry_codegen
        "${CMAKE_CURRENT_SOURCE_DIR}/gl_registry_codegen_fixtures/empty_registry.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/gl_registry_codegen_cli_out/empty"
        "3.3"
)
set_tests_properties(gl_registry_codegen_empty_registry_reproves_test PROPERTIES LABELS unit WILL_FAIL TRUE)

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

# Public name collision gate (docs/api-conventions.md R6; TODO.md
# "Desvios", 25/08/2026 achado: "a auditoria de colisao de nomes
# publicos usa LISTA CURADA, nao varredura do sistema real"). Replaces
# trusting a hand-typed name list with a mechanical scan: every name
# check_public_name_collision.sh's own enumerate_names.awk finds under
# PROJECT_SOURCE_DIR/include, checked against every "#define" the
# compiler's OWN system search path (discovered live via
# `${cxx} -E -Wp,-v -xc++ -`, never a hardcoded /usr/include) actually
# contains. POSIX sh, and the discovery trick is a GCC-frontend
# convention; same declared downgrade as every other gate in this file
# - runs on the four Linux CI legs, not Windows (see the script's own
# header for why the Windows leg is covered by a DIFFERENT, pre-
# existing mechanism, header_hygiene_test.cpp's hostile-windows.h
# compile, that this gate does not replace).
if(UNIX)
    add_test(
        NAME public_name_collision_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_public_name_collision.sh"
            "${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(public_name_collision_test PROPERTIES LABELS consume)

    add_test(
        NAME public_name_collision_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_public_name_collision.sh" --selftest "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(public_name_collision_selftest PROPERTIES LABELS selftest)
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

    # GODS_LAWS.md L-40 achado (revisao adversarial, 25/08/2026):
    # layers_test was the ONLY test wired to this gate, and always ran
    # against the real repo root (never empty) - the empty-scan floor
    # inside check_layers.sh was never exercised by the suite. Wiring
    # --selftest here closes that gap, same consume/selftest label
    # taxonomy as the other gates in this file.
    add_test(
        NAME layers_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.sh" --selftest
    )
    set_tests_properties(layers_selftest PROPERTIES LABELS selftest)
endif()

# L-05/L-06 gate (GODS_LAWS.md, "Gate L-05/L-06" job of
# .github/workflows/ci.yml): proves the repository has no forbidden
# term (see tests/tools/check_no_x11.sh's own header for the L-40
# findings this gate exists to close). Wiring it here too, not only in
# the standalone CI job, gives a local `ctest -R x11` and folds it into
# the same consume/selftest label taxonomy as the other gates in this
# file. POSIX sh; same declared downgrade as layers_test above.
if(UNIX)
    add_test(
        NAME x11_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_x11.sh" "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(x11_test PROPERTIES LABELS consume)

    add_test(
        NAME x11_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_x11.sh" --selftest
    )
    set_tests_properties(x11_selftest PROPERTIES LABELS selftest)
endif()

# Duplicated-law gate (GODS_LAWS.md / ESCOPO.md separacao LEI/ESCOPO,
# 26/08/2026 - proposta em
# /var/tmp/glintfx-plan/separacao-leis-escopo.md secao 5, executada na
# fase 2): proves the DUP-BLOCK anchored text in GODS_LAWS.md and
# ESCOPO.md stays byte-for-byte identical (see
# tests/tools/check_dup_laws.sh's own header for the two motives of
# duplication - decisao do lider vs. duvida do orquestrador). POSIX
# sh; same declared downgrade as layers_test/x11_test above.
if(UNIX)
    add_test(
        NAME dup_laws_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dup_laws.sh" "${CMAKE_SOURCE_DIR}"
    )
    set_tests_properties(dup_laws_test PROPERTIES LABELS consume)

    add_test(
        NAME dup_laws_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dup_laws.sh" --selftest
    )
    set_tests_properties(dup_laws_selftest PROPERTIES LABELS selftest)
endif()

# [[nodiscard]] gate (CE-4 of CORE-ERROR, TODO.md): proves
# glintfx::gltfx_rslt<T>'s [[nodiscard]] is a real compiler diagnostic
# by compiling a fixture that drops the result (must FAIL) and one that
# uses it (must succeed) - see check_nodiscard_rslt.sh's own header.
# Two include dirs, not one (same pair glintfx_set_target_include_dirs()
# gives every real target at BUILD_INTERFACE, cmake/GlintfxLibrary.cmake):
# err.hpp pulls in the GENERATED glintfx/export.hpp for GLINTFX_API, which
# only exists under GLINTFX_GENERATED_INCLUDE_DIR, not under the
# committed include/ tree. Compile-only (-c in the script, no link), so
# no glintfx build artifact is needed here at all - the same reason
# PROJECT_SOURCE_DIR/include is passed directly rather than an installed
# prefix. POSIX sh; same declared downgrade as layers_test above.
if(UNIX)
    add_test(
        NAME nodiscard_rslt_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_nodiscard_rslt.sh"
            "${PROJECT_SOURCE_DIR}/include"
            "${GLINTFX_GENERATED_INCLUDE_DIR}"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(nodiscard_rslt_test PROPERTIES LABELS consume)
endif()

# Debug-only precondition guard (correction of 25/08/2026, GODS_LAWS.md
# CORE-ERROR): proves BOTH halves of gltfx_rslt<T>::value()/error()'s
# assert()-based precondition check - stops deterministically with a
# message in a Debug-flags compile (NDEBUG undefined), costs nothing
# and changes nothing in a Release-flags compile (NDEBUG defined) - see
# check_rslt_precondition.sh's own header for the two independent UB
# shapes (primary template vs. the T=void specialization) this proves
# separately, live, rather than assumed identical. Needs the ALREADY-
# BUILT library's directory (unlike nodiscard_rslt_test above, this
# fixture links and RUNS, it does not stop at -c) -
# $<TARGET_FILE_DIR:glintfx_library> resolves to the right directory in
# both BUILD_SHARED_LIBS modes; a static-mode -Wl,-rpath entry with
# nothing to resolve there is harmless, not an error. POSIX sh; same
# declared downgrade as layers_test above.
if(UNIX)
    add_test(
        NAME rslt_precondition_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_rslt_precondition.sh"
            "${PROJECT_SOURCE_DIR}/include"
            "${GLINTFX_GENERATED_INCLUDE_DIR}"
            "$<TARGET_FILE_DIR:glintfx_library>"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(rslt_precondition_test PROPERTIES LABELS consume)
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
# layers_test/consume_test above: runs in the Linux jobs of the matrix,
# not on Windows, which does not have `sh` by default. Windows coverage
# of this same embedding path is a dedicated CI step, not a ctest case,
# same shape as consume_test's own Windows counterpart above -
# tools/ci/check-embed.ps1 proves the identical four things (configure/
# build/run via add_subdirectory, generated headers scoped under the
# embedded build dir, GLINTFX_INSTALL default not leaking headers, and
# the GLINTFX_INSTALL=ON opt-in genuinely installing them), invoked from
# the `windows` job in .github/workflows/ci.yml (CI-CONSUME).
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

# EMBED-DLL decision-logic gate: proves glintfx_colocate_embedded_
# runtime_dll() and glintfx_embedded_runtime_colocate_default()
# (cmake/GlintfxLibrary.cmake / cmake/GlintfxOptions.cmake) against
# every branch that matters - the escape valve, and all eight
# PROJECT_IS_TOP_LEVEL x WIN32 x BUILD_SHARED_LIBS combinations - see
# tests/embed_dll_colocation/CMakeLists.txt's own header for the full
# reasoning. Registered WITHOUT if(UNIX), on purpose, unlike every
# other consumption gate in this file: this one has zero shell/POSIX
# dependency at all (LANGUAGES NONE in the fixture means the whole
# assertion is a plain `cmake -S/-B` configure), so it is genuinely
# cross-platform and runs on the Windows job too, catching a regression
# to the decision logic before the far more expensive full MSVC
# configure/build/link/run cycle (check-embed.ps1's Invoke-RunEmbed)
# even starts. A fixed, deterministic build directory under this
# project's own binary tree, not a scratch mktemp -d: there is nothing
# to build or run here (LANGUAGES NONE), so a stale artifact from a
# previous run cannot mask this run's RESULT the way it could for a
# compiled fixture.
#
# --fresh (EMBED-DLL, achado local, tools/preci.sh --selftest): the
# claim that used to sit on this line - "re-configuring an existing
# directory is idempotent" - was WRONG, caught by actually running
# preci.sh, not by inspection. CMake refuses to reconfigure an EXISTING
# build directory whose cache already recorded a DIFFERENT generator
# ("CMake Error: generator : Ninja / Does not match the generator used
# previously: Unix Makefiles"): a stale build-preci/tests/
# embed_dll_colocation_check/ from before -G "${CMAKE_GENERATOR}"
# existed on this line had defaulted, silently, to Unix Makefiles (this
# machine has `make`, unlike the Arch CI job) - reproduced live, not
# guessed, by inspecting that stale CMakeCache.txt's own
# CMAKE_GENERATOR entry and its mtime, hours older than the fix.
# --fresh (CMake >= 3.24; this project requires >= 3.28) wipes and
# reconfigures every single ctest run of this test, closing the whole
# CLASS of "fixed path + stale cached setting" bug this same slice
# already hit for the generator - not just this one instance of it.
#
# -G "${CMAKE_GENERATOR}" (EMBED-DLL, achado do CI real, Arch nos dois
# modos): a configuracao aninhada e um PROCESSO INDEPENDENTE que NAO
# herda nada do build pai - nem gerador, nem compilador, nem caminho,
# nem variavel de cache; o que ela precisa, ela precisa receber
# explicitamente (a mesma causa ja mordeu esta fatia duas vezes antes -
# CI-CONSUME, caminho relativo virando recursao; CI-CONSUME-2, o
# compilador nao herdado no Ubuntu). Sem -G, o cmake aninhado cai no
# padrao "Unix Makefiles", que so existe onde `make` esta instalado -
# a imagem do Arch (matrix.instalar em .github/workflows/ci.yml) so
# instala `ninja`, nunca `make`, e por isso reprovou so la, nos dois
# modos (compartilhado e estatico), enquanto as outras quatro
# distribuicoes tem os dois presentes e mascaravam o defeito.
# Reproduzido ao vivo, container archlinux:latest com o MESMO comando
# de instalacao do job real ("pacman -Syu --noconfirm gcc cmake ninja
# pkgconf git wayland wayland-protocols", sem make): "CMake was unable
# to find a build program corresponding to 'Unix Makefiles'" - texto
# identico ao log do CI. Corrigido, mesmo container, verde.
#
# Varredura completa do que mais este fixture aninhado assume do
# ambiente e nao recebe explicitamente (GODS_LAWS.md L-40 item 5 -
# enumeracao fechada por construcao, nao busca dirigida - todo
# `${...}` e todo include()/find_program()/find_package() em
# tests/embed_dll_colocation/CMakeLists.txt E nos dois arquivos reais
# que ele inclui, GlintfxOptions.cmake/GlintfxLibrary.cmake,
# conferidos um a um):
#   - Compilador: NAO se aplica. O fixture usa LANGUAGES NONE (nenhuma
#     linguagem habilitada, nenhum compilador procurado) - por isso o
#     Ubuntu nunca mordeu aqui, so em embed_test (achado anterior desta
#     mesma fatia, que E compilado).
#   - CMAKE_MODULE_PATH: o unico include() de nome nu nos dois arquivos
#     reais e `include(GenerateExportHeader)`, em GlintfxLibrary.cmake
#     linha 9 - TOPO DE ARQUIVO, fora de qualquer function(), logo ele
#     RODA SEMPRE que o fixture inclui aquele arquivo.
#     CORRECAO (revisao adversarial de 25/08/2026): a versao anterior
#     deste comentario dizia "dentro de uma funcao que este fixture
#     NUNCA chama". Era FALSO - conferido com awk, nao ha function()
#     antes da linha 9. A conclusao de que nao ha buraco SOBREVIVE, mas
#     por outro motivo: GenerateExportHeader e modulo NATIVO do CMake,
#     resolvido via CMAKE_ROOT embutido no proprio binario, nunca via
#     CMAKE_MODULE_PATH herdado do processo pai. Conclusao certa,
#     justificativa errada - e justificativa errada envelhece pior que
#     ausencia de justificativa, porque o proximo leitor a reusa.
#     Os dois include() que o fixture realmente executa usam caminho
#     absoluto completo (${GLINTFX_SOURCE_DIR}/cmake/...), nao nome nu.
#   - Toolchain/plataforma cruzada, CMAKE_BUILD_TYPE: NAO se aplicam -
#     LANGUAGES NONE nao compila nada, entao nenhum dos dois jamais e
#     lido em caminho nenhum deste fixture.
#   - Variavel de cache do pai: PROJECT_IS_TOP_LEVEL, WIN32,
#     BUILD_SHARED_LIBS, CMAKE_RUNTIME_OUTPUT_DIRECTORY e
#     CMAKE_BINARY_DIR sao TODAS definidas explicitamente pelo proprio
#     fixture, por caso, dentro do loop das 8 combinacoes e das 3
#     funcoes - nenhuma delas herdada implicitamente do pai.
#   - PATH: o `cmake` do processo aninhado ja vem de "${CMAKE_COMMAND}"
#     (caminho absoluto resolvido pelo pai), nao de um `cmake` nu que
#     dependeria do PATH do ambiente onde o teste roda.
add_test(
    NAME embed_dll_colocation_test
    COMMAND "${CMAKE_COMMAND}"
        --fresh
        -S "${CMAKE_CURRENT_SOURCE_DIR}/embed_dll_colocation"
        -B "${CMAKE_CURRENT_BINARY_DIR}/embed_dll_colocation_check"
        -G "${CMAKE_GENERATOR}"
        "-DGLINTFX_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
)
set_tests_properties(embed_dll_colocation_test PROPERTIES LABELS consume)

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

# pkg-config gate (PKG-DIST): proves the INSTALLED glintfx.pc, not just
# the CMake package - a bare `pkg-config --cflags --libs glintfx`
# resolves and links tests/raw_link/main.cpp with no CMake and no
# find_package(glintfx) involved at all, across six scenarios (default
# layout, Debian-multiarch, a malformed trailing-slash libdir, absolute
# install dirs, static mode with a load-bearing Libs.private proof, and
# a DESTDIR-staged Fedora-format install) - see check_pkgconfig.sh's
# own header for the full reasoning behind each, and for why the
# output-content validator it applies to all six is what makes the
# DESTDIR scenario possible at all (no expected path to precompute).
# POSIX sh; same declared downgrade as the other consumption gates
# above (pkg-config's own convention is Unix/Linux packaging; there is
# no Windows-specific coverage here because glintfx has no platform
# layer that needs Libs.private there yet - see GlintfxInstall.cmake's
# own comment at glintfx_install_pkgconfig()).
if(UNIX)
    add_test(
        NAME pkgconfig_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkgconfig.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/raw_link/main.cpp"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(pkgconfig_test PROPERTIES LABELS consume)
endif()

# Post-install pkg-config VALIDATOR gate (PKG-VALIDATE): proves the
# install(CODE) step in cmake/GlintfxPkgConfigValidate.cmake /
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in - the piece that
# runs on a PACKAGER's own machine, at `cmake --install` time, not
# only in this project's own CI - actually fires, actually resolves
# the real (including DESTDIR-staged) install location, actually
# honors both halves of its escape hatch, and fails CLOSED with a
# self-sufficient diagnostic on a real broken library artifact, a
# real missing glintfx.pc, and a hand-assembled empty-Cflags/Libs
# fixture (GODS_LAWS.md L-40), while degrading to a WARNING (not a
# FATAL_ERROR) when pkg-config itself is absent from PATH - see
# check_pkgconfig_validate.sh's own header for the eight scenarios and
# what is deliberately NOT independently re-proven. This is a
# DIFFERENT claim than pkgconfig_test above: that gate proves
# glintfx.pc's CONTENT is correct across six layouts OUR OWN CI
# enumerates; this one proves the VALIDATOR ITSELF, which is designed
# to reach layouts neither pkgconfig_test nor any of this project's CI
# jobs ever try (GODS_LAWS.md LEI ZERO: the base of consumers is
# public and unknown). POSIX sh; same declared downgrade as the other
# consumption gates above (pkg-config's own convention is Unix/Linux
# packaging; no Windows-specific coverage here for the same reason
# pkgconfig_test has none - see that gate's own comment).
if(UNIX)
    add_test(
        NAME pkgconfig_validate_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkgconfig_validate.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(pkgconfig_validate_test PROPERTIES LABELS consume)
endif()

# Blank install-dir gate (adversarial review round 3, PKG-DIST task 3):
# proves CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR are refused at
# CONFIGURE time when empty or whitespace-only
# (glintfx_require_nonblank_install_subdir, cmake/GlintfxInstall.cmake) -
# blank is never a legitimate layout (it collapses some install()
# destinations to the filesystem root, confirmed live before the
# guard existed), unlike a malformed-but-nonblank value, which still
# names a real layout somewhere and is handled by the pkg-config gate
# above instead of refused. POSIX sh; same declared downgrade as the
# other consumption gates above.
#
# ${CMAKE_CXX_COMPILER} passed through explicitly (same pattern as
# pkgconfig_test above): the script's own nested `cmake -S -B` is a
# SECOND, independent configure of the whole glintfx tree, and
# project(glintfx LANGUAGES CXX) at CMakeLists.txt:18 resolves the
# compiler before glintfx_require_nonblank_install_subdir ever runs -
# so that nested configure needs its own -DCMAKE_CXX_COMPILER, it does
# not inherit this one's. Confirmed live (GODS_LAWS.md L-14, container
# only): on the Ubuntu CI job, which installs ONLY g++-14 (no plain
# c++/g++ symlink), the un-pinned nested configure failed with "No
# CMAKE_CXX_COMPILER could be found" before ever reaching the blank-var
# guard - a false negative that hid the real assertion this test
# exists to make.
if(UNIX)
    add_test(
        NAME blank_install_dir_rejected_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_blank_install_dir_rejected.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(blank_install_dir_rejected_test PROPERTIES LABELS consume)
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

# README test-count consistency gate (DOC-DRIFT, adversarial review of
# 2026-08-26): README.md's own "Building from source" section states
# the ctest total for shared and static mode in prose ("N registered
# cases in shared mode and M in static mode") - a sentence that had
# already gone stale once, three commits earlier, with nothing to
# catch it (README/CHANGELOG said 33/32 while the real ctest -N count
# was already 34/33). check_readme_test_count.sh compares the prose
# number against THIS build's own `ctest -N` total and reproves on any
# mismatch, with GODS_LAWS.md L-40's empty-scan floor covering the
# case where the sentence itself is rewritten or moved and the regex
# stops matching anything. POSIX sh; same declared downgrade as every
# other gate in this file (Windows has no `sh` by default).
if(UNIX)
    add_test(
        NAME readme_test_count_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_test_count.sh"
            "${PROJECT_SOURCE_DIR}/README.md"
            "${PROJECT_BINARY_DIR}"
            "$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
    )
    set_tests_properties(readme_test_count_test PROPERTIES LABELS consume)

    add_test(
        NAME readme_test_count_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_test_count.sh" --selftest
    )
    set_tests_properties(readme_test_count_selftest PROPERTIES LABELS selftest)
endif()
