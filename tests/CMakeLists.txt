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
# gltfx_time_point and the four gltfx_duration_*/gltfx_now() functions
# (gltfx_now() added by LOOP-RUN fatia 6a, D-W6b-52) - see
# time_test.cpp's own header comment.
glintfx_add_test(time_test)

# CORE-FIXED-STEP (LOOP-RUN fatia 6a, docs/plano-w6b-fatias-6-8.md
# D-W6b-47, GODS_LAWS.md L-19/L-20/L-40): the optional passo-fixo atom
# a consumer reaches for on top of the raw, variable gltfx_frame_tick::
# elapsed - see fixed_step_test.cpp's own header comment. gltfx_fixed_
# step_accumulate() is public API (GLINTFX_API, exported from
# glintfx_library), so this needs no extra include dir/target_sources,
# exactly like time_test/math2d_test above.
glintfx_add_test(fixed_step_test)

# CORE-LOG CL-1 (TODO.md W5, GODS_LAWS.md L-19/L-20/L-26/L-40): the
# numeric, append-only severity scale plano/core-log.md Q2 fixes - see
# log_severity_test.cpp's own header comment.
glintfx_add_test(log_severity_test)

# CORE-MATH2D (TODO.md W4, GODS_LAWS.md L-19/L-20/L-26/L-40): the six
# frozen pieces of the core layer's 2D math - vector, matrix,
# rectangle, transform, angle and interpolation - across the project's
# two precision families (double in the world, single on the screen).
# See math2d_test.cpp's own header comment for the map from each of the
# leader's four decisions to the case that proves it. No if(UNIX)
# guard: everything here is pure core-layer value types and standard
# library, so it registers unconditionally on all five platforms,
# exactly like version_test.
glintfx_add_test(math2d_test)

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
    # GFSS-SEL-PARSE-NTH (05/09/2026): parse_anb() carries no GLINTFX_API by
    # design (GODS_LAWS.md L-19 - it is an internal microparser, not public
    # surface), so this target cannot reach it through the shared object and
    # compiles the source directly, the same way it already does for
    # selector_parse.cpp above.
    "${PROJECT_SOURCE_DIR}/src/gfss/anb_parse.cpp"
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

# GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_LAWS.md L-17/L-19/L-20/
# L-26/L-27/L-40, docs/gfss-property-registry-v1.md - 340 lines,
# twenty-two decisions, CTO, ratified in the project leader's own
# autonomous-mode order of 05/09/2026): the closed 104-property
# registry (public, include/glintfx/gfss/property.hpp, keyword.hpp -
# [PMU], one-way door) plus its own internal applied/reserved lifecycle
# accessor (src/gfss/property_status.hpp, NOT promoted to include/
# glintfx/ - GODS_LAWS.md L-19, the SAME "GFSS-API decides the public
# shape" reasoning value_parse.hpp/color_parse.hpp already carry for
# this slice) - see gfss_property_registry_test.cpp's own header
# comment. Needs the SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE include
# dir every other internal-header consumer above already adds, for
# gfss/property_status.hpp and gfss/property_table.hpp.
glintfx_add_test(gfss_property_registry_test)
target_include_directories(gfss_property_registry_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# property_status.cpp defines glintfx::style::detail::gfss_property_
# status()/gfss_property_status_counts() with NO GLINTFX_API
# (deliberately - property_status.hpp's own header comment), the SAME
# reason gfss_value_test above compiles value_parse.cpp a second time
# instead of linking the hidden copy inside glintfx_library's own .so.
# property.cpp itself is NOT recompiled here: it defines the registry's
# THREE PUBLIC GLINTFX_API functions (gltfx_gfss_property_name/is_
# inherited/initial), already reachable through glintfx::glintfx below
# - recompiling it a second time into this executable would define
# those same public symbols twice in one binary (property_status.cpp's
# own header comment carries the full reasoning; property_table.hpp,
# the header BOTH .cpp files and this test itself include, is `inline
# constexpr` throughout, so sharing it across translation units needs
# no extra compile at all).
target_sources(gfss_property_registry_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/property_status.cpp"
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
    # GFSS-MATCH-ATTR/GFSS-MATCH-STRUCT (TODO.md, wave W5): compound_
    # match.cpp's own match_compound() now calls into attribute_match.
    # cpp/structural_match.cpp directly (compound_match.hpp's own
    # updated header comment) - this second compile needs both or the
    # link fails, the exact same reason it already needed selector_
    # parse.cpp above.
    "${PROJECT_SOURCE_DIR}/src/gfui/attribute_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/structural_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/anb_parse.cpp"
)

# GFSS-MATCH-ATTR (TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-22/L-40):
# whether ONE attribute simple selector ("[foo]", "[foo=bar]", the six
# comparison operators) holds against ONE node, plus its wiring into
# compound_match.cpp's own match_compound() - see gfui_match_attr_test.
# cpp's own header comment for the two fatias (A the evaluator alone,
# B through real parsed selector text). Needs the SAME
# "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir every other internal-
# header consumer above already adds, for gfss/gfui's own private
# headers.
glintfx_add_test(gfui_match_attr_test)
target_include_directories(gfui_match_attr_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# attribute_match.cpp/compound_match.cpp/selector_parse.cpp define
# glintfx::gfui::detail::attribute_selector_holds()/match_compound()/
# glintfx::style::detail::parse_selector_list() with NO GLINTFX_API -
# deliberately (GODS_LAWS.md L-19), the SAME reason gfui_compound_
# match_test above compiles compound_match.cpp/selector_parse.cpp a
# second time instead of linking the hidden copy inside glintfx_
# library's own .so.
target_sources(gfui_match_attr_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/selector_parse.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/compound_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/attribute_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/structural_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/anb_parse.cpp"
)

# GFSS-MATCH-STRUCT (TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-40): the
# eleven structural pseudo-classes (first/last/only-child, first/last/
# only-of-type, :empty, and the four An+B functions nth-child/nth-
# last-child/nth-of-type/nth-last-of-type), plus their wiring into
# compound_match.cpp's own match_compound() - see gfui_match_struct_
# test.cpp's own header comment for the three fatias (A the seven
# simple evaluators, B the four An+B ones, C through real parsed
# selector text). Needs the SAME "${PROJECT_SOURCE_DIR}/src" PRIVATE
# include dir every other internal-header consumer above already adds.
glintfx_add_test(gfui_match_struct_test)
target_include_directories(gfui_match_struct_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Same second-compile reasoning as gfui_match_attr_test immediately
# above - structural_match.cpp/attribute_match.cpp/compound_match.cpp/
# selector_parse.cpp/anb_parse.cpp carry no GLINTFX_API either.
target_sources(gfui_match_struct_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gfss/selector_parse.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/compound_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/attribute_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfui/structural_match.cpp"
    "${PROJECT_SOURCE_DIR}/src/gfss/anb_parse.cpp"
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
#
# opengl32/gdi32 were added for X-GL-0 (docs/plano-w6a-janela.md sec.
# 2.2 row 2): the file's own two new cases call ChoosePixelFormat/
# SetPixelFormat (Gdi32.lib/Gdi32.dll per their own Microsoft Learn
# pages) and wglCreateContext/wglMakeCurrent/wglDeleteContext/
# wglGetProcAddress/glGetString (Opengl32.lib/Opengl32.dll per theirs) -
# same dependency-zero reasoning as user32 above: both are Windows
# system libraries this probe links against directly, never a header
# or library this project vendors or ships.
if(WIN32)
    glintfx_add_test(win32_runner_probe_test)
    target_link_libraries(win32_runner_probe_test PRIVATE user32 opengl32 gdi32)
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

# W-D' (docs/plano-w6a-janela.md fatia 6): display_backend_port, the
# sibling concept display_connection_port.hpp's own header comment
# promised - see display_backend_port_concept_test.cpp's own header
# comment for why fake_display_adapter is reused UNCHANGED as this
# concept's own NEGATIVE control instead of being extended with a
# pump_events() it was never meant to carry. OS-agnostic, same
# reasoning as the two cases just above (no Wayland/Win32 type
# reachable from this TU).
glintfx_add_test(display_backend_port_concept_test)
target_include_directories(display_backend_port_concept_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# W-C' (docs/plano-w6a-janela.md fatia 4, GODS_LAWS.md L-04): the
# window's neutral state, its text-field validation, and the seat's
# input-device-class facts - all five files below are OS-agnostic (no
# Wayland or Win32 type reachable from any of them, same as the three
# cases just above), so all five are registered UNGUARDED, never
# inside an if(UNIX)/if(WIN32) block, and each compiles its own .cpp a
# SECOND time directly into the test (none of these types has
# GLINTFX_API - GODS_LAWS.md L-19, "nada e exportado" - so there is no
# hidden library symbol to link against, only source to recompile,
# same technique display_port_concept_test's own neighbors use above).
glintfx_add_test(window_state_test)
target_include_directories(window_state_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(window_state_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
)

glintfx_add_test(utf8_validation_test)
target_include_directories(utf8_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(utf8_validation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
)

glintfx_add_test(window_desc_validation_test)
target_include_directories(window_desc_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(window_desc_validation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
    "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
)

glintfx_add_test(seat_capabilities_test)
target_include_directories(seat_capabilities_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(seat_capabilities_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/input/seat_capabilities.cpp"
)

# C-OPT (docs/plano-w6b-placa-e-laco.md fatia 2a, GODS_LAWS.md L-17/
# L-19/L-20/L-26/L-40): the append-only graphics-option registry
# (include/glintfx/platform/gl/gfx_option.hpp, gltfx_gfx_option and its
# four public discovery functions) and its two pure atoms - see each
# test file's own header comment. All three are OS-agnostic (no
# Wayland/Win32/EGL/WGL type reachable from any of them), registered
# UNGUARDED like window_state_test and its neighbors just above.
#
# gfx_option_registry_test only exercises the PUBLIC header (gfx_
# option_registry.cpp's own four GLINTFX_API functions are already
# linked in through glintfx::glintfx below) - no "${PROJECT_SOURCE_DIR}
# /src" include dir needed, same as gfui_node_state_test above needs
# none either.
glintfx_add_test(gfx_option_registry_test)

# gfx_option_validation_test and gfx_open_only_fixation_test recompile
# their own atom's .cpp a second time (neither carries GLINTFX_API -
# GODS_LAWS.md L-19, "nada e exportado" - so there is no hidden library
# symbol to link against, only source to recompile, the same technique
# window_desc_validation_test above already uses). gfx_option_registry.
# hpp's own table and find_gfx_option_row() are `inline` - reachable
# from both without a third recompiled .cpp.
glintfx_add_test(gfx_option_validation_test)
target_include_directories(gfx_option_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gfx_option_validation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gfx_option_validation.cpp"
)

glintfx_add_test(gfx_open_only_fixation_test)
target_include_directories(gfx_open_only_fixation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gfx_open_only_fixation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gfx_open_only_fixation.cpp"
)

# GL-CONTEXT (docs/plano-w6b-placa-e-laco.md fatia 2b, GODS_LAWS.md
# L-17/L-19/L-20/L-26/L-40): the four OS-agnostic pure atoms this fatia
# adds on top of C-OPT above - the descriptor-shape validator, the GL
# 3.3-core version policy, the EGL-surface resize policy (D-W6b-5) and
# the GPU-kind storage atom (D-W6b-13) - plus a compile-only proof that
# gl_context_adapter_port (src/platform/port/) is genuinely satisfiable
# by a real type. None of the five reaches a Wayland/Win32/EGL/WGL
# type, so all five register UNGUARDED, exactly like the C-OPT trio
# just above.
glintfx_add_test(gl_context_desc_validation_test)
target_include_directories(gl_context_desc_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gl_context_desc_validation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_context_desc_validation.cpp"
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gfx_option_validation.cpp"
)

glintfx_add_test(gl_version_policy_test)
target_include_directories(gl_version_policy_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gl_version_policy_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_version_policy.cpp"
)

glintfx_add_test(gl_surface_size_policy_test)
target_include_directories(gl_surface_size_policy_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gl_surface_size_policy_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_surface_size_policy.cpp"
)

# gpu_kind_state.hpp is entirely inline (no .cpp, same reasoning
# gfx_option_registry.hpp's own inline table needs none) - the public
# gpu.hpp it sits on top of is already linked in through
# glintfx::glintfx below.
glintfx_add_test(gpu_kind_state_test)
target_include_directories(gpu_kind_state_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

glintfx_add_test(gl_context_adapter_port_concept_test)
target_include_directories(gl_context_adapter_port_concept_test PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Wayland-DOMAIN, not Wayland-ONLY (see this file's own header comment
# above global_catalog_test on the exact same distinction, and this
# file's own header comment for why guarding this one would repeat
# that fixed mistake): window_configure_sequence.{hpp,cpp} lives under
# src/platform/wayland/ because the xdg_toplevel_state numbers it
# translates are Wayland-domain knowledge, but the CODE ITSELF never
# includes a Wayland header or touches a Wayland type - registered
# unguarded so it runs, and proves the same behavior, on all five
# platforms.
glintfx_add_test(window_configure_sequence_test)
target_include_directories(window_configure_sequence_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(window_configure_sequence_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/wayland/window_configure_sequence.cpp"
)

# frame_callback_sequence.{hpp,cpp} (W-EGL, docs/plano-w6b-placa-e-
# laco.md fatia 3, D-W6b-6) is the SAME "Wayland-DOMAIN, not Wayland-
# ONLY" shape as window_configure_sequence just above: it lives under
# src/platform/wayland/ because wl_surface.frame is Wayland-domain
# knowledge, but the atom itself only ever sees a bool and a budget in
# milliseconds - no wl_callback, no EGLDisplay, no socket - so it
# registers unguarded and runs on all five platforms.
glintfx_add_test(frame_callback_sequence_test)
target_include_directories(frame_callback_sequence_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(frame_callback_sequence_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/wayland/frame_callback_sequence.cpp"
)

# GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec. 4.4; docs/plano-w6b-
# fatias-5b-revisao.md sec. 4.3, D-W6b-30/33/38): the SAME "domain, not
# only" shape as window_configure_sequence_test/frame_callback_
# sequence_test right above - drm_gpu_kind.cpp/egl_device_dedup.cpp
# live under src/platform/wayland/ because the KNOWLEDGE (DRM driver
# names, EGLDeviceEXT render nodes) is Wayland/Linux-domain, but
# neither .cpp reaches a Wayland or Linux header - both register
# unguarded and run on all five platforms.
glintfx_add_test(drm_gpu_kind_test)
target_include_directories(drm_gpu_kind_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(drm_gpu_kind_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/wayland/drm_gpu_kind.cpp"
)

glintfx_add_test(egl_device_dedup_test)
target_include_directories(egl_device_dedup_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(egl_device_dedup_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/wayland/egl_device_dedup.cpp"
)

# GL-GPU-KIND, vias 1/2 (docs/plano-w6b-fatias-5b-revisao.md sec.
# 1.5/3/4.2, D-W6b-38): OS-AGNOSTIC on purpose - they reason about an
# already-classified gltfx_gpu_kind enumeration (via 1) or a GL_NVX_
# gpu_memory_info reading off the current context (via 2), used by
# BOTH the Linux "nvidia-drm answers nothing" case and the Win32
# reserve for when DXCore itself does not answer IsIntegrated (D-W6b-
# 37 regra 4) - same reasoning src/platform/gl/CMakeLists.txt's own
# gl_context_facade.cpp comment already gives for living in gl/.
glintfx_add_test(gpu_kind_exclusion_test)
target_include_directories(gpu_kind_exclusion_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gpu_kind_exclusion_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gpu_kind_exclusion.cpp"
)

glintfx_add_test(memory_separation_kind_test)
target_include_directories(memory_separation_kind_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(memory_separation_kind_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/memory_separation_kind.cpp"
)

# CONSERTO (07/09/2026, GL-GPU-KIND, revisão adversarial da fatia 5b):
# a COSTURA entre kernel/DXCore -> via 1 -> via 2 (classify_current_
# gpu(), duplicada nos dois arquivos platform/{wayland,win32}/*_
# context_adapter.cpp) não tinha teste nenhum - gpu_kind_seam.hpp/.cpp's
# own header comment has the full account, RED literal included.
# Depende de gpu_kind_exclusion.cpp (via 1) pela mesma razão que
# gpu_kind_seam.cpp já depende dele em src/platform/gl/CMakeLists.txt.
glintfx_add_test(gpu_kind_seam_test)
target_include_directories(gpu_kind_seam_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gpu_kind_seam_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gpu_kind_seam.cpp"
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gpu_kind_exclusion.cpp"
)

# CONSERTO (06/09/2026, GL-GPU-KIND, achado do team-lead medido dentro
# do container real: um driver que responde "GL_NVX_gpu_memory_info
# suportada" mas escreve memoria nao inicializada de volta) -
# read_gl_memory_facts.cpp's own header comment has the full account.
glintfx_add_test(gl_memory_facts_test)
target_include_directories(gl_memory_facts_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(gl_memory_facts_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_memory_facts.cpp"
)

# GL-GPU-KIND, Windows (docs/plano-w6b-fatias-5b-revisao.md sec.
# 3/4.1/4.3, D-W6b-37): Win32-DOMAIN, not Win32-ONLY - the SAME shape
# as drm_gpu_kind_test above, mirrored for the DXCore side: neither
# dxcore_adapter_match.cpp nor dxcore_gpu_kind.cpp reaches a Win32 or
# COM header, so both register unguarded and run on all five
# platforms.
glintfx_add_test(dxcore_adapter_match_test)
target_include_directories(dxcore_adapter_match_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(dxcore_adapter_match_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_adapter_match.cpp"
)

glintfx_add_test(dxcore_gpu_kind_test)
target_include_directories(dxcore_gpu_kind_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(dxcore_gpu_kind_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_gpu_kind.cpp"
)

# LOOP-RUN fatia 6a (docs/plano-w6b-fatias-6-8.md sec. 8.1, GODS_LAWS.md
# L-17/L-19/L-20/L-40): three pure atoms under src/platform/loop/, none
# reaching an operating-system header - the same "no #if anywhere in
# it, GODS_LAWS.md L-04 applied to a value type" shape window_state_
# test.cpp already gives one directory over - so all three register
# unguarded and run on all five platforms, exactly like frame_callback_
# sequence_test just above.
glintfx_add_test(frame_tick_state_test)
target_include_directories(frame_tick_state_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(frame_tick_state_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/loop/frame_tick_state.cpp"
)

glintfx_add_test(frame_cap_schedule_test)
target_include_directories(frame_cap_schedule_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(frame_cap_schedule_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/loop/frame_cap_schedule.cpp"
)

glintfx_add_test(loop_callbacks_validation_test)
target_include_directories(loop_callbacks_validation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_sources(loop_callbacks_validation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src/platform/loop/loop_callbacks_validation.cpp"
)

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
        # GL-CONTEXT fatia 5 (D-W6b-28): display_adapter.cpp's own
        # calls now resolve to connection_failure.cpp's own build_
        # connection_failure(), extracted out of this file - the same
        # "second compile needs every callee linked alongside it"
        # reasoning global_catalog.cpp's own line above already gives.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/connection_failure.cpp"
        # INBOX (drenagem 06/09/2026): display_adapter.cpp's own
        # flush_with_retry() now calls bounded_output_wait.cpp's own
        # wait_for_writable_until() - the SAME "second compile needs
        # every callee linked alongside it" reasoning this block's own
        # header comment already gives for connection_failure.cpp
        # right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        # LOOP-RUN fatia 7, conserto pos-revisao (D-W6b-57, 07/09/2026):
        # flush_with_retry() now also calls flush_retry_policy.cpp's
        # own flush_write_wait_is_fatal() - the SAME "second compile
        # needs every callee linked alongside it" reasoning as bounded_
        # output_wait.cpp right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
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

    # WL-WINDOW fatia W-A (docs/plano-w6a-janela.md fatia 3, plano W5
    # sec. 1.2): wayland_display_adapter::bind()'s own refusal path on
    # an adapter never opened - same second-compile technique as
    # display_connect_failure_test right above it (glintfx.so's hidden
    # visibility, this block's own header comment).
    glintfx_add_test(display_bind_refusal_test)
    target_include_directories(display_bind_refusal_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        ${GlintfxWaylandClient_INCLUDE_DIRS}
    )
    target_sources(display_bind_refusal_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/connection_failure.cpp"
        # INBOX (drenagem 06/09/2026): display_adapter.cpp's own
        # flush_with_retry() now calls bounded_output_wait.cpp's own
        # wait_for_writable_until() - the SAME "second compile needs
        # every callee linked alongside it" reasoning this block's own
        # header comment already gives for connection_failure.cpp
        # right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        # LOOP-RUN fatia 7, conserto pos-revisao (D-W6b-57, 07/09/2026):
        # flush_with_retry() now also calls flush_retry_policy.cpp's
        # own flush_write_wait_is_fatal() - the SAME "second compile
        # needs every callee linked alongside it" reasoning as bounded_
        # output_wait.cpp right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
    )
    target_link_directories(display_bind_refusal_test PRIVATE ${GlintfxWaylandClient_LIBRARY_DIRS})
    target_link_libraries(display_bind_refusal_test PRIVATE ${GlintfxWaylandClient_LIBRARIES})
    target_compile_options(display_bind_refusal_test PRIVATE ${GlintfxWaylandClient_CFLAGS_OTHER})

    # GL-CONTEXT fatia 1 (docs/plano-w6b-placa-e-laco.md, G-1): the
    # raw wl_display* accessor the EGL probe needs - same second-
    # compile technique as display_bind_refusal_test right above it
    # (glintfx.so's hidden visibility, this block's own header
    # comment). No compositor reachable from this TU (see the test
    # file's own header comment).
    glintfx_add_test(display_native_display_test)
    target_include_directories(display_native_display_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        ${GlintfxWaylandClient_INCLUDE_DIRS}
    )
    target_sources(display_native_display_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/connection_failure.cpp"
        # INBOX (drenagem 06/09/2026): display_adapter.cpp's own
        # flush_with_retry() now calls bounded_output_wait.cpp's own
        # wait_for_writable_until() - the SAME "second compile needs
        # every callee linked alongside it" reasoning this block's own
        # header comment already gives for connection_failure.cpp
        # right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        # LOOP-RUN fatia 7, conserto pos-revisao (D-W6b-57, 07/09/2026):
        # flush_with_retry() now also calls flush_retry_policy.cpp's
        # own flush_write_wait_is_fatal() - the SAME "second compile
        # needs every callee linked alongside it" reasoning as bounded_
        # output_wait.cpp right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
    )
    target_link_directories(display_native_display_test PRIVATE ${GlintfxWaylandClient_LIBRARY_DIRS})
    target_link_libraries(display_native_display_test PRIVATE ${GlintfxWaylandClient_LIBRARIES})
    target_compile_options(display_native_display_test PRIVATE ${GlintfxWaylandClient_CFLAGS_OTHER})

    # WL-WINDOW fatia W-B (docs/plano-w6a-janela.md fatia 5): the pure
    # half of "does this compositor announce what a shell needs" -
    # check_shell_requirements() against a global_catalog built by
    # hand, no compositor, no wayland-client link required at all (no
    # Wayland type is reachable from shell_requirements.{hpp,cpp} -
    # see that header's own comment). The real bind() against a live
    # compositor is fatia 5's own shell_smoke, inside the container
    # (GODS_LAWS.md L-09) - if(UNIX)-guarded here (not registered
    # unguarded like global_catalog_test/window_configure_sequence_
    # test above) because its Windows-side twin is a DIFFERENT
    # mechanism entirely (win32_display_connect_test, the window-class
    # factory accepting registration), an ALIAS in
    # tests/parity_aliases.txt, not the same code running on both
    # platforms.
    glintfx_add_test(shell_requirements_test)
    target_include_directories(shell_requirements_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(shell_requirements_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/shell_requirements.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
    )

    # WL-WINDOW fatia W-E (docs/plano-w6a-janela.md fatia 8): the pure-
    # listener half of wayland_window_adapter - every callback exercised
    # DIRECTLY with a null proxy (this fatia's own briefing: "callbacks
    # com proxy nulo"), never a real wl_surface/xdg_surface/xdg_toplevel
    # request - configure translation (window_configure_sequence.hpp),
    # the buffer-scale factor feeding window_state::apply_buffer_scale(),
    # and the close latch are all PURE reads/writes of window_state, the
    # same property the callback declarations' own header comments
    # already document. open()/wait_first_configure() themselves (the
    # paths that DO call into a live compositor) are proven for real by
    # window_smoke, inside the container (GODS_LAWS.md L-09), via P-0's
    # own inventory - never by this target.
    #
    # display_adapter.cpp/global_catalog.cpp are linked here ONLY
    # because window_adapter.cpp's own open()/wait_first_configure()
    # (compiled into this SAME translation unit, whether or not this
    # test ever calls them) reference wayland_display_adapter::
    # roundtrip() - the same "the whole .cpp is one link unit" reasoning
    # display_bind_refusal_test's own block above already relies on.
    # wayland_shell_adapter's own is_open()/compositor()/shell() are all
    # inline in shell_adapter.hpp (no out-of-line symbol to resolve), so
    # shell_adapter.cpp/shell_requirements.cpp are deliberately absent
    # from this list.
    glintfx_add_test(window_adapter_listener_test)
    target_include_directories(window_adapter_listener_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(window_adapter_listener_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/window_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/window_configure_sequence.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/connection_failure.cpp"
        # INBOX (drenagem 06/09/2026): display_adapter.cpp's own
        # flush_with_retry() now calls bounded_output_wait.cpp's own
        # wait_for_writable_until() - the SAME "second compile needs
        # every callee linked alongside it" reasoning this block's own
        # header comment already gives for connection_failure.cpp
        # right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        # LOOP-RUN fatia 7, conserto pos-revisao (D-W6b-57, 07/09/2026):
        # flush_with_retry() now also calls flush_retry_policy.cpp's
        # own flush_write_wait_is_fatal() - the SAME "second compile
        # needs every callee linked alongside it" reasoning as bounded_
        # output_wait.cpp right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
    )
    glintfx_add_wayland_xdg_shell_binding(window_adapter_listener_test)

    # WL-SEAT fatia S-B (docs/plano-w6a-janela.md fatia 12): the pure-
    # listener half of wayland_seat_adapter - both callbacks
    # (capabilities, name) exercised DIRECTLY with a null wl_seat*
    # proxy, the same "callbacks com proxy nulo" technique window_
    # adapter_listener_test above already uses. Needs only wayland-
    # client itself (wl_seat_interface, the WL_SEAT_CAPABILITY_*
    # bitmask constants, wl_seat_listener) - no xdg-shell protocol
    # binding is reachable from seat_adapter.{hpp,cpp}, so this links
    # GlintfxWaylandClient_* directly (display_connect_failure_test's
    # own block above documents these variables), never glintfx_add_
    # wayland_xdg_shell_binding().
    #
    # display_adapter.cpp/global_catalog.cpp are linked here ONLY
    # because wayland_seat_adapter::open() (compiled into this SAME
    # translation unit, whether or not this test ever calls it)
    # references wayland_display_adapter::globals()/bind() - the same
    # "the whole .cpp is one link unit" reasoning window_adapter_
    # listener_test's own block above already documents for wayland_
    # window_adapter::open(). seat_capabilities.cpp is linked because
    # wl_seat_capabilities() calls seat_capabilities::set_capability().
    #
    # open()/close() themselves (the paths that DO call into a live
    # compositor, including the version-aware wl_seat_release()/wl_
    # seat_destroy() choice in close()) are proven for real by
    # seat_test, inside the container (GODS_LAWS.md L-09, tests/
    # container/seat_test.cpp), via P-0's own inventory - never by
    # this target.
    glintfx_add_test(seat_adapter_listener_test)
    target_include_directories(seat_adapter_listener_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        ${GlintfxWaylandClient_INCLUDE_DIRS}
    )
    target_sources(seat_adapter_listener_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/seat_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/global_catalog.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/connection_failure.cpp"
        # INBOX (drenagem 06/09/2026): display_adapter.cpp's own
        # flush_with_retry() now calls bounded_output_wait.cpp's own
        # wait_for_writable_until() - the SAME "second compile needs
        # every callee linked alongside it" reasoning this block's own
        # header comment already gives for connection_failure.cpp
        # right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        # LOOP-RUN fatia 7, conserto pos-revisao (D-W6b-57, 07/09/2026):
        # flush_with_retry() now also calls flush_retry_policy.cpp's
        # own flush_write_wait_is_fatal() - the SAME "second compile
        # needs every callee linked alongside it" reasoning as bounded_
        # output_wait.cpp right above.
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/input/seat_capabilities.cpp"
    )
    target_link_directories(seat_adapter_listener_test PRIVATE ${GlintfxWaylandClient_LIBRARY_DIRS})
    target_link_libraries(seat_adapter_listener_test PRIVATE ${GlintfxWaylandClient_LIBRARIES})
    target_compile_options(seat_adapter_listener_test PRIVATE ${GlintfxWaylandClient_CFLAGS_OTHER})
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
        # window_message_route.cpp/window_state.cpp - link-only (X-2/
        # WIN-WINDOW fatia 9's own review, GODS_LAWS.md L-04): display_
        # adapter.cpp's own shared window_proc has a real call to win32_
        # window_adapter_route_message() (window_message_route.hpp),
        # which must resolve at LINK time even though this fixture's own
        # message-only window never reaches the branch that calls it
        # (win32_window_adapter's own instances are never created here -
        # see window_message_route.hpp's own "SAFE ONLY BECAUSE OF WHAT
        # THE FOUR MESSAGE TYPES THEMSELVES MEAN" paragraph). window_
        # state.cpp is handle_message()'s own dependency, not window_
        # adapter.cpp's open()/set_title() ones - see window_message_
        # route.cpp's own header comment for why THIS pair, not the
        # whole of window_adapter.cpp plus app_user_model_id.cpp.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(win32_display_connect_test PRIVATE user32)
endif()

# PARITY-GATE (achado do servidor real, 05/09/2026, run 34043723220):
# o par Windows de tests/display_native_display_test.cpp - o mesmo
# fato de fronteira (o acessor de handle nativo comeca nulo antes de
# open()), so que para win32_display_adapter::native_handle() em vez
# de wayland_display_adapter::native_display(). Mesmo if(WIN32)/
# segunda-compilacao que win32_display_connect_test acima (win32_
# display_adapter nao tem GLINTFX_API - GODS_LAWS.md L-19).
if(WIN32)
    glintfx_add_test(win32_display_native_handle_test)
    target_include_directories(win32_display_native_handle_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_display_native_handle_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, mesma
        # razao do bloco win32_display_connect_test acima (o wndproc
        # compartilhado de display_adapter.cpp chama win32_window_
        # adapter_route_message() em tempo de link, mesmo o adaptador
        # sob teste nunca chegando a abrir uma janela real aqui).
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(win32_display_native_handle_test PRIVATE user32)
endif()

# WIN-WINDOW fatia 7 (X-1', docs/plano-w6a-janela.md, D-W6a-16,
# 05/09/2026): the two witnesses of THIS fatia's own fix for F2 (the
# fixed window-class name measured against `main` at 270bef3, before
# this fatia - a second win32_display_adapter::open() in the same
# process used to refuse with ERROR_CLASS_ALREADY_EXISTS). Same
# if(WIN32)/second-compilation shape as win32_display_connect_test
# right above (win32_display_adapter has no GLINTFX_API, glintfx.dll's
# own hidden visibility never exports these methods).
if(WIN32)
    # Provokes the SAME refusal on purpose, against a single adapter,
    # by pre-registering (from outside the adapter) the exact name
    # class_name_for() says open() is about to ask for - see this
    # file's own header comment for why a per-instance name (D-W6a-16)
    # means the test has to COMPUTE that name rather than reuse a
    # single well-known constant.
    glintfx_add_test(win32_display_refusal_test)
    target_include_directories(win32_display_refusal_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_display_refusal_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, same
        # reason as win32_display_connect_test's own block above (see
        # its comment for the full reasoning).
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(win32_display_refusal_test PRIVATE user32)

    # NAMED "two_displays_test", not "win32_two_displays_test" -
    # docs/plano-w6a-janela.md fatia 7's own "Par no portao" column
    # requires this EXACT name so P-0's inventory (tests/tools/
    # check_test_parity.py) can line it up against the Wayland side's
    # own container fixture of the same name, staged separately under
    # tests/container/ - never a per-platform prefix here. Opens TWO
    # win32_display_adapter instances in the same process; before this
    # fatia's fix, the second open() would have failed exactly the way
    # win32_display_refusal_test provokes on purpose above.
    glintfx_add_test(two_displays_test)
    target_include_directories(two_displays_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(two_displays_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, same
        # reason as win32_display_connect_test's own block above (see
        # its comment for the full reasoning).
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(two_displays_test PRIVATE user32)
endif()

# Y-1 (docs/plano-w6a-janela.md fatia 13, TODO.md WIN-SEAT): the Win32
# counterpart of fatia 12's own S-B (Wayland seat, src/platform/
# wayland/). Same if(WIN32)/second-compilation shape as win32_display_
# connect_test/win32_display_refusal_test above (win32_seat_adapter has
# no GLINTFX_API, glintfx.dll's own hidden visibility never exports
# these methods).
if(WIN32)
    # "listas sinteticas" (the plan's own name for this fixture):
    # win32_seat_adapter::translate() exercised directly against
    # hand-built RAWINPUTDEVICELIST arrays and digitizer bitmasks - no
    # window, no RegisterRawInputDevices. display_adapter.cpp is linked
    # alongside it only because seat_adapter.hpp includes display_
    # adapter.hpp (win32_seat_adapter::open() takes a `const win32_
    # display_adapter &`) - this fixture never calls open(). Alias of
    # seat_adapter_listener_test (tests/parity_aliases.txt) - same
    # "proxy nulo"/"listas sinteticas" role, different mechanism
    # (GODS_LAWS.md L-04).
    glintfx_add_test(win32_seat_translation_test)
    target_include_directories(win32_seat_translation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_seat_translation_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/seat_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/input/seat_capabilities.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, same
        # reason as win32_display_connect_test's own block above (see
        # its comment for the full reasoning) - display_adapter.cpp is
        # what actually references win32_window_adapter_route_message()
        # here, not seat_adapter.cpp.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(win32_seat_translation_test PRIVATE user32)

    # NAMED "seat_test", not "win32_seat_test" - same requirement as
    # two_displays_test above (docs/plano-w6a-janela.md fatia 13's own
    # "Par no portao" column, tests/container/seat_test.cpp's own
    # header comment): P-0's inventory needs this EXACT name to line up
    # against the Wayland side's own container fixture of the same
    # name. A real win32_display_adapter and a real win32_seat_adapter
    # opened against the windows-latest runner - RegisterRawInput
    # Devices for real, capabilities() read from the runner's own
    # GetRawInputDeviceList()/GetSystemMetrics(SM_DIGITIZER), and a
    # SYNTHETIC WM_INPUT_DEVICE_CHANGE (SendMessageW, never a real
    # unplug/replug) proving the GWLP_WNDPROC subclass routes to this
    # adapter instance - see this fixture's own header comment.
    glintfx_add_test(seat_test)
    target_include_directories(seat_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(seat_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/seat_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/input/seat_capabilities.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, same
        # reason as win32_display_connect_test's own block above (see
        # its comment for the full reasoning) - display_adapter.cpp is
        # what actually references win32_window_adapter_route_message()
        # here, not seat_adapter.cpp.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(seat_test PRIVATE user32)
endif()

# X-2 (docs/plano-w6a-janela.md fatia 9, TODO.md WIN-WINDOW): win32_
# window_adapter, the Windows counterpart of WL-WINDOW's own W-E
# (window_adapter_listener_test above). Same if(WIN32)/second-
# compilation shape as win32_display_connect_test/win32_seat_
# translation_test above (win32_window_adapter has no GLINTFX_API,
# glintfx.dll's own hidden visibility never exports these methods) -
# display_adapter.cpp is linked alongside both fixtures below because
# window_adapter.cpp's own window_proc extension (window_message_
# route.hpp) lives there, and window_adapter.hpp takes a `const win32_
# display_adapter &`; window_state.cpp/window_desc_validation.cpp/
# utf8_validation.cpp because win32_window_adapter::open() calls
# validate_window_text_field() (D-W6a-23) and writes into window_state
# (D-W5-14). app_user_model_id.cpp (D-W6a-21) is linked so the
# application_id path compiles, even though neither fixture below
# exercises it against a real property store.
if(WIN32)
    # "mensagens sinteticas" (the plan's own name for this fixture):
    # handle_message() exercised DIRECTLY with hand-built wParam/lParam
    # values - no window, no CreateWindowExW, no live message queue.
    # Alias of window_adapter_listener_test (tests/parity_aliases.txt) -
    # same "proxy nulo"/"listas sinteticas"/"mensagens sinteticas" role,
    # different mechanism (GODS_LAWS.md L-04).
    glintfx_add_test(win32_message_translation_test)
    target_include_directories(win32_message_translation_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_message_translation_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_adapter.cpp"
        # window_message_route.cpp: handle_message() itself now lives
        # here, split out of window_adapter.cpp (X-2/WIN-WINDOW fatia 9's
        # own review, GODS_LAWS.md L-17/L-04) - this fixture calls it
        # directly ("mensagens sinteticas" above), so it is no longer
        # optional the way it is for the display/seat-only fixtures.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/app_user_model_id.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
    )
    target_link_libraries(win32_message_translation_test PRIVATE user32 shell32 ole32 propsys)

    # Sec. 5 risk 6 of the plan ("WM_CLOSE devolvido a DefWindowProc"):
    # a REAL window (windows-latest runner, the same interactive window
    # station win32_display_connect_test.cpp already proves), a REAL
    # SendMessageW(WM_CLOSE) from the creating thread, and IsWindow()
    # still true afterward - the half win32_message_translation_test
    # above cannot prove without a live HWND. Alias of window_adapter_
    # listener_test (tests/parity_aliases.txt): the "close pegajoso e
    # janela viva" case is the same fact the Wayland side's own
    # xdg_toplevel_close case already covers, by a completely different
    # mechanism (GODS_LAWS.md L-04).
    glintfx_add_test(win32_window_close_request_test)
    target_include_directories(win32_window_close_request_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_window_close_request_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_adapter.cpp"
        # window_message_route.cpp: the real SendMessageW(WM_CLOSE) below
        # reaches handle_message() through display_adapter.cpp's own
        # shared window_proc, exactly like production code - split out
        # of window_adapter.cpp (X-2/WIN-WINDOW fatia 9's own review,
        # GODS_LAWS.md L-17/L-04), so it is linked here explicitly now.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/app_user_model_id.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
    )
    target_link_libraries(win32_window_close_request_test PRIVATE user32 shell32 ole32 propsys)
endif()

# X-WGL (docs/plano-w6b-placa-e-laco.md fatia 4, D-W6b-1/4/6/7/17/18,
# GODS_LAWS.md L-04/L-09/L-40): the Win32 mirror of W-EGL's own frame_
# callback_sequence_test/egl_probe_smoke (fatia 3) - win32_gl_context_
# adapter, wgl_extension_loader and wgl_proc_address have no GLINTFX_
# API (GODS_LAWS.md L-19), so every fixture below compiles the
# production .cpp files a SECOND time, the same technique every other
# win32/*_test target above already uses.
if(WIN32)
    # No window, no device context, no rendering context at all (this
    # fixture's own header comment) - only wgl_proc_address.cpp itself,
    # linked against opengl32 the same way glintfx_library now does
    # (src/platform/win32/CMakeLists.txt).
    glintfx_add_test(win32_wgl_proc_address_test)
    target_include_directories(win32_wgl_proc_address_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_wgl_proc_address_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/wgl_proc_address.cpp"
    )
    target_link_libraries(win32_wgl_proc_address_test PRIVATE opengl32)

    # The disposable-window dance (wgl_extension_loader.hpp's own
    # header comment) needs RegisterClassExW/CreateWindowExW (user32),
    # ChoosePixelFormat/SetPixelFormat/DescribePixelFormat (gdi32) and
    # wglCreateContext/wglMakeCurrent/wglDeleteContext/wglGetProcAddress
    # (opengl32) - the same three libraries tests/win32_runner_probe_
    # test.cpp's own CMakeLists.txt block already links for the
    # identical set of calls.
    glintfx_add_test(win32_wgl_extension_loader_test)
    target_include_directories(win32_wgl_extension_loader_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_wgl_extension_loader_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/wgl_extension_loader.cpp"
    )
    target_link_libraries(win32_wgl_extension_loader_test PRIVATE user32 gdi32 opengl32)

    # dxcore_reader_probe_test - GL-GPU-KIND (docs/plano-w6b-fatias-5b-
    # revisao.md sec. 4.6): "the leitor ran against the real system and
    # this is what it saw", paired with tests/container/egl_device_
    # reader_probe.cpp (tests/parity_aliases.txt) - the SAME "link src/
    # directly, no guard needed inside an already-if(WIN32) block"
    # shape win32_wgl_extension_loader_test just above uses. No system
    # library beyond what dxcore_adapter_enumeration.cpp itself needs
    # (kernel32's LoadLibraryW/GetProcAddress, already linked
    # transitively through user32/the CRT - dxcore.dll is NEVER a link-
    # time dependency, only a runtime LoadLibraryW target).
    glintfx_add_test(dxcore_reader_probe_test)
    target_include_directories(dxcore_reader_probe_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(dxcore_reader_probe_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_adapter_enumeration.cpp"
    )

    # win32_iconic_present_test - NAMED to line up with docs/plano-w6b-
    # placa-e-laco.md's own fatia 4 table row ("Prova Windows" column):
    # a REAL win32_display_adapter + win32_window_adapter + win32_gl_
    # context_adapter, the same "second compilation" set win32_window_
    # close_request_test above already links, plus this fatia's own
    # three GL atoms and gl_version_policy.cpp (create_context()'s own
    # dependency, fatia 2b, already frozen).
    glintfx_add_test(win32_iconic_present_test)
    target_include_directories(win32_iconic_present_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_iconic_present_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/app_user_model_id.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/wgl_context_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/wgl_extension_loader.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/wgl_proc_address.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_version_policy.cpp"
        # GL-GPU-KIND (achado do team-lead, 07/09/2026 - o mesmo defeito do
        # Containerfile, num segundo lugar onde fontes sao listadas a mao):
        # wgl_context_adapter.cpp's own #include list (this file's own
        # header comment) pulls in every one of these six - LNK2019 for
        # all of them until this line existed.
        "${PROJECT_SOURCE_DIR}/src/platform/gl/gl_memory_facts.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/gl/gpu_kind_exclusion.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/gl/gpu_kind_seam.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/gl/memory_separation_kind.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_adapter_enumeration.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_adapter_match.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/dxcore_gpu_kind.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/gl_device_luid.cpp"
    )
    target_link_libraries(win32_iconic_present_test PRIVATE user32 shell32 ole32 propsys gdi32 opengl32)

    # win32_wait_events_test - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-
    # 6-8.md sec. 8.2, D-W6b-50, "Prova Windows" column): the Win32
    # mirror of tests/container/wait_events_smoke.cpp, over a REAL
    # win32_display_adapter, same "second compilation" set win32_
    # display_connect_test above already links (win32_display_adapter's
    # own message-only window shares the SAME window_proc/GWLP_USERDATA
    # routing every other win32 fixture in this suite reuses).
    glintfx_add_test(win32_wait_events_test)
    target_include_directories(win32_wait_events_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(win32_wait_events_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        # window_message_route.cpp/window_state.cpp - link-only, the
        # SAME reasoning win32_display_connect_test's own comment above
        # gives in full for the identical pair.
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
    )
    target_link_libraries(win32_wait_events_test PRIVATE user32)
endif()

# WL-WINDOW-HANDLE (docs/plano-w6a-janela.md fatia 11, absorved into
# one fatia by the CTO's own decision): window_parity_test.cpp
# (tests/parity/) is the ONE file, only-public-API, no-#if proof this
# project's own paridade law (GODS_LAWS.md L-04) exists to require -
# see that file's own header comment. NOT registered through
# glintfx_add_test() (cmake/GlintfxTest.cmake): that function expects a
# GLINTFX_TEST-based source using glintfx_test_harness, and this file
# is a plain int main() on purpose, the SAME shape the Linux side needs
# to be staged and run as a container fixture (tests/container/
# Containerfile) unchanged. Linked against the PUBLIC glintfx::glintfx
# target directly (never a second recompilation of internal sources -
# the whole point of this fixture is proving the PACKAGED facade
# works), if(WIN32) only: the Linux side of this SAME name is the
# container fixture P-0 (tests/tools/check_container_fixture_
# inventory.py) feeds into the paridade inventory instead, never a
# second ctest registration here.
if(WIN32)
    add_executable(window_parity_test "${CMAKE_CURRENT_SOURCE_DIR}/parity/window_parity_test.cpp")
    target_link_libraries(window_parity_test PRIVATE glintfx::glintfx)
    glintfx_apply_compile_options(window_parity_test)
    glintfx_copy_runtime_dlls_after_build(window_parity_test)
    add_test(NAME window_parity_test COMMAND window_parity_test)
    set_tests_properties(window_parity_test PROPERTIES LABELS consume)
endif()

# GL-CONTEXT fatia 5, P-GL (docs/plano-w6b-fatias-5.md sec. 3, D-W6b-
# 29): gl_context_parity_test.cpp (tests/parity/) is window_parity_
# test's own gemeo one layer up - ONE file, only-public-API, no-#if,
# proving gltfx_gl_context behaves identically on both systems
# (GODS_LAWS.md L-04). Same mechanism as window_parity_test right
# above: linked against the PUBLIC glintfx::glintfx target directly,
# if(WIN32) only here - the Linux side of this SAME name is the
# container fixture P-0 feeds into the paridade inventory instead.
if(WIN32)
    add_executable(gl_context_parity_test "${CMAKE_CURRENT_SOURCE_DIR}/parity/gl_context_parity_test.cpp")
    target_link_libraries(gl_context_parity_test PRIVATE glintfx::glintfx)
    glintfx_apply_compile_options(gl_context_parity_test)
    glintfx_copy_runtime_dlls_after_build(gl_context_parity_test)
    add_test(NAME gl_context_parity_test COMMAND gl_context_parity_test)
    set_tests_properties(gl_context_parity_test PROPERTIES LABELS consume)
endif()

# FACADE-PIN, T4/G3 (docs/plano-conserto-fachadas-uaf.md sec. 8/9):
# the Windows counterpart of facade_pin_smoke.cpp (tests/container/,
# T0) - paired by name in tests/parity_aliases.txt. UNLIKE window_
# parity_test/gl_context_parity_test right above, this target does
# NOT link glintfx::glintfx: it needs BOTH the public factories
# (gltfx_display::open()/gltfx_window::open(), exported with
# GLINTFX_API) AND the two internal passkeys display_internal_access::
# get()/window_internal_access::get() (declared in the public header,
# DEFINED only in display_facade.cpp/window_facade.cpp, GODS_LAWS.md
# L-19 - never placed in glintfx.dll's own export table, so a consumer
# linking against the packaged library can never resolve them).
# win32_facade_pin_test.cpp's own header comment carries the full
# reasoning. Every source below is the WHOLE chain those two public
# handles PIMPL over on Windows, recompiled standalone - the same "no
# CMake/pacote" shape facade_pin_smoke.cpp already uses on the Linux
# side (tests/container/Containerfile, plain g++).
if(WIN32)
    add_executable(win32_facade_pin_test "${CMAKE_CURRENT_SOURCE_DIR}/win32_facade_pin_test.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/display_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_adapter.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/window_message_route.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/win32/app_user_model_id.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/display_facade.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_facade.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_state.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/window_desc_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/window/utf8_validation.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/err.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/err_code.cpp"
    )
    target_include_directories(win32_facade_pin_test PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${GLINTFX_GENERATED_INCLUDE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
    )
    # CI run 34067447918: this target compiles display_facade.cpp/
    # window_facade.cpp/err.cpp/err_code.cpp DIRECTLY (this file's own
    # comment above, "does NOT link glintfx::glintfx"), so it never
    # inherits glintfx_apply_static_define()'s define the normal way
    # (PUBLIC target_link_libraries propagation - cmake/GlintfxLibrary.
    # cmake). Without it, include/glintfx/platform/window/display.hpp's
    # own GLINTFX_API declarations expand to __declspec(dllimport) (no
    # DLL is ever built here), and MSVC refuses to let the SAME TU both
    # import and DEFINE a symbol - C4273 "inconsistent dll linkage",
    # escalated to a hard error by /WX. Reproduced against the real
    # cl.exe (tools/msvc-container/README.md), fixed the SAME way that
    # README's own "uso do dia a dia" example already documents for a
    # standalone win32/*.cpp compile: this define makes GLINTFX_API
    # expand to nothing, matching that this target never crosses a DLL
    # boundary at all - the ordinary "no DLL, no decoration" case, not a
    # workaround specific to this test.
    target_compile_definitions(win32_facade_pin_test PRIVATE GLINTFX_LIBRARY_STATIC_DEFINE)
    target_link_libraries(win32_facade_pin_test PRIVATE glintfx_test_harness user32 shell32 ole32 propsys)
    glintfx_apply_compile_options(win32_facade_pin_test)
    glintfx_copy_runtime_dlls_after_build(win32_facade_pin_test)
    add_test(NAME win32_facade_pin_test COMMAND win32_facade_pin_test)
    set_tests_properties(win32_facade_pin_test PROPERTIES LABELS consume)
endif()

# Facade export-boundary gate (CI run 34067447918, commit ddd40b3):
# win32_facade_pin_test right above recompiled display_facade.cpp/
# window_facade.cpp/err.cpp/err_code.cpp directly, without linking
# glintfx::glintfx and without GLINTFX_LIBRARY_STATIC_DEFINE - C4273
# "inconsistent dll linkage" on the real cl.exe, invisible on GCC/Clang
# (this file's own comment on win32_facade_pin_test explains why) and
# invisible on THIS machine (the target is if(WIN32)-guarded, so it did
# not even exist here to warn about anything). This gate is a
# STRUCTURAL read of this very file - no compile, no Windows, no MSVC
# needed - so it catches the next occurrence of the same mistake on the
# machine that writes it, not only on the server five minutes later.
# Registered WITHOUT a platform guard (GODS_LAWS.md L-04): it reads
# text, builds nothing.
find_program(GLINTFX_PYTHON3_EXECUTABLE NAMES python3 python REQUIRED)
add_test(
    NAME facade_export_boundary_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_facade_export_boundary.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(facade_export_boundary_test PROPERTIES LABELS consume)

add_test(
    NAME facade_export_boundary_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_facade_export_boundary.py" --selftest
)
set_tests_properties(facade_export_boundary_selftest PROPERTIES LABELS selftest)

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
# check_public_name_collision.py's own enumerate_names_in_text() finds
# under PROJECT_SOURCE_DIR/include, checked against every "#define" the
# compiler's OWN system search path (discovered live via
# `${cxx} -E -Wp,-v -xc++ -`, never a hardcoded /usr/include) actually
# contains.
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"): PORTED from POSIX sh +
# tests/tools/enumerate_names.awk (a separate scratch file the sh
# version wrote at runtime) to a single python3 file that reimplements
# the five awk extraction rules natively - enumerate_names.awk is
# DELETED in this same fatia, now orphaned.
#
# REVISADO em 04/09/2026 (retorno do orquestrador sobre a primeira
# versao desta fatia): a versao anterior escondia o MSVC atras de
# `if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")`, com um
# `message(STATUS ...)` no `else()` que so aparece no log de
# configuracao - nenhum inventario de teste (`ctest -N`) o enxergava,
# e a ausencia nao era CONTADA em lugar nenhum que uma maquina possa
# ler. Corrigido seguindo o PRECEDENTE da casa (commit ed14ba2, "nome
# hostil inviavel no Windows vira ausencia DECLARADA, nao pulo
# silencioso", tests/tools/check_spdx.py): os dois testes sao
# registrados INCONDICIONALMENTE, nas cinco plataformas, recebendo
# CMAKE_CXX_COMPILER_ID como terceiro argumento; e' o proprio script
# (ver seu cabecalho, gcc_frontend_discovery_available()) quem decide,
# em tempo de execucao, se o mecanismo GCC-frontend (`-E -Wp,-v -xc++`,
# `-E -dM -xc++`) esta disponivel - no MSVC nao esta, e o script
# imprime a contagem (1 caso, 0 exercido, 1 declarado nao aplicavel,
# com o motivo e a cobertura equivalente) e sai com codigo 0, em vez de
# desaparecer do `ctest -N`.
#
# GLINTFX_PYTHON3_EXECUTABLE: cached find_program() result, already
# resolved earlier in this file (hygiene_coverage_test's own
# registration above) - find_program() is idempotent across repeated
# calls with the same cache variable, so it is not re-called here.
add_test(
    NAME public_name_collision_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_public_name_collision.py"
        "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CXX_COMPILER_ID}"
)
set_tests_properties(public_name_collision_test PROPERTIES LABELS consume)

# TIMEOUT override, MEASURED not guessed (GODS_LAWS.md L-11/L-44, achado
# real de 06/09/2026, CI run 34033291326): this test had been inheriting
# the SUITE-WIDE 120s default (cmake/GlintfxTest.cmake's own
# glintfx_set_default_test_timeout(), sized for lightweight unit tests
# and the standalone-CMake-project consumption gates, "a legitimate run
# finishes in a small fraction of it") until it timed out for the first
# time on "Windows - estatico". Before touching the number, the nine
# PRECEDING green runs of that same job were pulled from the server
# (`gh run view --job <id> --log`, never estimated) and their own
# "FASE" block (check_public_name_collision.py's own INSTRUMENT-PNC)
# compared: "nomes publicos" (178), "diretorios de sistema" (9) and
# "arquivos encontrados" (6217, ~389MB read) were IDENTICAL across all
# nine - this project's own public API had not grown since the SEVENTH
# run back, so the work this gate does had not grown either. The total
# test time across those nine identical-workload runs still ranged
# 51.26s-100.62s (mean ~67.4s) - variance close to 2x with ZERO change
# in what was being measured, the same executor-I/O intermittency
# BATCH-PARITY-WIN already named in check_public_name_collision.py's own
# header on 05/09/2026, not a regression proportional to this library's
# growth (the diagnostic this project's own precedent - a process spent
# per swept item - would call for instead, and does not apply here: this
# gate was already a single batched invocation before this finding).
# 100.62s already used 84% of the 120s default before the run that
# finally crossed it; 300s restores roughly the SAME headroom ratio the
# suite-wide default gives lighter tests, over the measured worst case,
# without guessing a number nobody ran.
#
# SECOND TIMEOUT override, 300s -> 900s (GODS_LAWS.md L-44/L-45, second
# CROSS-CHECK job-lead request, 08/09/2026, CI run 34271655070 job
# "Windows - compartilhado", ***Timeout 300.06 sec). Before touching the
# number again, four of the most recent runs (34248155435, 34237420599,
# 34228668830, 34271655070) were pulled (`gh run view --job <id> --log`,
# never estimated) across BOTH Windows jobs, not just the one that had
# been measured above - the 06/09/2026 finding only sampled "Windows -
# estatico" ("that same job", see above), so the suspicion that the 300s
# ceiling had never been exercised against "Windows - compartilhado" was
# correct, and the gap was real, not assumed:
#
#   Windows - compartilhado: 61.39s, 271.18s, 59.69s, ***Timeout 300.06s
#   Windows - estatico:      85.57s, 71.13s,  187.79s, 59.51s
#
# The measured worst case (uncensored - the 300.06s timeout is a killed
# process, not a true completion time, and is excluded from this number)
# climbed from 100.62s (06/09/2026, "estatico" only) to 271.18s
# ("compartilhado", run 34237420599) - both Windows jobs got slower, and
# "compartilhado" crossed the 300s ceiling outright. This is the SAME
# executor-I/O intermittency already named above (BATCH-PARITY-WIN) and
# not a hang: the same job/run (34271655070) shows OTHER lightweight
# consume tests that normally finish in well under a second stretching to
# tens of seconds in that one run alone (dep_zero_selftest 25.39s,
# rslt_precondition_test 8.86s, pkgconfig_validate_test 63.43s - all
# PASSED, none timed out) - a single slow run hitting many unrelated
# tests is the signature of a contended shared runner, not this one
# script waiting on something that never arrives. A dedicated sweep of
# every `LABELS consume` test's measured time across these same 8 job
# logs (43 of 53 apply to a Windows job; the other 10 are Unix-only or
# Linux-side twins, e.g. `visibility_test`/`pkgconfig_test`) found
# exactly ONE test within 20% of its own ceiling: this one - no other
# consume test needed its timeout touched.
#
# 900s keeps roughly the SAME ~3x headroom ratio over the new measured
# worst case (271.18s) that 300s kept over the old one (100.62s ->
# 300s is ~2.98x); it is not sized against the 300.06s timeout itself,
# since that number is censored (the process was killed mid-run, not
# measured to completion).
set_tests_properties(public_name_collision_test PROPERTIES TIMEOUT 900)

add_test(
    NAME public_name_collision_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_public_name_collision.py" --selftest
        "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_COMPILER_ID}"
)
set_tests_properties(public_name_collision_selftest PROPERTIES LABELS selftest)

# Visibility test: only makes sense with dynamic symbols (.so) and the
# Unix `nm -D` tool. Windows now has its OWN parity script instead of
# being left unproven: EXPORTS-PARITY-WIN (05/09/2026) added
# tools/ci/check-exports-win.ps1 (dumpbin /exports against the real
# glintfx.dll), registered in the `windows` CI job next to
# check-dep-zero-win.ps1 (DEPZERO-PARITY-WIN) and
# check-port-privacy-win.ps1 (PORT-PRIVACY-WIN) - the same
# different-mechanism/same-coverage split GODS_LAWS.md L-04 asks for.
#
# MEASURED, not assumed (the reasoning lives in that script's own
# header): the leak this test guards against - two libstdc++ internals
# escaping the shared object, ABI-STDLIB-LEAK - CANNOT recur on Windows
# through the same causal chain. A PE export table is OPT-IN: nothing
# leaves the binary without an explicit __declspec(dllexport) or a .def
# entry, the exact inverse of ELF, whose export-everything default is
# what the original leak exploited (libstdc++'s own headers stamp
# visibility("default") on std::basic_string, and an explicit attribute
# beats the -fvisibility=hidden preset). Microsoft's STL headers stamp
# no such thing, and `grep -rn '__declspec(dll' src/ include/` finds
# NOTHING in this tree - GLINTFX_API, via generate_export_header(), is
# the only source of dllexport here. The Windows script exists anyway,
# and reproves any future regression against the real binary, because
# "impossible by construction" is a claim that has to keep being
# measured (GODS_LAWS.md L-36).
if(BUILD_SHARED_LIBS AND UNIX)
    add_test(
        NAME visibility_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_exports.sh" "$<TARGET_FILE:glintfx_library>"
    )
    set_tests_properties(visibility_test PROPERTIES LABELS consume)

    # PARITY-GATE: check_exports.sh nunca tinha --selftest proprio,
    # enquanto o irmao Windows (tools/ci/check-exports-win.ps1) ja
    # tinha - o achado exato que o gate de paridade (tests/tools/
    # check_test_parity.py) pegou e nao pode excetuar (EXPORTS-PARITY-
    # WIN, o item que cobriria, esta Concluido). Os quatro controles
    # agora vivem em check_exports.sh (--selftest), espelhando
    # Invoke-SelfTest do lado Windows - ver tests/parity_aliases.txt.
    add_test(
        NAME visibility_selftest
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_exports.sh" --selftest
    )
    set_tests_properties(visibility_selftest PROPERTIES LABELS selftest)
endif()

# LIB-NO-UNDEF gate (GODS_LAWS.md L-36/L-40, TODO.md): the OPPOSITE
# direction of visibility_test above - read tests/tools/check_no_undef_
# glintfx.py's own header for the real defect this closes (commit
# 8eb7d73: compound_match.cpp called four structural_* functions plus
# attribute_selector_holds() whose .cpp files had been added to the
# TEST targets only, never to glintfx_library's own target_sources() -
# the .so linked and shipped anyway, silently, with five glintfx::
# symbols left dynamically undefined; the 88-case suite stayed green
# because it recompiles the sources a second time inside each test
# binary instead of linking against the shipped library). visibility_
# test scans DEFINED dynamic symbols and reproves anything OUTSIDE
# glintfx:: (too much leaves the library); this gate scans UNDEFINED
# dynamic symbols and reproves anything INSIDE glintfx:: (not enough
# entered it) - neither subsumes the other.
#
# `nm` (both --shared's `-D` and --static's plain form) is the same
# POSIX/GNU-binutils tool check_exports.sh already requires, so this
# gate is if(UNIX)-guarded like port_privacy_test above, picking
# --shared or --static from the SAME BUILD_SHARED_LIBS variable that
# decides which artifact this configured tree even produces. --static
# never reproves by symbol content (declared and counted in the
# script's own output every run, GODS_LAWS.md L-40 - a .a has no link
# step of its own, so cross-object "undefined" is the expected shape,
# not a defect); the real link-time guarantee for a static consumer is
# out of this repository's reach. Windows counterpart: tools/ci/check-
# no-undef-glintfx-win.ps1 (registered in the `windows` CI job) - the
# defect is impossible there BY CONSTRUCTION (MSVC's link.exe refuses
# to produce a DLL with any unresolved internal reference, LNK2019/
# LNK1120, unlike GNU ld's default for a .so), so that script instead
# verifies the specific historical premise (both .cpp files actually
# compiled inside glintfx_library's own target, not just a test
# target) - see its own header for the full argument.
if(UNIX)
    if(BUILD_SHARED_LIBS)
        set(GLINTFX_LIB_NO_UNDEF_MODE "--shared")
    else()
        set(GLINTFX_LIB_NO_UNDEF_MODE "--static")
    endif()
    add_test(
        NAME lib_no_undef_test
        COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_undef_glintfx.py"
            "${GLINTFX_LIB_NO_UNDEF_MODE}" "$<TARGET_FILE:glintfx_library>"
    )
    set_tests_properties(lib_no_undef_test PROPERTIES LABELS consume)

    add_test(
        NAME lib_no_undef_selftest
        COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_undef_glintfx.py" --selftest
    )
    set_tests_properties(lib_no_undef_selftest PROPERTIES LABELS selftest)
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

# FACADE-PIN, G2 (docs/plano-conserto-fachadas-uaf.md sec. 9, GODS_
# LAWS.md L-19/L-36): the portao G1 (the pinned_adapter<A> concept
# every port now requires) structurally cannot reach - a class that
# registers `this` with the OS but is never selected through a port at
# all (wayland_shell_adapter, wayland_seat_adapter). Unguarded, same
# GATE-TREE-PARITY reasoning layers_test above already gives: this
# scan reads text, never builds anything platform-specific.
add_test(
    NAME self_registration_pinned_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_self_registration_pinned.py"
        "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(self_registration_pinned_test PROPERTIES LABELS consume)

add_test(
    NAME self_registration_pinned_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_self_registration_pinned.py" --selftest
)
set_tests_properties(self_registration_pinned_selftest PROPERTIES LABELS selftest)

# C-OPT id gate (docs/plano-w6b-placa-e-laco.md fatia 2a, GODS_LAWS.md
# L-26/L-40): gltfx_gfx_option's own append-only numeric ids (include/
# glintfx/platform/gl/gfx_option.hpp), cross-checked against the
# registry table's own row order and "dado" name strings (src/platform/
# gl/gfx_option_registry.hpp) - see that script's own header for the
# full four-item contract. Registered unguarded, same shape as
# layers_test/layers_selftest just above (this gate is pure text
# analysis, no OS-specific behavior to diverge on).
add_test(
    NAME gfx_option_ids_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_gfx_option_ids.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(gfx_option_ids_test PROPERTIES LABELS consume)

add_test(
    NAME gfx_option_ids_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_gfx_option_ids.py" --selftest
)
set_tests_properties(gfx_option_ids_selftest PROPERTIES LABELS selftest)

# L-05/L-06 gate (GODS_LAWS.md, "Gate L-05/L-06" job of
# .github/workflows/ci.yml): proves the repository has no forbidden
# term (see tests/tools/check_no_x11.py's own header for the L-40
# findings this gate exists to close). Wiring it here too, not only in
# the standalone CI job, gives a local `ctest -R x11` and folds it into
# the same consume/selftest label taxonomy as the other gates in this
# file.
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"): PORTED from POSIX sh
# (tests/tools/check_no_x11.sh, if(UNIX)-guarded, never exercised on
# Windows even though the check itself is plain-text scanning with no
# Unix-specific mechanism behind it) to python3 and registered here
# WITHOUT a system guard - same shape layers_test/spdx_test above. The
# old tests/tools/check_no_x11.sh is deleted in the same fatia.
add_test(
    NAME x11_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_x11.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(x11_test PROPERTIES LABELS consume)

add_test(
    NAME x11_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_no_x11.py" --selftest
)
# FAIL_REGULAR_EXPRESSION, the SAME belt dep_zero_trace_selftest/spdx_
# selftest wear (GODS_LAWS.md L-40): check_no_x11.py's own aggregator
# (a plain Python `all(controls)` over each control's own boolean
# return) does not share the POSIX-sh shared-global-variable collision
# the sh sibling's own DEPZERO-SELFTEST-FIX had to close, but the belt
# costs nothing and keeps the taxonomy uniform across every selftest in
# this file, sh or python3 alike.
set_tests_properties(x11_selftest PROPERTIES
    LABELS selftest
    FAIL_REGULAR_EXPRESSION "FALHOU")

# GODS_LAWS.md L-17/L-22 gate (o gemeo do gemeo, TDD para o achado do
# time-lead nesta fatia, LASTERROR-CLEAR): prova que todo Win32/WGL
# call em src/platform/win32/ cujo fracasso este projeto atribui via
# ::GetLastError() tem o thread-local LIMPO (::SetLastError(0);)
# imediatamente antes - ver tests/tools/check_win32_last_error_
# cleared.py's own header comment para o incidente-fonte (os_error_
# code=203, ERROR_ENVVAR_NOT_FOUND, relatado por um SwapBuffers() que
# nada tem a ver com variavel de ambiente, run 34171429194 "Windows -
# estatico"). Portavel de proposito (varredura de texto puro, GATE-
# TREE-PARITY - mesma forma x11_test acima), roda em qualquer SO sem
# precisar de toolchain Windows, e reprova ANTES do conserto desta
# fatia (RED provado localmente antes do commit) - a unica prova que
# a superficie inteira, nao so o sitio que o CI mediu, foi consertada.
add_test(
    NAME win32_last_error_cleared_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_win32_last_error_cleared.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(win32_last_error_cleared_test PROPERTIES LABELS consume)

add_test(
    NAME win32_last_error_cleared_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_win32_last_error_cleared.py" --selftest
)
set_tests_properties(win32_last_error_cleared_selftest PROPERTIES
    LABELS selftest
    FAIL_REGULAR_EXPRESSION "FALHOU")

# GL-GPU-KIND driver-name table gate (docs/plano-w6b-fatias-5b-
# revisao.md sec. 3/4.3, D-W6b-30/38, GODS_LAWS.md L-40): proves gpu.
# hpp's own GLINTFX-DRM-DRIVER-TABLE marked line and drm_gpu_kind.cpp's
# own if-chain name the SAME set of DRM driver names. Registered
# unguarded from birth (check_drm_driver_table.py's own header - the
# GATE-TREE-PARITY lesson x11_test/x11_selftest already paid for once,
# not repeated here).
add_test(
    NAME drm_driver_table_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_drm_driver_table.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(drm_driver_table_test PROPERTIES LABELS consume)

add_test(
    NAME drm_driver_table_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_drm_driver_table.py" --selftest
)
set_tests_properties(drm_driver_table_selftest PROPERTIES
    LABELS selftest
    FAIL_REGULAR_EXPRESSION "FALHOU")

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

# PARITY-GATE: sub-check (c) given a NAME OF ITS OWN, unconditional
# (reuses GLINTFX_DEP_ZERO_LIB_ARG computed above, so it sees the same
# real .so / "NONE" / "WINDOWS-SEPARATE" the bundled dep_zero_test
# call already does) - the achado this fatia closes: check-dep-zero-
# win.ps1's own dep_zero_binary_win_test had no Linux-side ctest name
# to pair with in tests/parity_aliases.txt, because sub-check (c) used
# to live only INSIDE dep_zero_test's bundled a+b+c verdict. On
# Windows this prints the "declared not applicable, see check-dep-
# zero-win.ps1" message and passes - the real binary check on Windows
# runs there, not here, by design (GODS_LAWS.md L-04, mechanism
# differs, coverage does not).
add_test(
    NAME dep_zero_binary_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero.py"
        --binary-only "${GLINTFX_DEP_ZERO_LIB_ARG}"
)
set_tests_properties(dep_zero_binary_test PROPERTIES LABELS consume)

add_test(
    NAME dep_zero_binary_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dep_zero.py" --selftest-binary-only
)
set_tests_properties(dep_zero_binary_selftest PROPERTIES
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
# tests/tools/check_dup_laws.py's own header for the two motives of
# duplication - decisao do lider vs. duvida do orquestrador).
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"): PORTED from POSIX sh
# (tests/tools/check_dup_laws.sh, if(UNIX)-guarded, never exercised on
# Windows even though the check itself is plain-text comparison with no
# Unix-specific mechanism behind it) to python3 and registered here
# WITHOUT a system guard - same shape layers_test/x11_test above. The
# old tests/tools/check_dup_laws.sh is deleted in the same fatia.
add_test(
    NAME dup_laws_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dup_laws.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(dup_laws_test PROPERTIES LABELS consume)

add_test(
    NAME dup_laws_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dup_laws.py" --selftest
)
set_tests_properties(dup_laws_selftest PROPERTIES LABELS selftest)

# Cobertura de pacote de sistema (achado do lider, GHA run 34056358894,
# commit 40c2622, 06/09/2026: catorze jobs Linux vermelhos porque
# nenhum dos cinco alvos instalava o pacote que fornece egl.pc depois
# que cmake/GlintfxEgl.cmake passou a exigi-lo via pkg_check_modules -
# GODS_LAWS.md L-44, "portao que nunca rodou no ambiente real nao e
# portao"). Deriva os dois lados ao vivo (modulos REQUIRED de
# cmake/*.cmake, jobs que configuram a arvore principal em
# .github/workflows/ci.yml) e cruza contra uma tabela modulo->pacote
# por familia de distro MEDIDA (ver o cabecalho de
# check_pkg_dep_coverage.py para a fonte de cada linha) - nunca
# hardcoded contra a lista de pacotes de hoje.
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04): scanner de texto puro sobre
# arquivos versionados, sem executar nada especifico de plataforma -
# registrado sem guarda de sistema, mesma forma de x11_test/
# dup_laws_test acima.
add_test(
    NAME pkg_dep_coverage_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkg_dep_coverage.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(pkg_dep_coverage_test PROPERTIES LABELS consume)

add_test(
    NAME pkg_dep_coverage_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkg_dep_coverage.py" --selftest
)
set_tests_properties(pkg_dep_coverage_selftest PROPERTIES LABELS selftest)

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
# above (pkg-config's own convention is Unix/Linux packaging). RAZAO
# CORRIGIDA em 06/09/2026 (docs/auditoria-decisoes-autonomas.md F3):
# this comment used to say "there is no Windows-specific coverage here
# because glintfx has no platform layer that needs Libs.private there
# yet" - false since 03/09/2026, when the win32 platform layer was
# born (cmake/GlintfxInstall.cmake's own glintfx_compute_pkgconfig_
# libs_private() now DOES compute Libs.private for it too). The real,
# remaining reason this gate stays if(UNIX): check_pkgconfig.sh itself
# hardcodes Unix artifact naming (libglintfx.so*/libglintfx.a globs) and
# DESTDIR semantics - porting it to also recognize glintfx.dll/glintfx.
# lib and Windows path conventions is its own fatia (tracked in TODO.md's
# own table, not attempted blind here without a real Windows machine to
# verify against). tests/parity_exceptions.txt's own entry for this test
# carries the same correction, with the fuller reasoning.
if(UNIX)
    add_test(
        NAME pkgconfig_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkgconfig.sh"
            "${PROJECT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/raw_link/main.cpp"
            "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(pkgconfig_test PROPERTIES LABELS consume)

    # TIMEOUT override, MEASURED not guessed (GODS_LAWS.md L-11/L-44,
    # same precedent as public_name_collision_test's own TIMEOUT 300
    # above): this test had been inheriting the SUITE-WIDE 120s default
    # (cmake/GlintfxTest.cmake's own glintfx_set_default_test_timeout())
    # until it timed out for real on "CachyOS - estatico" (run
    # 34237420599, ***Timeout 120.01 sec). PKG-WIN-INTEROP's own
    # find_program() fix (09fb92e) does not touch this test's own
    # execution path at all - the changed code is entirely inside
    # cmake/GlintfxPkgConfigValidateInstalled.cmake.in's WIN32 branch,
    # this test's own check_pkgconfig.sh never sets -DWIN32 - so this is
    # not a regression from that fatia, only a teto that was already
    # close to this test's real measured time on every Linux platform.
    #
    # Every real completion across the two most recent runs (never the
    # truncated 120.01s the timeout itself printed, which is not a
    # completion time): run 34228668830 - CachyOS estatico 115.11s,
    # CachyOS compartilhado 114.48s, Ubuntu compartilhado 114.00s, Arch
    # compartilhado 112.27s, Arch estatico 110.92s, Fedora (primario)
    # estatico 110.28s, Ubuntu estatico 108.74s, Fedora (primario)
    # compartilhado 96.01s; run 34237420599 - Arch compartilhado
    # 113.66s, Ubuntu compartilhado 107.20s, Fedora (primario) estatico
    # 106.33s, Fedora (primario) compartilhado 99.98s, CachyOS
    # compartilhado 90.89s (gh run view --job <id> --log, never
    # estimated). GODS_LAWS.md L-17 ("isolado ou padrao?"): the same
    # sweep across BOTH runs' full ctest output found this same test,
    # not CachyOS alone, already inside 20% of the 120s ceiling
    # (>=96s) on EVERY Linux platform in the matrix (Fedora, Ubuntu,
    # Arch, CachyOS) in at least one of shared/static mode - CachyOS
    # estatico was simply the first to actually cross it. No OTHER test
    # in the suite (across the same two runs' full output, Linux and
    # Windows jobs alike) reached that 96s floor except
    # public_name_collision_test, which already carries its own TIMEOUT
    # override above.
    #
    # 300s mirrors public_name_collision_test's own precedent exactly
    # (worst measured 115.11s here vs 100.62s there; ~2.6x headroom over
    # this test's own worst measured value here, the same order of
    # magnitude as that precedent's ~3x) - never widening the SUITE-WIDE
    # 120s default itself, only this one test's own budget, sized to its
    # own measured worst case.
    set_tests_properties(pkgconfig_test PROPERTIES TIMEOUT 300)
endif()

# Post-install pkg-config VALIDATOR gate (PKG-VALIDATE): proves the
# install(CODE) step in cmake/GlintfxPkgConfigValidate.cmake /
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in - the piece that
# runs on a PACKAGER's own machine, at `cmake --install` time, not
# only in this project's own CI - actually fires, actually resolves
# the real (including DESTDIR-staged) install location, actually
# honors both halves of its escape hatch, and fails CLOSED with a
# self-sufficient diagnostic on a real broken library artifact, a
# real missing glintfx.pc, a real missing header tree, and a
# hand-assembled empty-Cflags/Libs fixture (GODS_LAWS.md L-40), while
# degrading to a WARNING (not a FATAL_ERROR) when pkg-config itself is
# absent from PATH - see check_pkgconfig_validate.py's own header for
# the SEVENTEEN scenarios it actually exercises. The "eight" this
# comment used to claim was stale prose, never updated after
# PKG-WIN-SCOPE/PKG-NATIVE grew the file; the count now lives in the
# script's own header, next to the list it is derived from. This is a
# DIFFERENT claim than pkgconfig_test above: that gate proves
# glintfx.pc's CONTENT is correct across six layouts OUR OWN CI
# enumerates; this one proves the VALIDATOR ITSELF, which is designed
# to reach layouts neither pkgconfig_test nor any of this project's CI
# jobs ever try (GODS_LAWS.md LEI ZERO: the base of consumers is
# public and unknown).
#
# ENSINANDO O VALIDADOR (GODS_LAWS.md L-04, decisao do lider,
# 04/09/2026). O historico importa aqui porque ha uma decisao MORTA no
# caminho: em 27/08 o CTO decidiu, em modo autonomo, tornar o glintfx.pc
# um artefato so de Unix; o LIDER REVERTEU no mesmo dia, verbatim - "O
# .pc volta a ser instalado no Windows, e decidimos juntos o que fazer
# com o validador que reprova instalacao boa". Levada a ele em
# 04/09/2026, a decisao foi ENSINAR o verificador a reconhecer o nome do
# artefato em cada sistema. Nao um segundo validador so para Windows,
# nao deixar o Windows sem validacao: um validador so, ensinado.
#
# PORTED from POSIX sh (tests/tools/check_pkgconfig_validate.sh,
# if(UNIX)-guarded since 27/08/2026, never exercised on the Windows CI
# job at all) to python3 and registered here WITHOUT a system guard -
# same GATE-TREE-PARITY shape as blank_install_dir_rejected_test and
# layers_test above. The port's own closed LIBRARY_ARTIFACT_PATTERNS
# list (three names - libglintfx.so*, libglintfx.a, glintfx.lib -
# duplicated from cmake/GlintfxPkgConfigValidateInstalled.cmake.in's own
# glintfx_pkgconfig_library_glob_patterns(), which globs all three with
# NO platform filter) is what actually closes the Windows gap: the
# retired .sh's own artifact-removal step only ever searched for the two
# Unix names, so on a real Windows install it would have found NOTHING
# to delete and the broken-library control would have passed without
# ever biting. The port reproves outright when that removal count comes
# up zero (GODS_LAWS.md L-40), instead of trusting a later assertion to
# catch it by accident. The old .sh is deleted in the same fatia.
add_test(
    NAME pkgconfig_validate_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkgconfig_validate.py"
        "${PROJECT_SOURCE_DIR}"
        "${CMAKE_CXX_COMPILER}"
)
set_tests_properties(pkgconfig_validate_test PROPERTIES LABELS consume)

# --selftest: pure logic controls (normalize_wrapped_message's own
# wrap-immunity across a range of widths - the fix that also closes the
# unconserted line 482 the ASSERT-WRAP item names, since normalization
# is now applied to EVERY cmake-output comparison in the file instead of
# being chosen point by point - the closed LIBRARY_ARTIFACT_PATTERNS
# enumeration, find/remove_library_artifacts, and rewrite_libdir_line).
# No cmake configure, no compiler, no real install needed. Same shape
# blank_install_dir_rejected_selftest already established.
add_test(
    NAME pkgconfig_validate_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_pkgconfig_validate.py" --selftest
)
set_tests_properties(pkgconfig_validate_selftest PROPERTIES LABELS selftest)

# Blank install-dir gate (adversarial review round 3, PKG-DIST task 3):
# proves CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR are refused at
# CONFIGURE time when empty or whitespace-only
# (glintfx_require_nonblank_install_subdir, cmake/GlintfxInstall.cmake) -
# blank is never a legitimate layout (it collapses some install()
# destinations to the filesystem root, confirmed live before the
# guard existed), unlike a malformed-but-nonblank value, which still
# names a real layout somewhere and is handled by the pkg-config gate
# above instead of refused.
#
# INSTALL-PARITY-WIN (GODS_LAWS.md L-04, decisao do lider: "O
# comportamento deve ser igual em qualquer OS"): PORTED from POSIX sh
# (tests/tools/check_blank_install_dir_rejected.sh, if(UNIX)-guarded,
# never exercised on Windows) to python3 and registered here WITHOUT a
# system guard - same GATE-TREE-PARITY shape as public_name_collision_
# test/layers_test above. The old .sh is deleted in the same fatia.
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
add_test(
    NAME blank_install_dir_rejected_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_blank_install_dir_rejected.py"
        "${PROJECT_SOURCE_DIR}"
        "${CMAKE_CXX_COMPILER}"
)
set_tests_properties(blank_install_dir_rejected_test PROPERTIES LABELS consume)

# --selftest: pure text-processing controls (classify_configure_result,
# normalize_wrapped_message) - no nested cmake configure, no compiler
# needed. Closes the same "own gate never proved against itself" gap
# layers_selftest/public_name_collision_selftest already closed for
# their siblings (GODS_LAWS.md L-40).
add_test(
    NAME blank_install_dir_rejected_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_blank_install_dir_rejected.py" --selftest
)
set_tests_properties(blank_install_dir_rejected_selftest PROPERTIES LABELS selftest)

# Mixed absolute/relative libdir/includedir gate (PKG-LIBDIR-MIX, achado
# colateral de 27/08/2026, pre-existente): proves CMAKE_INSTALL_LIBDIR
# and CMAKE_INSTALL_INCLUDEDIR are refused at CONFIGURE time when one is
# ABSOLUTE and the other is RELATIVE
# (glintfx_require_consistent_libdir_includedir_kind, cmake/GlintfxInstall.cmake) -
# that mix bakes glintfx.pc's `prefix=` from CMAKE_INSTALL_LIBDIR's
# absoluteness alone, so an install-time --prefix override moves only
# the relative half and the .pc ends up pointing at a directory that is
# not there. The two SAME-kind combinations (both absolute, both
# relative - the common, GNUInstallDirs-default case) are proven to
# stay unaffected in the same run. Same GATE-TREE-PARITY shape as
# blank_install_dir_rejected_test above: unguarded by platform
# (GODS_LAWS.md L-04), resolved via GLINTFX_PYTHON3_EXECUTABLE.
add_test(
    NAME libdir_includedir_mix_rejected_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_libdir_includedir_mix_rejected.py"
        "${PROJECT_SOURCE_DIR}"
        "${CMAKE_CXX_COMPILER}"
)
set_tests_properties(libdir_includedir_mix_rejected_test PROPERTIES LABELS consume)

# --selftest: pure text-processing controls (classify_configure_result)
# - no nested cmake configure, no compiler needed. Same
# "own gate never proved against itself" closure as
# blank_install_dir_rejected_selftest above (GODS_LAWS.md L-40).
add_test(
    NAME libdir_includedir_mix_rejected_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_libdir_includedir_mix_rejected.py" --selftest
)
set_tests_properties(libdir_includedir_mix_rejected_selftest PROPERTIES LABELS selftest)

# Macro-balance gate (TODO.md HDR-MACRO-BALANCE, GODS_LAWS.md L-40):
# proves every "#define" a public header under PROJECT_SOURCE_DIR/include
# introduces is "#undef"'d in the SAME file, closing the exact gap the
# 27/08/2026 adversarial review found by experiment (removing
# include/glintfx/gfss/token.hpp's own outer #undef and watching the
# full -Werror build and suite pass 47/47 anyway - see
# check_macro_balance.py's own header comment for the two designs
# measured before choosing a pure text scan over a compiler-based
# `-dM` diff, and why the compiler-based one hits a hard MSVC-parity
# wall check_public_name_collision.py's own NAMES-PARITY-WIN research
# already documented). Pure Python, zero subprocess spawn (GODS_LAWS.md
# L-11), unguarded by platform (GATE-TREE-PARITY, GODS_LAWS.md L-04) -
# text scanning has no per-OS mechanism to diverge.
add_test(
    NAME macro_balance_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_macro_balance.py"
        "${PROJECT_SOURCE_DIR}/include"
)
set_tests_properties(macro_balance_test PROPERTIES LABELS consume)

add_test(
    NAME macro_balance_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_macro_balance.py" --selftest
)
set_tests_properties(macro_balance_selftest PROPERTIES LABELS selftest)

# Environment-fact sweep gate (TODO.md GATE-ENV-SWEEP, GODS_LAWS.md
# L-40): closed enumeration of tests/tools/, tests/container/, cmake/,
# tools/ci/ and .github/workflows/ci.yml, requiring every use of a
# closed, named vocabulary of machine-fact primitives (tool
# availability, core count, JSON list order, null-vs-empty-array,
# CRLF line endings, "compiler available" conflated with "concept
# exists on this platform") to carry a nearby declaration comment -
# see check_env_sweep.py's own header comment for the real incidents
# (`_KNOWN_INCIDENTS` in that file - count not fixed here, GODS_LAWS.md
# CLAUDE.md "DOC-ESTADO") this gate is calibrated against, and for why
# the signal vocabulary is keyed on
# named remedy functions rather than generic primitives (measured live:
# the generic form fired on eleven UNRELATED, safe uses across the
# real tree - GODS_LAWS.md L-40's own warning against a heuristic that
# produces noise nobody reads). Pure Python, zero subprocess spawn,
# unguarded by platform - same GATE-TREE-PARITY shape as
# macro_balance_test above.
add_test(
    NAME env_sweep_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_env_sweep.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(env_sweep_test PROPERTIES LABELS consume)

# The --selftest here also runs calibrate_against_known_incidents()
# against the REAL project tree (never a fixture) - see
# check_env_sweep.py's own selftest_calibration_control() - so this
# case is deliberately NOT hermetic like the other *_selftest cases in
# this file; it depends on PROJECT_SOURCE_DIR still containing the
# historical markers `_KNOWN_INCIDENTS` calibrates against.
add_test(
    NAME env_sweep_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_env_sweep.py" --selftest
)
set_tests_properties(env_sweep_selftest PROPERTIES LABELS selftest)

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

# README volatile-numbers gate (decisao do CTO, mesma fatia do conserto
# de link WIN-WINDOW/X-2): a narrativa de paridade dos dois paragrafos
# acima ja apodreceu TRES vezes em tres commits - uma vez pega pelo gate
# irmao que existia aqui (check_readme_test_count.py, a frase "has N
# registered cases..."), duas vezes NAO pegas por nada (o "Linux's 90",
# o "88", "ten renamed pairs" contra os 19 apelidos reais em tests/
# parity_aliases.txt, "forty controls" contra os 38 que check_dep_
# zero.py realmente declara, uma mensagem de programa citada em ingles
# que ele nunca imprimiu, e por fim o proprio par de numeros "116/114" e
# "114/114" - nenhuma sessao deste projeto tem toolchain MSVC a mao, e o
# numero do Windows sempre foi deduzido, nunca medido). O CTO recusou um
# parser mais esperto da prosa (precisaria de uma ancora por numero, e
# nove ancoras tornam a prosa refem do portao - precedente citado: o
# version-sync do ecossistema Rust, que confere token de forma fixa,
# nunca prosa livre). A saida escolhida, levada ate o fim em 06/09/2026:
# a narrativa para de citar QUALQUER numero, o gate irmao acima e
# APOSENTADO (GODS_LAWS.md L-67: revogado e apagado, nao arquivado -
# ver o historico git deste arquivo para o texto antigo), e este portao
# reprova qualquer digito que volte a aparecer na narrativa, fora das
# duas isencoes que restam no proprio check_readme_volatile_numbers.py
# (spans entre crases, e citacoes L-NN). Registrado SEM guarda de
# plataforma (GODS_LAWS.md L-04): este portao le texto, nao constroi
# nada, nao ha razao para rodar em so um SO.
add_test(
    NAME readme_volatile_numbers_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_volatile_numbers.py"
        "${PROJECT_SOURCE_DIR}/README.md"
)
set_tests_properties(readme_volatile_numbers_test PROPERTIES LABELS consume)

add_test(
    NAME readme_volatile_numbers_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_readme_volatile_numbers.py" --selftest
)
set_tests_properties(readme_volatile_numbers_selftest PROPERTIES LABELS selftest)

# CLAUDE.md "Estado atual do repositorio" volatile-number gate (item
# novo do lider, 06/09/2026 - docs/auditoria-decisoes-autonomas.md F2,
# decisao de 25/08 de nao criar portao mecanico para isto, que ja
# reincidiu quatro vezes no README antes de check_readme_volatile_
# numbers.py acima existir). Escopo deliberadamente estreito - so' a
# secao que a propria CLAUDE.md declara "nao carrega contador" (ver
# check_claude_md_state_numbers.py's own header para a razao completa
# de nao escanear o documento inteiro). Registrado SEM guarda de
# plataforma (GODS_LAWS.md L-04): le texto, constroi nada.
add_test(
    NAME claude_md_state_numbers_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_claude_md_state_numbers.py"
        "${PROJECT_SOURCE_DIR}/CLAUDE.md"
)
set_tests_properties(claude_md_state_numbers_test PROPERTIES LABELS consume)

add_test(
    NAME claude_md_state_numbers_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_claude_md_state_numbers.py" --selftest
)
set_tests_properties(claude_md_state_numbers_selftest PROPERTIES LABELS selftest)

# Pre-commit hook chain gate (TODO.md INBOX drenagem 06/09/2026,
# GODS_LAWS.md L-36/L-40): tools/git-hooks/pre-commit is a plain text
# file this repository's own tree can read directly, so this gate reads
# the VERSIONED source and reproves the moment check_dep_zero.py's own
# `--staged` invocation or check_spdx.py's own invocation stops
# appearing in it - the exact regression that let seven files land
# without an SPDX header while nothing local ever noticed (the hook had
# never chained the license gate at all, on top of never having been
# installed in that clone - install.sh's own header explains why THAT
# half can never be checked from inside the tree). Registered WITHOUT a
# platform guard (GODS_LAWS.md L-04): this portao reads text, builds
# nothing, no reason to run on only one OS.
add_test(
    NAME precommit_hook_chain_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_precommit_hook_chain.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(precommit_hook_chain_test PROPERTIES LABELS consume)

add_test(
    NAME precommit_hook_chain_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_precommit_hook_chain.py" --selftest
)
set_tests_properties(precommit_hook_chain_selftest PROPERTIES LABELS selftest)

# Long-dash-in-public-documentation gate (TODO.md item DASH-PUBDOC,
# docs/decisoes-inbox-tres.md SS3, GODS_LAWS.md L-32): the global hook
# that enforces "nunca em-dash em texto user-facing"
# (~/.claude/hooks/no_mdash.py / ~/.claude/githooks/no_mdash_staged.py)
# classifies "public" by name/folder and never runs on the server nor
# travels with a clone - it does not know this project's own L-21
# ("documento se decide pelo LEITOR") and does not match `docs/*.md`.
# SAME SHAPE as spdx_test/precommit_hook_chain_test immediately above:
# registered UNGUARDED (this portao reads text, builds nothing, no
# reason to run on only one OS - GODS_LAWS.md L-04), so it runs inside
# the SAME ctest invocation the five platforms already execute, and is
# exercised by tools/preci.sh's own ctest stage with no dedicated
# preci.sh stanza (same as spdx_test/vendor_purity_test/readme_
# volatile_numbers_test, none of which get one either).
add_test(
    NAME dash_pubdoc_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dash_pubdoc.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(dash_pubdoc_test PROPERTIES LABELS consume)

add_test(
    NAME dash_pubdoc_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dash_pubdoc.py" --selftest
)
set_tests_properties(dash_pubdoc_selftest PROPERTIES LABELS selftest)

# Rounding-order gate (TODO.md GATE-LLROUND-ORDER, GODS_LAWS.md L-20/
# L-36/L-40): closed static check that every llround/lround/round/
# nearbyint/rint call under src/ and include/ is either guarded by a
# std::clamp(...) nested in its OWN argument expression (src/core/
# color.cpp, src/gfss/color_parse.cpp) or preceded, in the SAME
# function body, by a comparison of its OWN argument against BOTH
# std::numeric_limits<...>::max()/::min() (src/core/time.cpp) - never
# satisfied by ANY comparison merely being present (see check_round_
# order.py's own header comment for the loophole this closes and the
# two negative controls in --selftest that prove it: wrong variable,
# and untied to numeric_limits). Reproduces the 28/08/2026 finding this
# item is named after (moving the rounding call to BEFORE the range
# comparisons left the WHOLE suite green, ASan/UBSan included, because
# the defect lives inside glibc's own uninstrumented std::llround()) -
# --selftest's own calibration control requires the check to still
# pass against this project's REAL tree. Pure Python, zero subprocess
# spawn, unguarded by platform - same GATE-TREE-PARITY shape as
# macro_balance_test/env_sweep_test above.
add_test(
    NAME round_order_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_round_order.py"
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(round_order_test PROPERTIES LABELS consume)

add_test(
    NAME round_order_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_round_order.py" --selftest
)
set_tests_properties(round_order_selftest PROPERTIES LABELS selftest)

# PARITY-GATE (TODO.md, GODS_LAWS.md L-04/L-36/L-40): the mechanical
# gate this item exists to build - confronts the ctest inventory
# (`ctest -N`, GODS_LAWS.md L-45) two systems actually register,
# reproving any test name present on one side and absent on the other
# UNLESS tests/parity_exceptions.txt (a genuine, permanent-or-tracked
# absence) or tests/parity_aliases.txt (the same coverage under a
# different name) accounts for it. The rule this item is named after:
# an exception pointing at a TODO.md item already marked concluded
# reproves ("concluido sem par") - see check_test_parity.py's own
# header for the ten real names that trip exactly this today (their
# Windows-side proof runs as a raw PowerShell CI step, never
# registered as ctest, so this gate cannot see it - a real,
# unresolved finding, not a bug in this gate).
#
# Only --selftest is wired here: the REAL --compare invocation needs
# the Linux and Windows CI legs' own `ctest -N` inventories, which
# only exist as build artifacts inside a live CI run (see the `parity`
# job in .github/workflows/ci.yml) - a single local ctest run has only
# ITS OWN platform's inventory, never the other leg's, so it cannot
# self-compare. --selftest proves the comparison LOGIC itself (the
# three required red controls plus the alias/SEM-PENDENCIA edge
# cases) with synthetic fixtures, the same shape check_readme_test_
# count.py's own --selftest already uses for cross-platform sentences
# it cannot observe locally either. Unguarded by platform - same
# GATE-TREE-PARITY shape as every other python3 gate in this file.
add_test(
    NAME test_parity_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_test_parity.py" --selftest
)
set_tests_properties(test_parity_selftest PROPERTIES LABELS selftest)

# Container fixture wiring gate (P-0, docs/plano-w6a-janela.md fatia
# 1, D-W6a-19): closes the blind spot the P-0 implementer named in
# his own report - a fixture can gain a `COPY --from=...` line in
# tests/container/Containerfile and a `docker exec` step in the
# `wayland-container` job of .github/workflows/ci.yml without anyone
# remembering to also append `echo "<nome>" >> parity_inventory.txt`
# in that same step. The job still goes green (the fixture ran), the
# job's own piso-de-varredura step still goes green (the OTHER
# fixtures already make the file non-empty), and the forgotten name
# never reaches the `parity` job's comparison - it is never a
# reported gap, it simply never entered the conversation. See tests/
# tools/check_container_fixture_inventory.py's own header for the
# full shape of the failure and the two independent sources of ground
# truth it cross-checks (Containerfile COPY lines vs. ci.yml append
# lines, both directions).
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04): registered here WITHOUT a
# system guard, same shape as dup_laws_test/spdx_test/test_parity_
# selftest above - the script only ever reads two plain-text files
# already present in any checkout (tests/container/Containerfile and
# .github/workflows/ci.yml itself), nothing OS-specific behind it, so
# it runs identically on all five platforms. Because of that, the
# name `container_fixture_inventory_test` appears identically in the
# Linux and Windows `ctest -N` inventories - no entry is needed in
# tests/parity_exceptions.txt nor tests/parity_aliases.txt for it,
# the same reason test_parity_selftest immediately above needs
# neither.
add_test(
    NAME container_fixture_inventory_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_container_fixture_inventory.py"
        --compare
        "${CMAKE_SOURCE_DIR}/tests/container/Containerfile"
        "${CMAKE_SOURCE_DIR}/.github/workflows/ci.yml"
)
set_tests_properties(container_fixture_inventory_test PROPERTIES LABELS consume)

add_test(
    NAME container_fixture_inventory_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_container_fixture_inventory.py" --selftest
)
set_tests_properties(container_fixture_inventory_selftest PROPERTIES LABELS selftest)

# Container fixture INCLUDES gate (06/09/2026, GODS_LAWS.md L-04/L-17/
# L-36/L-40, conserto dos dois vermelhos da onda W6b): closes the blind
# spot the whole-tree rewrite of prepare_arch_ports_fixture.sh's own
# copy_source_tree() opens by construction - staging every .hpp/.cpp/.h
# under src/, include/ and tests/parity/ instead of a hand-named list of
# directories means a header can move to a directory nobody taught this
# gate about, and a bare `docker build` would only find out several
# fixtures in, with "No such file or directory". See tests/tools/
# check_container_fixture_includes.py's own header for the full shape.
#
# NO REAL-MODE ctest CASE HERE, unlike container_fixture_inventory_test
# right above (which only ever reads two plain-text files already
# present in any checkout): this gate's real mode needs a STAGED tree,
# which only prepare_arch_ports_fixture.sh's own `sh` script produces -
# that script already calls this gate as its own last step, and
# .github/workflows/ci.yml's own `wayland-container` job already runs
# that script before `docker build`. --selftest here is fully hermetic
# (its own tempfile fixtures, never the real staged tree), so it is
# registered on all five platforms with no guard, same GATE-TREE-PARITY
# shape as container_fixture_inventory_selftest above.
add_test(
    NAME container_fixture_includes_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_container_fixture_includes.py" --selftest
)
set_tests_properties(container_fixture_includes_selftest PROPERTIES LABELS selftest)

# Container fixture LINK gate (08/09/2026, GODS_LAWS.md L-11/L-17/L-36/
# L-40, quarta mordida de "lista de fontes mantida a mao" - server run
# 34192811273): sibling of container_fixture_includes_selftest above,
# closing the blind spot that gate cannot by construction - it proves
# every #include resolves, never that the resulting TUs LINK. A
# production atom (flush_retry_policy.cpp) started being called from a
# sibling TU (display_adapter.cpp) through a header BOTH already
# #include - no #include ever moved, so the includes gate stayed green
# while sixteen of this Containerfile's own g++ invocations died at
# real `ld` time with "undefined reference". See tests/tools/
# check_container_fixture_link.py's own header comment for the full
# account.
#
# NO REAL-MODE ctest CASE HERE, same reason container_fixture_includes_
# selftest right above has none: real mode RUNS the Containerfile's own
# g++/gcc invocations against a STAGED tree that only prepare_arch_
# ports_fixture.sh's own `sh` script produces - that script already
# calls this gate as the LAST step of its own main(), right after
# verify_fixture_includes(), and .github/workflows/ci.yml's own
# `wayland-container` job already runs that script before `docker
# build`. --selftest here is fully hermetic (its own tempfile fixtures,
# a synthetic two-file "atom skipped from the link" case that never
# touches Wayland/EGL, so it needs none of this host's dev packages -
# see that script's own selftest_missing_atom_reproves() for the exact
# shape), so it is registered on all five platforms with no guard, same
# GATE-TREE-PARITY shape as container_fixture_includes_selftest.
#
# CONSERTO 08/09/2026 (server run 34215308251, GODS_LAWS.md L-17/L-36/
# L-49): "hermetic" above was true for the Wayland/EGL dev packages,
# but the synthetic fixture still shelled out to a LITERAL "g++" -
# that's an environment fact this gate's own author congealed by
# accident, and it reproved on 8 of the matrix's 22 legs the moment
# g++ was not literally spelled that way (Ubuntu installs the
# versioned "g++-14" on purpose, no bare "g++" exists) or the temp
# path it built used backslashes (Windows). check_container_fixture_
# link.py's own discover_selftest_compiler() now finds a real GNU-
# compatible compiler instead of assuming the name - CMAKE_CXX_
# COMPILER/CMAKE_CXX_COMPILER_ID passed below are the FIRST source it
# tries (same "already proven to work by this very build" reasoning
# as the CMAKE_CXX_COMPILER/CMAKE_CXX_COMPILER_ID pair threaded to
# other python gates elsewhere in this file), with a PATH search as
# fallback and a DECLARED, counted skip (CTest's own SKIP_RETURN_CODE,
# never a silent pass) when neither source finds one - see that
# function's own comment for why CMAKE_CXX_COMPILER is rejected
# outright when its ID is MSVC (Windows CI's own CXX compiler for the
# PRODUCT build, incompatible with this fixture's GNU-flavored flags).
add_test(
    NAME container_fixture_link_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_container_fixture_link.py" --selftest
        "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_COMPILER_ID}"
)
set_tests_properties(container_fixture_link_selftest PROPERTIES LABELS selftest)
set_tests_properties(container_fixture_link_selftest PROPERTIES SKIP_RETURN_CODE 77)

# PARITY-GATE, decisao do lider apos o achado desta mesma fatia: dez
# verificacoes do lado Windows (DEPZERO-PARITY-WIN, PORT-PRIVACY-WIN,
# EXPORTS-PARITY-WIN, PRECI-WIN, CI-CONSUME) rodavam soltas como passo
# de shell cru do job `windows` em .github/workflows/ci.yml, invisiveis
# a `ctest -N` - o gate de paridade (tests/tools/check_test_parity.py)
# so enxerga `ctest -N`, entao um par real que so existe fora do ctest
# e, para ele, um par que nao existe. Ordem do lider: registrar como
# teste de verdade, no molde que a onda anterior ja usou para os onze
# gates de arvore (GATE-TREE-PARITY) - com um ganho que passa do
# portao: agora QUALQUER pessoa roda estas dez na propria maquina, nao
# so o servidor.
#
# if(WIN32), nao "sem guarda": ao contrario dos onze gates de arvore
# (texto puro, genuinamente OS-agnostico depois de reescritos em
# python3), estas seis usam mecanismo exclusivo do Windows de verdade
# (dumpbin.exe via PowerShell) - a mesma forma que win32_display_
# connect_test/win32_runner_probe_test ja usam acima neste arquivo.
# find_program(pwsh): "shell: pwsh" ja e a shell declarada em todo
# step do job `windows` que chama estes mesmos scripts hoje
# (.github/workflows/ci.yml) - REQUIRED so dentro do bloco if(WIN32),
# nunca nas outras quatro plataformas, onde pwsh pode nem estar
# instalado e nao faz falta nenhuma.
#
# GLINTFX_CTEST_MODO: as seis chamadas reais (nao as de -SelfTest, que
# nao recebem -Modo) usam a MESMA string "compartilhado"/"estatico"
# que .github/workflows/ci.yml ja passa via matrix.modo - derivada
# aqui de BUILD_SHARED_LIBS (a mesma variavel que decide o modo em
# CMakeLists.txt da raiz) para nao depender de o CI continuar
# passando essa string por fora.
if(WIN32)
    find_program(GLINTFX_PWSH_EXECUTABLE NAMES pwsh REQUIRED)

    if(BUILD_SHARED_LIBS)
        set(GLINTFX_CTEST_MODO "compartilhado")
    else()
        set(GLINTFX_CTEST_MODO "estatico")
    endif()

    # DEPZERO-PARITY-WIN: par de dep_zero_binary_test/dep_zero_binary_
    # selftest (unconditional, acima neste arquivo) - a sub-
    # verificacao (c) (dumpbin /imports aqui, readelf -d no Linux)
    # ganhou nome proprio dos dois lados na mesma fatia que fechou este
    # achado (antes vivia EMBUTIDA dentro do dep_zero_test bundlado, e
    # por isso nao tinha gemeo para apontar). Ver tests/parity_
    # aliases.txt.
    add_test(
        NAME dep_zero_binary_win_selftest
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-dep-zero-win.ps1" -SelfTest
    )
    set_tests_properties(dep_zero_binary_win_selftest PROPERTIES LABELS selftest)

    add_test(
        NAME dep_zero_binary_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-dep-zero-win.ps1"
            -Modo "${GLINTFX_CTEST_MODO}" -BuildDir "${PROJECT_BINARY_DIR}"
    )
    set_tests_properties(dep_zero_binary_win_test PROPERTIES LABELS consume)

    # PORT-PRIVACY-WIN: par EXATO de port_privacy_test/port_privacy_
    # selftest (if(UNIX) acima, tools/check_port_privacy.sh) - os dois
    # lados tem tanto o caso real quanto o proprio autoteste do script,
    # sob o mesmo par de nomes logicos; ver tests/parity_aliases.txt.
    add_test(
        NAME port_privacy_win_selftest
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-port-privacy-win.ps1" -SelfTest
    )
    set_tests_properties(port_privacy_win_selftest PROPERTIES LABELS selftest)

    add_test(
        NAME port_privacy_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-port-privacy-win.ps1"
            -Modo "${GLINTFX_CTEST_MODO}" -BuildDir "${PROJECT_BINARY_DIR}"
    )
    set_tests_properties(port_privacy_win_test PROPERTIES LABELS consume)

    # EXPORTS-PARITY-WIN: exports_win_test e par de visibility_test
    # (if(UNIX) AND BUILD_SHARED_LIBS acima, tools/check_exports.sh);
    # exports_win_selftest e par de visibility_selftest, que ganhou
    # --selftest proprio na mesma fatia que fechou este achado
    # (check_exports.sh nunca tinha um, o irmao Windows ja tinha) -
    # ver tests/parity_aliases.txt para os dois pares.
    add_test(
        NAME exports_win_selftest
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-exports-win.ps1" -SelfTest
    )
    set_tests_properties(exports_win_selftest PROPERTIES LABELS selftest)

    add_test(
        NAME exports_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-exports-win.ps1"
            -Modo "${GLINTFX_CTEST_MODO}" -BuildDir "${PROJECT_BINARY_DIR}"
    )
    set_tests_properties(exports_win_test PROPERTIES LABELS consume)

    # CI-CONSUME (Concluido): par de consume_test (if(UNIX) acima,
    # tools/check_consume.sh) - ver tests/parity_aliases.txt.
    add_test(
        NAME consume_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-consume.ps1"
            -BuildDir "${PROJECT_BINARY_DIR}"
            -PackageSrcDir "${CMAKE_CURRENT_SOURCE_DIR}/package"
    )
    set_tests_properties(consume_win_test PROPERTIES LABELS consume)

    # CI-CONSUME (Concluido): par de embed_test (if(UNIX) acima,
    # tools/check_embed.sh) - ver tests/parity_aliases.txt.
    add_test(
        NAME embed_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-embed.ps1"
            -GlintfxSourceDir "${PROJECT_SOURCE_DIR}"
            -EmbedSrcDir "${CMAKE_CURRENT_SOURCE_DIR}/embed"
    )
    set_tests_properties(embed_win_test PROPERTIES LABELS consume)

    # PRECI-WIN (Concluido): par de preci_selftest (if(UNIX) acima) -
    # ver tests/parity_aliases.txt. Custo aceito e declarado: no job
    # `windows` este caso corria PRIMEIRO, logo apos o checkout, antes
    # de qualquer preparo pesado de MSVC/CMake (falha rapida, sem pagar
    # o build) - registrado como ctest ele passa a rodar junto com o
    # resto da suite, no mesmo passo "Testes" de .github/workflows/
    # ci.yml, igual ao preci_selftest do lado Linux ja faz. Perde o
    # "falha rapida antes do build pesado"; ganha a paridade mecanica
    # que esta fatia existe para provar.
    add_test(
        NAME preci_win_selftest
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/preci.ps1" -Selftest
    )
    set_tests_properties(preci_win_selftest PROPERTIES LABELS selftest)

    # Os quatro abaixo fecham o achado OPOSTO da mesma fatia: quatro
    # testes se(UNIX) sem NENHUM mecanismo Windows, nem ctest nem step
    # cru. Decisao do lider: construir o mecanismo Windows de cada um,
    # lendo o gemeo Linux pelo que ele PROVA, nao pelo que ele faz -
    # tres sao checagem real (mecanismo diferente, cobertura igual);
    # o quarto e uma convencao de empacotamento GNU/Linux que nao
    # existe no MSVC, e por isso e ausencia DECLARADA e CONTADA, nunca
    # um teste que some (GODS_LAWS.md L-40).
    add_test(
        NAME install_includedir_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-install-includedir-win.ps1"
            -GlintfxSourceDir "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(install_includedir_win_test PROPERTIES LABELS consume)

    add_test(
        NAME no_target_collision_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-no-target-collision-win.ps1"
            -GlintfxSourceDir "${PROJECT_SOURCE_DIR}"
            -FixtureSrcDir "${CMAKE_CURRENT_SOURCE_DIR}/embed_name_collision"
    )
    set_tests_properties(no_target_collision_win_test PROPERTIES LABELS consume)

    add_test(
        NAME output_name_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-output-name-win.ps1"
            -BuildDir "${PROJECT_BINARY_DIR}"
            -ConsumerSrcFile "${CMAKE_CURRENT_SOURCE_DIR}/raw_link/main.cpp"
    )
    set_tests_properties(output_name_win_test PROPERTIES LABELS consume)

    # Declarado NAO APLICAVEL de proposito (ver o cabecalho do script) -
    # nao recebe argumento nenhum, e nunca reprova nesta maquina.
    add_test(
        NAME install_packager_layout_win_test
        COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tools/ci/check-install-packager-layout-win.ps1"
    )
    set_tests_properties(install_packager_layout_win_test PROPERTIES LABELS consume)

    # tools/ci/check-pkgconfig-installed.ps1 NAO entra aqui: o unico
    # jeito de chama-lo hoje depende de um valor gerado em TEMPO DE
    # EXECUCAO por um passo anterior do job `windows`
    # (steps.consumo_instalado.outputs.prefix, o prefixo de instalacao
    # escolhido por check-consume.ps1) - nao existe em tempo de
    # configure do CMake, e encaixar isso em ctest pede um mecanismo de
    # ordem/dependencia entre casos (CMake FIXTURES_REQUIRES/
    # FIXTURES_SETUP) que nenhum teste desta arvore usa ainda. Fora de
    # escopo desta fatia - relatado ao orquestrador como um decimo-
    # primeiro caso encontrado, nao um dos dez originais.
endif()

# WAIT-POINTS gate (INBOX, drenagem 06/09/2026, GODS_LAWS.md L-17/L-36/
# L-40): "a promessa publica de que o desenho e o bombeamento de
# eventos nunca prendem o aplicativo" foi furada duas vezes por um
# poll() sem teto (72754af consertou o primeiro sitio; display_adapter.
# cpp::flush_with_retry() era o gemeo, vivo ate este commit, porque a
# auditoria anterior olhou so' o ponto ja conhecido em vez de enumerar
# o espaco inteiro). check_wait_points.py varre os quatro adaptadores
# que tocam o sistema operacional (mais o atomo compartilhado que este
# commit extraiu, bounded_output_wait.cpp) atras de toda chamada que
# PODE bloquear, e confere cada sitio contra tests/wait_points.txt (o
# manifesto - ver o cabecalho dele para o formato e a classificacao).
# Registrado SEM guarda de sistema, a MESMA forma de layers_test acima
# (GATE-TREE-PARITY, GODS_LAWS.md L-04): e' Python puro sobre texto
# fonte, nenhuma compilacao envolvida, entao o comportamento e' o mesmo
# em qualquer plataforma.
add_test(
    NAME check_wait_points_test
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_wait_points.py" "${CMAKE_SOURCE_DIR}"
)
set_tests_properties(check_wait_points_test PROPERTIES LABELS consume)

# GODS_LAWS.md L-36 (portao so conta provado vermelho): --selftest
# fixture os cinco controles (positivo, sitio sem declaracao, manifesto
# podre, varredura vazia, agulha dentro de comentario) contra fixtures
# descartaveis, nunca contra a arvore real - a mesma separacao
# positivo/negativo/vazio que layers_selftest acima ja estabelece.
add_test(
    NAME check_wait_points_selftest
    COMMAND "${GLINTFX_PYTHON3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_wait_points.py" --selftest
)
set_tests_properties(check_wait_points_selftest PROPERTIES LABELS selftest)

# bounded_output_wait_test - o vermelho/verde real (nao so' a leitura
# de texto do gate acima) do atomo que substituiu os dois poll(...,
# -1)/write_deadline duplicados (this file's own header comment on
# check_wait_points_test). Wayland-only (raw AF_UNIX socketpair()/
# fcntl()/poll() POSIX, nenhum tipo Windows), mesma guarda if(UNIX) que
# display_connect_failure_test usa logo acima neste arquivo -
# bounded_output_wait.hpp/.cpp em si nao dependem de wayland-client,
# entao nenhum include adicional alem de src/ e' preciso.
if(UNIX)
    glintfx_add_test(bounded_output_wait_test)
    target_include_directories(bounded_output_wait_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(bounded_output_wait_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
    )
endif()

# flush_retry_policy_test - LOOP-RUN fatia 7, conserto pos-revisao
# (docs/plano-w6b-fatias-6-8.md, D-W6b-57; GODS_LAWS.md L-20): the red/
# green witness for the policy atom that replaced flush_with_retry()'s
# own fixed 100ms budget and unconditional m_fatal latch (this file's
# own header comment on flush_retry_policy.hpp names the defect). Pure
# data in, pure bool out - same if(UNIX) guard as bounded_output_wait_
# test right above it, because bounded_wait_outcome (the enum this atom
# reads) only exists to serve the Wayland write-wait today; no wayland-
# client, no socket, no wl_display needed for this target itself.
if(UNIX)
    glintfx_add_test(flush_retry_policy_test)
    target_include_directories(flush_retry_policy_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_sources(flush_retry_policy_test PRIVATE
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/bounded_output_wait.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/wayland/flush_retry_policy.cpp"
    )
endif()
