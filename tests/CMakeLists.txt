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

# QA gate fix, 27/08/2026 (GODS_LAWS.md L-20/L-40): proves
# glintfx::test::ensure_check_failure_was_recorded() (harness/check.hpp),
# the invariant check harness_main.cpp's catch (const case_check_failed &)
# clause now calls instead of the empty body clang-tidy's
# bugprone-empty-catch reproved - see the test file's own header comment.
glintfx_add_test(harness_check_invariant_test)

# CORE-COLOR (TODO.md, GODS_LAWS.md L-19/L-26): frozen layout of
# gltfx_rgba plus the round-trip/premultiply conversions - see
# color_test.cpp's own header comment.
glintfx_add_test(color_test)

# CORE-TIME (TODO.md W3, GODS_LAWS.md L-19/L-20/L-26): the core
# layer's public representation of elapsed time - gltfx_duration/
# gltfx_time_point and the three gltfx_duration_* conversions - see
# time_test.cpp's own header comment.
glintfx_add_test(time_test)

# ASSET-LOAD (TODO.md W3, GODS_LAWS.md L-19/L-20/L-22/L-40, ESCOPO.md
# SS2 decisions 4 and 8): path-to-bytes loader - see
# include/glintfx/platform/asset/file.hpp's own header comment (moved
# there from include/glintfx/asset/ by the lider's own ruling,
# 28/08/2026 adversarial review - see that header's "UNDER
# include/glintfx/platform/" paragraph) and asset_load_test.cpp's own
# header comment for the closed six-scenario matrix. No if(UNIX) guard:
# std::filesystem/std::ifstream are standard-library, not OS-specific,
# so this registers on every platform in the matrix, unconditionally,
# exactly like version_test above.
glintfx_add_test(asset_load_test)

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
    "${PROJECT_SOURCE_DIR}/src/gfss/numeric_lexeme.cpp"
)

# GFSS-SEL-PARSE-CORE (TODO.md, GODS_LAWS.md L-19/L-20/L-28/L-40):
# selector grammar core (tag/.class/#id/*, compound, the four
# combinators, comma-separated list, every argument-less pseudo-class
# and the five functional ones recognized by name with a raw argument)
# built on top of GFSS-TOKEN's own token stream - see gfss_selector_
# parse_test.cpp's own header comment. Needs src/gfss/selector_parse.hpp
# and its own private siblings (selector_ast.hpp, selector_pseudo_
# vocabulary.hpp), none promoted to include/glintfx/ (GODS_LAWS.md L-19:
# "o header nasce interno") - the SAME "${PROJECT_SOURCE_DIR}/src"
# PRIVATE include dir gfss_tokenizer_test/gfss_color_parse_test above
# already add for their own private headers. Diagnostics this parser
# produces live in the SAME shared src/gfss/diagnostic_vocabulary.hpp
# GFSS-TOKEN's own tokenizer already uses (project leader's decision,
# 27/08/2026: one list for the whole gfss track, not a separate one per
# producer layer - see diagnostic_vocabulary.hpp's own header comment),
# so no third vocabulary header is registered here.
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

