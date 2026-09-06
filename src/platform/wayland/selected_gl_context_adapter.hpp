// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/wayland/egl_context_adapter.hpp"

// selected_gl_context_adapter.hpp - W-EGL (docs/plano-w6b-placa-e-
// laco.md fatia 3, D-W6b-1, GODS_LAWS.md L-19), the exact sibling of
// selected_window_adapter.hpp/selected_display_adapter.hpp one file
// over ("adaptador escolhido na compilacao" - GODS_LAWS.md L-19 item
// 2): gl_context_impl.hpp (src/platform/gl/, fatia 2b) expects to find
// EXACTLY this file, under EXACTLY this name, the moment it is
// compiled on a non-Windows platform - the #include this fatia's own
// briefing named as "declarado aqui, so como o #include que este
// arquivo espera encontrar".
//
// src/platform/CMakeLists.txt only add_subdirectory()s this wayland/
// directory under if(UNIX) - a build that never enters this directory
// never sees this alias, and there is no #ifdef inside any function
// body picking between backends anywhere in this project (GODS_LAWS.md
// L-19 armadilha 3). Windows' own sibling
// (src/platform/win32/selected_gl_context_adapter.hpp, fatia 4) does
// exactly the same thing under its own directory, aliasing ITS OWN
// adapter - never editing this one.
namespace glintfx::platform {

using selected_gl_context_adapter = wayland_egl_context_adapter;

} // namespace glintfx::platform
