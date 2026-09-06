// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/win32/wgl_context_adapter.hpp"

// selected_gl_context_adapter.hpp - X-WGL (docs/plano-w6b-placa-e-
// laco.md fatia 4, D-W6b-1, GODS_LAWS.md L-19), the exact Win32 sibling
// of src/platform/wayland/selected_gl_context_adapter.hpp one directory
// over ("adaptador escolhido na compilacao" - GODS_LAWS.md L-19 item
// 2): gl_context_impl.hpp (src/platform/gl/, fatia 2b) expects to find
// EXACTLY this file, under EXACTLY this name, the moment it is
// compiled on Windows.
//
// src/platform/CMakeLists.txt only add_subdirectory()s this win32/
// directory under if(WIN32) - a build that never enters this directory
// never sees this alias, and there is no #ifdef inside any function
// body picking between backends anywhere in this project (GODS_LAWS.md
// L-19 armadilha 3). Wayland's own sibling (src/platform/wayland/
// selected_gl_context_adapter.hpp, fatia 3) does exactly the same thing
// under its own directory, aliasing ITS OWN adapter - never editing
// this one.
namespace glintfx::platform {

using selected_gl_context_adapter = win32_gl_context_adapter;

} // namespace glintfx::platform
