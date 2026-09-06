# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/gl/CMakeLists.txt
#
# Single subject: C-OPT (docs/plano-w6b-placa-e-laco.md fatia 2a) - the
# OS-AGNOSTIC graphics-option mechanism (the registry gfx_option_
# registry.{hpp,cpp} defines the four GLINTFX_API discovery functions
# gfx_option.hpp declares over, and the two pure atoms gfx_option_
# validation.cpp/gfx_open_only_fixation.cpp) plus, later, the OS-
# touching gl_context handle fatia 2b lands here too (docs/plano-w6b-
# placa-e-laco.md D-W6b-1: "handle proprio ... em diretorio publico
# proprio include/glintfx/platform/gl/").
#
# Added UNCONDITIONALLY, before the UNIX/WIN32 branch in src/platform/
# CMakeLists.txt - same reasoning port/, window/ and input/ already
# give for their own unconditional add_subdirectory(): none of these
# three .cpp files reaches an operating-system header (gfx_option_
# registry.cpp only touches its own table; gfx_option_validation.cpp
# and gfx_open_only_fixation.cpp are pure atoms over plain data), so
# there is nothing platform-specific here to gate.

target_sources(glintfx_library PRIVATE
    gfx_option_registry.cpp
    gfx_option_validation.cpp
    gfx_open_only_fixation.cpp
)
