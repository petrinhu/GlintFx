# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/gfss/CMakeLists.txt
#
# gfss layer (GODS_LAWS.md L-19/L-28): the gfss style-sheet
# tokenizer/parser track ("A trilha GFSS", TODO.md). Pure core, like
# core/ - no file here includes an OS header (tests/tools/
# check_layers.sh's OS_HEADER_PATTERN would reprove one that did,
# same gate core/ already answers to, even though this directory is
# not yet named in that script's own core_source_dirs() list).
#
# Single subject: the sources of this layer. Properties of the
# `glintfx_library` target stay in src/CMakeLists.txt; this file only
# adds sources to a target already created in the parent directory.

target_sources(glintfx_library PRIVATE
    token_kind.cpp
    lexical_rules.cpp
    tokenizer.cpp
    named_colors.cpp
    color_parse.cpp
)
