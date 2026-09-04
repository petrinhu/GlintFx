# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/gfui/CMakeLists.txt
#
# gfui layer (GFSS-NODE-VIEW, GODS_LAWS.md L-17/L-19/L-28): the motor's
# own module, distinct from gfss/ (the leaf FORMAT) and gfml (the
# markup) - ESCOPO.md SS4 fixes the trio and node_view.hpp's own header
# comment (D-NV-1) explains why the node contract belongs here, not in
# gfss/. Pure core, same reason src/core/ and src/gfss/ are: no file
# here includes an OS header (tests/tools/check_layers.py's
# OS_HEADER_PATTERN would reprove one that did, though this directory,
# like gfss/ before it, is not yet named in that script's own
# core_source_dirs() list - the plano's own INBOX line, TODO.md,
# records that gap for a future higiene fatia, not this one).
#
# Single subject: the sources of this layer. Properties of the
# `glintfx_library` target stay in src/CMakeLists.txt; this file only
# adds sources to a target already created in the parent directory.
#
# GFSS-MATCH-SIMPLE (TODO.md, /var/tmp/glintfx-plan/gfss-match-simple-
# plano.md item 0.6): compound_match.cpp is the FIRST file in this
# layer to #include something from src/gfss/ (gfss/selector_ast.hpp,
# gfss/ascii_case.hpp) - a motor -> format dependency, the correct
# direction per ESCOPO.md SS4. glintfx_library never had "${PROJECT_
# SOURCE_DIR}/src" on its own PRIVATE include path before this (cmake/
# GlintfxLibrary.cmake's glintfx_set_target_include_dirs() only adds
# include/ and the generated dir, both PUBLIC); this is the SAME
# precedent src/render/CMakeLists.txt:100 already sets for its own
# PRIVATE include dir, applied here so a quoted "gfss/..."/"gfui/..."
# include resolves the same way it already does for every test target
# in tests/CMakeLists.txt (their own "${PROJECT_SOURCE_DIR}/src"
# PRIVATE addition, e.g. gfui_node_view_test's own comment).
target_sources(glintfx_library PRIVATE
    node_state.cpp
    compound_match.cpp
)

target_include_directories(glintfx_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
