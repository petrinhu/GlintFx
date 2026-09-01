# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/gfui/CMakeLists.txt
#
# gfui layer (GFSS-NODE-VIEW, GODS_LAWS.md L-17/L-19/L-28): the motor's
# own module, distinct from gfss/ (the leaf FORMAT) and gfml (the
# markup) - ESCOPO.md SS4 fixes the trio and node_view.hpp's own header
# comment (D-NV-1) explains why the node contract belongs here, not in
# gfss/. Pure core, same reason src/core/ and src/gfss/ are: no file
# here includes an OS header (tests/tools/check_layers.sh's
# OS_HEADER_PATTERN would reprove one that did, though this directory,
# like gfss/ before it, is not yet named in that script's own
# core_source_dirs() list - the plano's own INBOX line, TODO.md,
# records that gap for a future higiene fatia, not this one).
#
# Single subject: the sources of this layer. Properties of the
# `glintfx_library` target stay in src/CMakeLists.txt; this file only
# adds sources to a target already created in the parent directory.

target_sources(glintfx_library PRIVATE
    node_state.cpp
)
