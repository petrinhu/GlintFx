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
# gl_context_facade.cpp IS DELIBERATELY NOT LISTED HERE YET (GL-CONTEXT
# fatia 2b's own scope boundary): it PIMPLs over platform::selected_gl_
# context_adapter, a type ONLY fatia 3 (Wayland/EGL) or fatia 4 (Win32/
# WGL) provides - src/platform/wayland/selected_gl_context_adapter.hpp
# / src/platform/win32/selected_gl_context_adapter.hpp neither exists
# yet. Adding this file's own target_sources entry before either lands
# would fail glintfx_library's own build on BOTH platforms (this is a
# single shared target, GODS_LAWS.md L-11's own "1 trabalho pesado por
# vez" - breaking it here blocks every OTHER test in the suite, not
# only GL ones). docs/plano-w6b-placa-e-laco.md's own §4 bundles fatias
# 2+3+4 into ONE push (push 2) for exactly this reason. The file itself
# (gl_context_impl.hpp, gl_context_facade.cpp) is written and reviewed
# as part of fatia 2b; wiring it into this target_sources() call is the
# LAST step of whichever of fatia 3/4 lands second, in the SAME commit
# that finally makes glintfx::glintfx build with a GL context inside.
#
# Added UNCONDITIONALLY, before the UNIX/WIN32 branch in src/platform/
# CMakeLists.txt - same reasoning port/, window/ and input/ already
# give for their own unconditional add_subdirectory(): none of these
# six .cpp files reaches an operating-system header (each is a pure
# atom over plain data, or - gfx_option_registry.cpp - only touches its
# own table), so there is nothing platform-specific here to gate.

target_sources(glintfx_library PRIVATE
    gfx_option_registry.cpp
    gfx_option_validation.cpp
    gfx_open_only_fixation.cpp
    gl_context_desc_validation.cpp
    gl_version_policy.cpp
    gl_surface_size_policy.cpp
)
