// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/wayland/window_adapter.hpp"

// selected_window_adapter.hpp - WL-WINDOW-HANDLE, the exact sibling of
// selected_display_adapter.hpp one file over (GODS_LAWS.md L-19 item
// 2: "adaptador escolhido na compilação"). src/platform/CMakeLists.txt
// only add_subdirectory()s this wayland/ directory under if(UNIX) - a
// build that never enters this directory never sees this alias, and
// there is no #ifdef inside any function body picking between backends
// anywhere in this project (GODS_LAWS.md L-19 armadilha 3). A future
// Win32 backend provides its own sibling file of the same name, under
// its own directory, aliasing ITS OWN adapter - never editing this
// one (src/platform/win32/selected_window_adapter.hpp does exactly
// that, same fatia).
namespace glintfx::platform {

using selected_window_adapter = wayland_window_adapter;

} // namespace glintfx::platform
