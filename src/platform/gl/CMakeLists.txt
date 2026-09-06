# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/gl/CMakeLists.txt
#
# Single subject: C-OPT (docs/plano-w6b-placa-e-laco.md fatia 2a) - the
# OS-AGNOSTIC graphics-option mechanism (the registry gfx_option_
# registry.{hpp,cpp} defines the four GLINTFX_API discovery functions
# gfx_option.hpp declares over, and the two pure atoms gfx_option_
# validation.cpp/gfx_open_only_fixation.cpp) - PLUS, GL-CONTEXT's own
# fatia 2b: the three more OS-AGNOSTIC pure atoms it adds (gl_context_
# desc_validation.cpp, gl_version_policy.cpp, gl_surface_size_policy.
# cpp; gpu_kind_state.hpp is header-only, no .cpp of its own) - D-W6b-1:
# "handle proprio ... em diretorio publico proprio include/glintfx/
# platform/gl/".
#
# gl_context_facade.cpp - WIRED IN by X-WGL (docs/plano-w6b-placa-e-
# laco.md fatia 4, 06/09/2026), the LAST of fatia 3 (Wayland/EGL) / 4
# (Win32/WGL) to land: it PIMPLs over platform::selected_gl_context_
# adapter, and BOTH src/platform/wayland/selected_gl_context_adapter.hpp
# (fatia 3) and src/platform/win32/selected_gl_context_adapter.hpp
# (fatia 4, this commit) now exist, so glintfx_library builds on both
# platforms with a real GL context inside. Was deliberately NOT listed
# here between fatia 2b landing and fatia 4 landing (docs/plano-w6b-
# placa-e-laco.md's own §4 bundles fatias 2+3+4 into ONE push, push 2,
# for exactly this reason - a single shared target, GODS_LAWS.md L-11's
# own "1 trabalho pesado por vez", would have failed the build on BOTH
# platforms otherwise). The file itself (gl_context_impl.hpp, gl_
# context_facade.cpp) was written and reviewed as part of fatia 2b;
# this line is that fatia's own promised last step.
#
# Added UNCONDITIONALLY, before the UNIX/WIN32 branch in src/platform/
# CMakeLists.txt - same reasoning port/, window/ and input/ already
# give for their own unconditional add_subdirectory(): none of these
# seven .cpp files reaches an operating-system header directly (each is
# a pure atom over plain data, or - gfx_option_registry.cpp - only
# touches its own table, or - gl_context_facade.cpp - only PIMPLs over
# whichever selected_gl_context_adapter the platform-specific
# subdirectory already resolved), so there is nothing platform-specific
# here to gate.

target_sources(glintfx_library PRIVATE
    gfx_option_registry.cpp
    gfx_option_validation.cpp
    gfx_open_only_fixation.cpp
    gl_context_desc_validation.cpp
    gl_version_policy.cpp
    gl_surface_size_policy.cpp
    gl_context_facade.cpp
)