# GFSS-VALUE (TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-22/L-27/L-28/L-40):
# the closed value-nature/length-unit representation (public,
# include/glintfx/gfss/value.hpp - [PMU], ratified by the project
# leader 27/08/2026) and its own single-token decoder
# (glintfx::style::detail::parse_value(), src/gfss/value_parse.hpp, NOT
# yet promoted to include/glintfx/ - GODS_LAWS.md L-19) - see gfss_
# value_test.cpp's own header comment. Needs the SAME
# "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir every other internal
# gfss header above already adds.
glintfx_add_test(gfss_value_test)
target_include_directories(gfss_value_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# value_parse.cpp defines glintfx::style::detail::parse_value() with NO
# GLINTFX_API - deliberately (GODS_LAWS.md L-19), the SAME reason
# gfss_color_parse_test/gfss_selector_parse_test above each compile
# their own internal .cpp a second time instead of linking the hidden
# copy inside glintfx_library's own .so. value_parse.cpp itself calls
# numeric_lexeme.hpp's own decode_number_lexeme()/decode_percentage_
# lexeme() (also not GLINTFX_API - numeric_lexeme.cpp's own header
# comment) and lexical_rules.hpp's own consume_number() (GFSS-TOKEN's
# own private grammar helper, reused here to re-derive a dimension
# token's own number/unit boundary - value_parse.cpp's own split_
# dimension_lexeme() comment) - both need their SECOND compile here
# too, or this executable fails to LINK, not merely to behave
# correctly. named_colors.hpp's own ascii_case_insensitive_equal() is
# fully INLINE (no .cpp counterpart), the SAME reason gfss_selector_
# parse_test above needs no extra source for it.
target_sources(gfss_value_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/value_parse.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/numeric_lexeme.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/lexical_rules.cpp"
)

# GFSS-NODE-VIEW, fatia A (TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-26/
# L-28/L-40, ESCOPO.md SS2 decision 6): the five gltfx_node_state
# pseudo-class bits, their frozen numeric values and
# gltfx_node_state_name()/gltfx_node_state_has() - see gfui_node_
# state_test.cpp's own header comment.
glintfx_add_test(gfui_node_state_test)

# GFSS-NODE-VIEW, fatia B+C (TODO.md, GODS_LAWS.md L-17/L-19/L-20/
# L-22/L-26/L-28/L-40, ESCOPO.md SS2 decision 6): the ten-entry table
# (gltfx_node_facts), the three-pointer view (gltfx_node_view), the
# attribute answer (gltfx_node_attribute) and gltfx_node_facts_first_
# missing() (fatia B), plus navigation over the two alien fixture
# trees through src/gfui/node_query.hpp's own internal forwarders
# (fatia C) - see gfui_node_view_test.cpp's own header comment. Needs
# the SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir every other
# internal-header consumer above already adds, for node_query.hpp.
glintfx_add_test(gfui_node_view_test)
target_include_directories(gfui_node_view_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# GFSS-MATCH-SIMPLE (TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-22/L-28/
# L-40, /var/tmp/glintfx-plan/gfss-match-simple-plano.md): whether a
# compound selector matches ONE node - see gfui_compound_match_test.
# cpp's own header comment for the four fatias (B state-pseudo-class
# table, C species/case/deferral over parsed text, D class-enumeration
# bitmask/repetition/fallback, E call-order proved by counting). Needs
# the SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir every other
# internal-header consumer above already adds, for gfss/gfui's own
# private headers.
glintfx_add_test(gfui_compound_match_test)
target_include_directories(gfui_compound_match_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# compound_match.cpp and selector_parse.cpp define glintfx::gfui::
# detail::match_compound()/glintfx::style::detail::parse_selector_
# list() with NO GLINTFX_API - deliberately (GODS_LAWS.md L-19), the
# SAME reason gfss_selector_parse_test above compiles selector_parse.
# cpp a second time instead of linking the hidden copy inside glintfx_
# library's own .so. This test needs the real parser AND the real
# matcher, so both get the same second compile.
target_sources(gfui_compound_match_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/selector_parse.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/compound_match.cpp"
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

# X-0 (TODO.md, GODS_LAWS.md L-09/L-40): win32_runner_probe_test.cpp's
# whole body lives inside `#if defined(_WIN32)` (see that file's own
# header comment for why) - registering it unconditionally would ask
# every non-Windows leg of the five-platform matrix to link an empty
# translation unit with zero registered cases. Guarded here instead,
# with the SAME "declared absence, not silence" shape
# src/platform/CMakeLists.txt's own WIN32/UNIX branch already uses
# (GODS_LAWS.md L-40: a gate that is skipped must say so, never just
# not run). user32 carries CreateWindowExW/RegisterClassExW/
# DestroyWindow/PeekMessageW - Win32 counts as system API for the
# dependency-zero rule (GODS_LAWS.md L-07), same category as
# libwayland-client on Linux.
if(WIN32)
    glintfx_add_test(win32_runner_probe_test)
    target_link_libraries(win32_runner_probe_test PRIVATE user32)
else()
    message(STATUS "glintfx: win32_runner_probe_test skipped (Windows-only probe, see its own header comment)")
endif()

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

# CODEGEN-NUMCONV: this tool used to have a SECOND, unchecked
# text-to-number conversion in main.cpp's own parse_max_version_argument()
# - std::stod() on a malformed max-version argument threw
# std::invalid_argument straight past main(), std::terminate()d, and
# SIGABRT'd with a message that never even named the bad argument text.
# Deliberately NOT WILL_FAIL-only (like the two gates above): the OLD,
# broken behavior ALSO exits non-zero (a crash's return code is
# non-zero too), so a plain "did the process fail" check would stay
# green across the regression this test exists to catch.
# PASS_REGULAR_EXPRESSION checks the ACTUAL diagnostic text instead
# (CTest ignores the return code once this property is set) - the
# CLEAN parse_decimal_number()-plus-fail() message this tool now
# prints, naming both the culprit ARGUMENT ("argumento max-version")
# and the culprit VALUE (the exact invalid text), never the generic
# libstdc++ terminate() banner the crash used to print.
add_test(
    NAME gl_registry_codegen_bad_max_version_argument_reproves_test
    COMMAND gl_registry_codegen
        "${CMAKE_CURRENT_SOURCE_DIR}/gl_registry_codegen_fixtures/empty_registry.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/gl_registry_codegen_cli_out/bad_max_version"
        "nao-e-numero"
)
set_tests_properties(gl_registry_codegen_bad_max_version_argument_reproves_test PROPERTIES
    LABELS unit
    PASS_REGULAR_EXPRESSION "argumento max-version nao e um numero decimal valido: 'nao-e-numero'")

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

# ARCH-PORTS (TODO.md, GODS_LAWS.md L-19/L-20/L-22/L-40): the display
# connection port, proved OS-agnostically below, plus the one Wayland-
# only adapter case that stays if(UNIX)-guarded further down
# (display_connect_failure_test).
#
# "${PROJECT_SOURCE_DIR}/src" PRIVATE on each: the SAME pattern
# gfss_tokenizer_test/gfss_color_parse_test above already use to reach
# src/gfss/'s own private headers - here it makes "platform/port/*.hpp"
# and "platform/wayland/*.hpp" resolvable as quoted includes.
# "fake/fake_display_adapter.hpp" needs no extra include dir: every
# test target already inherits glintfx_test_harness's own PUBLIC
# "${CMAKE_CURRENT_SOURCE_DIR}" (= tests/) include dir through
# glintfx_add_test()'s PRIVATE link to it.
#
# The three cases below used to sit inside the if(UNIX) guard that
# still wraps display_connect_failure_test further down, on the theory
# that everything in this ARCH-PORTS block was Wayland-only. That
# theory was wrong for these three (GODS_LAWS.md L-04, "comportamento
# igual em todo sistema", achado de 03/09/2026): global_catalog.{hpp,
# cpp} include only <cstdint>/<string>/<string_view>/<vector>/
# <algorithm>/<new> (every "wl_" mention in the header is a comment,
# not a type), and display_port_concept_test.cpp/display_connection_
# fake_test.cpp exercise display_connection_port/display_connection<A>
# purely through fake_display_adapter and a deliberately deficient
# local type - no Wayland type reachable from either TU. Guarding them
# meant these three cases silently never ran on Windows; they are
# registered unconditionally now, same as asset_load_test/
# hygiene_coverage_test/spdx_test above.
#
# WL-DISPLAY fatia A (TODO.md, GODS_LAWS.md L-19/L-20): the pure
# global catalog.
glintfx_add_test(global_catalog_test)
target_include_directories(global_catalog_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(global_catalog_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
)

# display_connection_port itself: the fake_display_adapter positive
# control and the missing_close negative control. This TU used to also
# static_assert that wayland_display_adapter satisfies the concept -
# that assertion is gone from here now, not moved: src/platform/
# wayland/selected_display_adapter_check.cpp already proves the exact
# same fact (selected_display_adapter is a plain `using` alias for
# wayland_display_adapter, not a distinct type) unconditionally, as
# part of glintfx_library itself, on every configure that enters
# src/platform/wayland/ at all - repeating it here as a SEPARATE,
# if(UNIX)-guarded ctest case added no coverage this file's own build
# did not already have, and dropping it is what makes this TU, and
# this ctest case, genuinely OS-agnostic instead of only OS-agnostic
# in its first two thirds.
glintfx_add_test(display_port_concept_test)
target_include_directories(display_port_concept_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

glintfx_add_test(display_connection_fake_test)
target_include_directories(display_connection_fake_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Wayland-only from here down: the real adapter case.
if(UNIX)
    # wayland_display_adapter has no GLINTFX_API (deliberately - GODS_
    # LAWS.md L-19/ARCH-PORTS, "nada e exportado"), so glintfx.so's own
    # -fvisibility=hidden (cmake/GlintfxLibrary.cmake) never puts its
    # methods in the dynamic symbol table. This test compiles display_
    # adapter.cpp a SECOND time, directly into its own object set - the
    # SAME technique gfss_color_parse_test/wl_proto_link_test above
    # already use for a symbol with no GLINTFX_API - rather than trying
    # (and failing) to link the .so's own hidden copy.
    glintfx_add_test(display_connect_failure_test)
    target_include_directories(display_connect_failure_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        ${GlintfxWaylandClient_INCLUDE_DIRS}
    )
    target_sources(display_connect_failure_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/display_adapter.cpp"
        # WL-DISPLAY fatia B: display_adapter.cpp now calls into
        # global_catalog's own member functions from its registry
        # listener - this second compile of display_adapter.cpp needs
        # global_catalog.cpp linked alongside it for the same reason
        # global_catalog_test above compiles it a second time itself
        # (glintfx.so's hidden visibility, see this block's own header
        # comment).
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
    )
    # wayland-client itself, resolved by the SAME plain pkg-config
    # variables glintfx_add_wayland_xdg_shell_binding() (cmake/
    # GlintfxWaylandProtocols.cmake) uses for glintfx_library - not that
    # function itself, which also pulls in the xdg-shell PROTOCOL
    # binding this test does not need. GlintfxWaylandClient_* were set
    # once, at include(GlintfxWaylandProtocols) time in the ROOT
    # CMakeLists.txt (before add_subdirectory(tests)), and are ordinary
    # variables, inherited into this directory's scope like any other.
    target_link_directories(display_connect_failure_test PRIVATE ${GlintfxWaylandClient_LIBRARY_DIRS})
    target_link_libraries(display_connect_failure_test PRIVATE ${GlintfxWaylandClient_LIBRARIES})
    target_compile_options(display_connect_failure_test PRIVATE ${GlintfxWaylandClient_CFLAGS_OTHER})
endif()

# WIN-DISPLAY fatia 3 (TODO.md, GODS_LAWS.md L-04/L-09): the Windows
# counterpart of display_connect_failure_test above - the real success
# path this time, proved live on the windows-latest GitHub Actions
# runner (the same runner tests/win32_runner_probe_test.cpp already
# proves hands out a real, interactive window station). Guarded the
# same "declared absence, not silence" shape win32_runner_probe_test's
# own if(WIN32) block already uses.
if(WIN32)
    # win32_display_adapter has no GLINTFX_API (deliberately - GODS_
    # LAWS.md L-19/WIN-DISPLAY, "nada e exportado"), so glintfx.dll's
    # own hidden visibility never puts its methods in the exported
    # symbol table. This test compiles display_adapter.cpp a SECOND
    # time, directly into its own object set - the exact same technique
    # display_connect_failure_test above uses for wayland_display_
    # adapter - rather than trying (and failing) to link the DLL's own
    # hidden copy.
    glintfx_add_test(win32_display_connect_test)
    target_include_directories(win32_display_connect_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_display_connect_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
    )
    target_link_libraries(win32_display_connect_test PRIVATE user32)
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
# enforced it (see tests/tools/check_hygiene_coverage.py's own header
# for the declared generated-header limitation).
#
# Ported from POSIX sh to python3 and registered WITHOUT an if(UNIX)
# guard (GODS_LAWS.md L-04, decisao do lider de 02/09/2026: "O
# comportamento deve ser igual em qualquer OS" - the sh version's
# if(UNIX) guard meant nobody checked header hygiene coverage on
# Windows at all). Same GLINTFX_PYTHON3_EXECUTABLE discovery
# check_dep_zero_trace.py's own registration already uses further
# below in this file - find_program() is idempotent across repeated
# calls with the same cache variable, so this second call costs
# nothing and lets this section stand on its own.
find_program(GLINTFX_PYTHON3_EXECUTABLE NAMES python3 python REQUIRED)

add_test(
    NAME hygiene_coverage_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_hygiene_coverage.py"
        "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/header_hygiene_test.cpp"
)
set_tests_properties(hygiene_coverage_test PROPERTIES LABELS consume)

add_test(
    NAME hygiene_coverage_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_hygiene_coverage.py" --selftest
)
set_tests_properties(hygiene_coverage_selftest PROPERTIES LABELS selftest)

# SPDX header coverage gate (GODS_LAWS.md L-08: publico no GitHub,
# AGPL-3.0). Ported from POSIX sh (tests/tools/check_spdx.sh, invoked
# directly by the ubuntu-only "leis" job in .github/workflows/ci.yml,
# never registered here nor exercised on any other platform) to
# python3 and registered here, WITHOUT an if(UNIX) guard, for the same
# L-04 reason as hygiene_coverage_test immediately above - this makes
# it run inside the SAME ctest invocation the linux and windows CI
# jobs already execute on all five platforms. See tests/tools/
# check_spdx.py's own header for GATE-SPDX-UNTRACKED, the coverage fix
# this port also carries (untracked files are now scanned too, not
# only `git ls-files`). The direct `sh tests/tools/check_spdx.sh`
# steps in the "leis" job are removed in the same fatia (redundant
# once this ctest case covers all five platforms, and the script
# itself is deleted).
add_test(
    NAME spdx_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_spdx.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(spdx_test PROPERTIES LABELS consume)

add_test(
    NAME spdx_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_spdx.py" --selftest
)
set_tests_properties(spdx_selftest PROPERTIES LABELS selftest)

# Vendor purity gate (GODS_LAWS.md L-07 EXCECAO No 1: third_party/
# khronos/ e a UNICA excecao de glintfx a lei de dependencia zero).
# PORTED from POSIX sh (tests/tools/check_vendor_purity.sh, invoked
# directly by the ubuntu-only "leis" job in .github/workflows/ci.yml,
# never registered here nor exercised on any other platform) to
# python3 and registered here WITHOUT a system guard, same L-04 reason
# as spdx_test immediately above - this makes it run inside the SAME
# ctest invocation the linux and windows CI jobs already execute on
# all five platforms. See tests/tools/check_vendor_purity.py's own
# header for GATE-TREE-PARITY-NEWLINE, the enumeration-safety fix this
# port also carries (os.walk() instead of a newline-joined `find`
# listing), and for why khronos_vendor_files.sh (the shell library
# this gate used to source) is deleted in the same fatia rather than
# kept for zero remaining consumers. The direct
# `sh tests/tools/check_vendor_purity.sh` steps in the "leis" job are
# removed in the same fatia (redundant once this ctest case covers all
# five platforms, and the script itself is deleted).
add_test(
    NAME vendor_purity_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_vendor_purity.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(vendor_purity_test PROPERTIES LABELS consume)

add_test(
    NAME vendor_purity_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_vendor_purity.py" --selftest
)
set_tests_properties(vendor_purity_selftest PROPERTIES LABELS selftest)

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
# violation"). PORTED from POSIX sh (tests/tools/check_layers.sh,
# if(UNIX)-guarded, never exercised on Windows) to python3 and
# registered here WITHOUT a system guard (GATE-TREE-PARITY,
# GODS_LAWS.md L-04, decisao do lider: "O comportamento deve ser igual
# em qualquer OS") - same shape as hygiene_coverage_test/spdx_test
# above. The old tests/tools/check_layers.sh is deleted in the same
# fatia.
add_test(
    NAME layers_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(layers_test PROPERTIES LABELS consume)

# GODS_LAWS.md L-40 achado (revisao adversarial, 25/08/2026):
# layers_test was the ONLY test wired to this gate, and always ran
# against the real repo root (never empty) - the empty-scan floor
# inside check_layers.py was never exercised by the suite otherwise.
# Wiring --selftest here closes that gap, same consume/selftest label
# taxonomy as the other gates in this file.
add_test(
    NAME layers_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.py" --selftest
)
set_tests_properties(layers_selftest PROPERTIES LABELS selftest)

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
    # DEPZERO-SELFTEST-FIX (28/08/2026, GODS_LAWS.md L-40): belt on top of
    # the exit code. check_no_x11.sh's own --selftest aggregator used to
    # be able to report exit 0 while its own stdout/stderr still printed
    # "FALHOU" for an individual control (a POSIX sh global-variable-name
    # collision fixed in the script itself, same fatia) - that is exactly
    # the shape of lie FAIL_REGULAR_EXPRESSION exists to catch: it fails
    # the ctest verdict on a text match REGARDLESS of the exit code,
    # closing the gap even if a future edit reintroduces the same
    # collision. Deliberately NOT PASS_REGULAR_EXPRESSION: per `cmake
    # --help-property PASS_REGULAR_EXPRESSION`, that property makes ctest
    # IGNORE the exit code entirely - it would trade this hole for a new
    # one instead of closing it.
    set_tests_properties(x11_selftest PROPERTIES
        LABELS selftest
        FAIL_REGULAR_EXPRESSION "FALHOU")
endif()

# Port privacy gate (ARCH-PORTS, TDD case R5, GODS_LAWS.md L-19/L-40):
# proves the ARCH-PORTS gatilho de parada holds - nothing installed,
# nothing exported, adapter list closed - see check_port_privacy.sh's
# own header for the four sub-checks. The library-path argument is
# "NONE" in static mode (sub-check (d), the dynamic-symbol scan, has no
# artifact to inspect there and says so instead of skipping silently -
# GODS_LAWS.md L-40). POSIX sh; same declared downgrade as layers_test
# above.
if(UNIX)
    if(BUILD_SHARED_LIBS)
        set(GLINTFX_PORT_PRIVACY_LIB_ARG "$<TARGET_FILE:glintfx_library>")
    else()
        set(GLINTFX_PORT_PRIVACY_LIB_ARG "NONE")
    endif()
    add_test(
        NAME port_privacy_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_port_privacy.sh"
            "${CMAKE_SOURCE_DIR}"
            "${GLINTFX_PORT_PRIVACY_LIB_ARG}"
    )
    set_tests_properties(port_privacy_test PROPERTIES LABELS consume)

    add_test(
        NAME port_privacy_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_port_privacy.sh" --selftest
    )
    set_tests_properties(port_privacy_selftest PROPERTIES LABELS selftest)
endif()

# Dependency-zero gate (DEPZERO-GATE, TODO.md, GODS_LAWS.md L-07/L-40):
# proves NOTHING in the tracked tree introduces a third-party
# dependency - three sub-checks (CMake surface, include surface, the
# built artifact's own DT_NEEDED) closed by FORM and POLARITY, never by
# a growing forbidden-name list - see check_dep_zero.py's own header
# for the full rationale, including two corrections found by measuring
# the real tree before writing this gate (own public headers consumed
# via <glintfx/...>, and the two CMake-generated headers export.hpp/
# version_macros.hpp) that the planning document's enumeration missed.
# Decision of the leader, 28/08/2026 (AskUserQuestion): this gate bites
# in BOTH commit (tools/git-hooks/pre-commit, installed by
# tools/git-hooks/install.sh) and CI - this is the CI half.
#
# DEPZERO-GATE-PY (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"): sub-checks (a)/(b)
# used to be POSIX sh, `if(UNIX)`-guarded, and NEVER ran on the
# `windows` CI job or in the pre-commit hook on Windows - the exact gap
# this port closes. Ported to check_dep_zero.py, registered here
# UNGUARDED, the same shape check_spdx.py/check_vendor_purity.py/
# dep_zero_trace already proved works on all five platforms.
# find_program(...NAMES python3 python REQUIRED): same resolution
# dep_zero_trace's own registration below already uses (cached, so
# this earlier call does not re-search) - see that registration's own
# comment for why NAMES python3 python and not find_package(Python3).
#
# Sub-check (c) (readelf -d, the built artifact's own DT_NEEDED) has NO
# Windows-shaped equivalent to run here: DEPZERO-PARITY-WIN
# (03/09/2026) already gave Windows its OWN, separate parity script
# (tools/ci/check-dep-zero-win.ps1, dumpbin /imports). The library-path
# argument is therefore three-valued, not two: the real .so on a shared
# Linux/BSD build, "NONE" in static mode (unchanged, GODS_LAWS.md
# L-40's declared-skip shape), and "WINDOWS-SEPARATE" on WIN32 (a
# THIRD declared skip, honest and distinct from the static one -
# check_dep_zero.py's own check_needed_allowlist() and this port's
# selftest_needed_windows_separate_skip control, see that file's own
# header, departure 2).
find_program(GLINTFX_PYTHON3_EXECUTABLE NAMES python3 python REQUIRED)

if(WIN32)
    set(GLINTFX_DEP_ZERO_LIB_ARG "WINDOWS-SEPARATE")
elseif(BUILD_SHARED_LIBS)
    set(GLINTFX_DEP_ZERO_LIB_ARG "$<TARGET_FILE:glintfx_library>")
else()
    set(GLINTFX_DEP_ZERO_LIB_ARG "NONE")
endif()
add_test(
    NAME dep_zero_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero.py"
        "${CMAKE_SOURCE_DIR}"
        "${GLINTFX_DEP_ZERO_LIB_ARG}"
)
set_tests_properties(dep_zero_test PROPERTIES LABELS consume)

add_test(
    NAME dep_zero_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero.py" --selftest
)
# DEPZERO-SELFTEST-FIX (28/08/2026, GODS_LAWS.md L-40): belt on top of
# the exit code, same shape dep_zero_trace_selftest/spdx_selftest/
# vendor_purity_selftest below already wear - costs nothing and keeps
# the taxonomy uniform across every selftest in this file.
set_tests_properties(dep_zero_selftest PROPERTIES
    LABELS selftest
    FAIL_REGULAR_EXPRESSION "FAILED")

# Dependency-zero TRACE gate (DEPZERO-TRACE, TODO.md, GODS_LAWS.md
# L-07/L-12/L-40; decision of the leader, 28/08/2026, AskUserQuestion:
# "Trocar o mecanismo: perguntar ao CMake"): the second oracle for the
# SAME law dep_zero_test above enforces, asking the CMake INTERPRETER
# what it actually executed (--trace-expand --trace-format=json-v1,
# plus the codemodel-v2 file API reply as a witness) instead of
# interpreting the TEXT of the CMakeLists.txt/*.cmake surface -
# tests/tools/check_dep_zero_trace.py's own header comment carries the
# full rationale (the nine escapes two adversarial reviews found
# against the text-based sub-check (a), why python3 is the authorized
# tool for this specific job under GODS_LAWS.md L-07's own
# "ferramenta de build" clarification of 28/08/2026, and the six rules
# R1..R6 this file judges the trace/codemodel against). check_dep_
# zero.sh's OWN sub-checks (b) (include surface) and (c) (the built
# artifact's DT_NEEDED, immediately above) are UNTOUCHED here - only
# its sub-check (a), the one nine escapes were found against, is
# superseded, and the OLD text machinery for (a) is retired in a
# SEPARATE, later fatia (DEPZERO-SHALLOW), never in the same commit
# that adds a not-yet-proven oracle (GODS_LAWS.md house rule of
# 28/08/2026: "portao que nunca reprovou nao e portao").
#
# Registered on ALL FIVE platforms, WITHOUT an if(UNIX) guard, same as
# dep_zero_test/dep_zero_selftest above since DEPZERO-GATE-PY: the
# mechanism is CMake's own trace and file API, identical on Windows
# (the same reply real IDEs consume there daily - cmake-file-api(7))
# and on the four POSIX targets - there is no POSIX-sh dependency to
# downgrade away from here.
#
# find_program(..., NAMES python3 python REQUIRED): resolves the
# interpreter by searching PATH for either name (python3 on the four
# POSIX targets, python on windows-latest, whose default Python 3.12
# install does not necessarily expose a "python3" alias - measured via
# actions/runner-images' own Windows2025-Readme.md, 29/08/2026), the
# SAME built-in mechanism find_program() already provides for
# clang-format/clang-tidy/cppcheck a little further down in this file.
# Deliberately NOT find_package(Python3 COMPONENTS Interpreter): that
# call is itself a find_package() invocation, and check_dep_zero.py
# (the text-based gate above, registered via this exact cached
# GLINTFX_PYTHON3_EXECUTABLE) pattern-matches every find_package()
# call against ITS OWN allowlist. Caught live (29/08/2026, tools/
# preci.sh --fast, stage_ctest, back when this gate was still the
# POSIX sh check_dep_zero.sh): dep_zero_test reproved THIS FILE's own
# find_package(Python3 ...REQUIRED) line the moment it first ran,
# citing tests/CMakeLists.txt by name - correct behavior on that
# gate's part, but fixing it would mean editing the allowlist, which
# is explicitly out of scope here (DEPZERO-SHALLOW, not DEPZERO-TRACE).
# find_program() is a DIFFERENT CMake command that check_dep_zero.py
# never inspects (it matches only "find_package("), so this line
# satisfies both oracles without widening either fatia's declared
# scope. Cached: this call reuses the SAME result the dep_zero_test
# registration above already resolved, no re-search.
find_program(GLINTFX_PYTHON3_EXECUTABLE NAMES python3 python REQUIRED)

# RUN_SERIAL TRUE: the script RE-CONFIGURES this exact build directory
# (to obtain the trace and file API reply) - running it concurrently
# with another ctest case would race the generator over the same
# build files, the identical reasoning embed_dll_colocation_test
# documents for its own nested configure, applied here to a configure
# of THIS project's own build dir instead of a fixture's.
add_test(
    NAME dep_zero_trace
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero_trace.py"
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}"
)
set_tests_properties(dep_zero_trace PROPERTIES LABELS consume RUN_SERIAL TRUE)

# --selftest here runs entirely against DISPOSABLE fixtures under
# mktemp (tests/tools/check_dep_zero_trace.py's own selftest_main(),
# each fixture is a tiny throwaway CMake project configured fresh) -
# it does not touch this build directory at all, so it needs neither
# RUN_SERIAL nor the source/build-dir arguments dep_zero_trace above
# takes. FAIL_REGULAR_EXPRESSION, the SAME belt dep_zero_selftest above
# already wears (GODS_LAWS.md L-40): this script's own aggregator
# (selftest_main(), a plain Python list comprehension over each
# control's boolean return - no POSIX-sh shared-global-variable
# collision is even POSSIBLE here, unlike the bug DEPZERO-SELFTEST-FIX
# fixed in the sibling sh gates) does not share that specific failure
# mode, but the belt costs nothing and keeps the taxonomy uniform
# across every selftest in this file, sh or python3 alike.
add_test(
    NAME dep_zero_trace_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero_trace.py"
        --selftest
)
set_tests_properties(dep_zero_trace_selftest PROPERTIES
    LABELS selftest
    FAIL_REGULAR_EXPRESSION "FAILED")

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

# [[nodiscard]] gate (CE-4 of CORE-ERROR, TODO.md; RSLT-PARITY-WIN,
# GODS_LAWS.md L-04, ordem do lider 03/09/2026): proves
# glintfx::gltfx_rslt<T>'s [[nodiscard]] is a real compiler diagnostic
# by compiling a fixture that drops the result (must FAIL) and one that
# uses it (must succeed) - see check_nodiscard_rslt.py's own header.
# Two include dirs, not one (same pair glintfx_set_target_include_dirs()
# gives every real target at BUILD_INTERFACE, cmake/GlintfxLibrary.cmake):
# err.hpp pulls in the GENERATED glintfx/export.hpp for GLINTFX_API, which
# only exists under GLINTFX_GENERATED_INCLUDE_DIR, not under the
# committed include/ tree. Compile-only (-c / /c in the script, no
# link), so no glintfx build artifact is needed here at all - the same
# reason PROJECT_SOURCE_DIR/include is passed directly rather than an
# installed prefix. Python (GLINTFX_PYTHON3_EXECUTABLE, already
# resolved above for dep_zero_trace), registered on ALL FIVE
# PLATFORMS, WITHOUT an if(UNIX) guard, unlike the POSIX-sh predecessor
# this replaces: check_nodiscard_rslt.py's own header carries the full
# reasoning (same "Windows has no sh by default" problem dep_zero_
# trace's own header already solved for a different gate, by trading
# the same mechanism away). CMAKE_CXX_COMPILER_ID selects MSVC's own
# diagnostic (C4834) vs. GCC/Clang's (nodiscard/discard/unused) inside
# the script - never guessed from the compiler path.
#
# STD-FLAG-WIN (estreia real no Windows, run 33832169390, GODS_LAWS.md
# L-44/L-49): the script used to hardcode the literal "/std:c++23" for
# MSVC. That literal is NOT a flag any MSVC version has ever accepted -
# CMAKE_CXX23_STANDARD_COMPILE_OPTION (Compiler/MSVC-CXX.cmake, this
# machine's CMake 4.1.6) resolves C++23 to "-std:c++latest" instead, on
# every MSVC new enough to support C++23 at all. The estreia's own log
# proved this live: `cl : Command line warning D9002 : ignoring unknown
# option '/std:c++23'`, cl.exe fell back to its pre-C++17 default, and
# <string_view>/<variant> then failed with C2039 "is not a member of
# 'std'" - the C2238 the coordinator first saw was a CASCADE off that,
# not a GLINTFX_API/export-header problem (that hypothesis is REFUTED:
# the real "Build" step of the SAME job, using the SAME cl.exe, already
# compiles this exact header successfully by going through CMake's own
# CXX_STANDARD 23 property, cmake/GlintfxCompileOptions.cmake, which
# resolves to whatever CMAKE_CXX23_STANDARD_COMPILE_OPTION says for
# THIS specific detected compiler/version - never a literal string this
# file or the python script gets to invent on its own). Passing that
# CMake-resolved variable through as a new argument, instead of letting
# either script hardcode a per-compiler literal a second time, is the
# fix: whatever flag spelling let the real target build is the exact
# same flag spelling the standalone fixture compile now uses, on every
# platform, not just MSVC - GCC/Clang already accepted the old literal
# only because "-std=c++23" happens to be their real flag too, not
# because hardcoding was ever the right design.
add_test(
    NAME nodiscard_rslt_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_nodiscard_rslt.py"
        "${PROJECT_SOURCE_DIR}/include"
        "${GLINTFX_GENERATED_INCLUDE_DIR}"
        "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CXX_COMPILER_ID}"
        "${CMAKE_CXX23_STANDARD_COMPILE_OPTION}"
)
set_tests_properties(nodiscard_rslt_test PROPERTIES LABELS consume)

# Debug-only precondition guard (correction of 25/08/2026, GODS_LAWS.md
# CORE-ERROR; RSLT-PARITY-WIN, GODS_LAWS.md L-04, ordem do lider
# 03/09/2026): proves BOTH halves of gltfx_rslt<T>::value()/error()'s
# assert()-based precondition check - stops deterministically with a
# message in a Debug-flags compile (NDEBUG undefined), costs nothing
# and changes nothing in a Release-flags compile (NDEBUG defined) - see
# check_rslt_precondition.py's own header for the two independent UB
# shapes (primary template vs. the T=void specialization) this proves
# separately, live, rather than assumed identical, AND for the
# Windows-specific interactive-dialog risk it declares rather than
# hides. Needs the ALREADY-BUILT library's directory (unlike
# nodiscard_rslt_test above, this fixture links and RUNS, it does not
# stop at -c/-Fo) - $<TARGET_FILE_DIR:glintfx_library> (the RUNTIME
# artifact: the .so a POSIX consumer both links and loads at runtime,
# or the .dll a Windows consumer loads at runtime) and
# $<TARGET_LINKER_FILE_DIR:glintfx_library> (the LINK-time artifact:
# the SAME .so on POSIX, but glintfx.lib - the import library, a
# DIFFERENT file - on Windows) are passed SEPARATELY so the script
# never has to assume they share a directory on every platform; a
# static-mode -Wl,-rpath entry with nothing to resolve there is
# harmless, not an error. Python, registered on ALL FIVE PLATFORMS,
# WITHOUT an if(UNIX) guard - same replacement, same reasoning as
# nodiscard_rslt_test above.
# STD-FLAG-WIN (same estreia, same fix as nodiscard_rslt_test's own
# comment above - read it first): this fixture also hardcoded the
# non-existent MSVC literal "/std:c++23" and failed the SAME way,
# never reaching its own compile-and-run subject at all (the estreia's
# log: "debug fixture failed to compile", the identical D9002/C2039
# cascade). CMAKE_CXX23_STANDARD_COMPILE_OPTION replaces it here too.
#
# CRT-LINK-WIN (Windows static-mode estreia, coordinator report
# 04/09/2026, GODS_LAWS.md L-04): once STD-FLAG-WIN above let this
# fixture actually reach LINK, the static BUILD_SHARED_LIBS mode failed
# there instead (LNK2038, RuntimeLibrary mismatch) - glintfx.lib links
# straight into the fixture's own binary in that mode, and MSVC's
# linker refuses two objects built against different C runtimes.
# $<TARGET_PROPERTY:glintfx_library,MSVC_RUNTIME_LIBRARY> is the SAME
# property CMake itself used to pick /MD or /MT when it built
# glintfx_library (policy CMP0091, unset here so CMake's own default
# applies) - passing it through, instead of the script guessing a
# literal, is what check_rslt_precondition.py's own CRT-LINK-WIN header
# comment explains in full (including why NDEBUG on/off must NOT also
# flip this).
#
# CRT-LINK-WIN-GENEX (second estreia of the SAME fatia, coordinator
# report 04/09/2026, GODS_LAWS.md L-04): the bare $<TARGET_PROPERTY:...>
# above still failed, one step further than CRT-LINK-WIN's own fix -
# check_rslt_precondition.py rejected the value it received with
# "unrecognized MSVC_RUNTIME_LIBRARY value:
# 'MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'". That literal IS the
# policy default this same comment quotes two paragraphs up - the
# property's STORED value is itself generator-expression TEXT
# ($<$<CONFIG:Debug>:Debug>, meant to be resolved against whichever
# config actually builds), and $<TARGET_PROPERTY:...> only substitutes
# that stored text verbatim; CMake does not re-scan a generator
# expression's own OUTPUT for further $<...> to evaluate; that would be
# a second, independent evaluation pass this project never asked for.
# $<TARGET_GENEX_EVAL:tgt,expr> is CMake's own construct for exactly
# this - "evaluated as a generator expression in the context of tgt
# target... enables consumption of custom target properties that
# themselves contain generator expressions" (cmake-generator-
# expressions(7), $<TARGET_GENEX_EVAL:tgt,expr>; its own doc example is
# this identical shape, $<TARGET_GENEX_EVAL:foo,$<TARGET_PROPERTY:foo,
# CUSTOM_KEYS>>) - nesting TARGET_PROPERTY inside TARGET_GENEX_EVAL adds
# the one recursive evaluation pass CMake does not do on its own,
# resolving $<CONFIG:Debug> against the SAME config this build actually
# uses and leaving one of the four canonical strings
# check_rslt_precondition.py's own MSVC_RUNTIME_LIBRARY_TO_FLAG lookup
# already expected - the script's rejection of the earlier literal was
# the piso doing its job, not a bug in the script; it is untouched by
# this fix.
add_test(
    NAME rslt_precondition_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_rslt_precondition.py"
        "${PROJECT_SOURCE_DIR}/include"
        "${GLINTFX_GENERATED_INCLUDE_DIR}"
        "$<TARGET_FILE_DIR:glintfx_library>"
        "$<TARGET_LINKER_FILE_DIR:glintfx_library>"
        "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CXX_COMPILER_ID}"
        "${CMAKE_CXX23_STANDARD_COMPILE_OPTION}"
        "$<TARGET_GENEX_EVAL:glintfx_library,$<TARGET_PROPERTY:glintfx_library,MSVC_RUNTIME_LIBRARY>>"
)
set_tests_properties(rslt_precondition_test PROPERTIES LABELS consume)

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
# was already 34/33). check_readme_test_count.py compares the prose
# number against THIS build's own `ctest -N` total and reproves on any
# mismatch, with GODS_LAWS.md L-40's empty-scan floor covering the
# case where the sentence itself is rewritten or moved and the regex
# stops matching anything.
#
# PORTED from POSIX sh (tests/tools/check_readme_test_count.sh, retired
# in this same fatia) to python3 and registered WITHOUT an if(UNIX)
# guard (GODS_LAWS.md L-04, decisao do lider de 02/09/2026: "O
# comportamento deve ser igual em qualquer OS" - the same shape
# spdx_test/hygiene_coverage_test above already proved). README-COUNT-
# PARITY (TODO.md, medido em 02/09/2026): README.md now states TWO
# independent sentences ("On Linux," / "On Windows,"), because the two
# platforms genuinely register different totals (several if(UNIX)-only
# gates never entered the Windows run); check_readme_test_count.py
# picks the sentence that matches the platform THIS ctest run is
# actually executing on, so it enforces Windows's own claim on Windows,
# not the Linux one.
add_test(
    NAME readme_test_count_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_test_count.py"
        "${PROJECT_SOURCE_DIR}/README.md"
        "${PROJECT_BINARY_DIR}"
        "$<IF:$<BOOL:${BUILD_SHARED_LIBS}>,shared,static>"
)
set_tests_properties(readme_test_count_test PROPERTIES LABELS consume)

add_test(
    NAME readme_test_count_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_test_count.py" --selftest
)
set_tests_properties(readme_test_count_selftest PROPERTIES LABELS selftest)
