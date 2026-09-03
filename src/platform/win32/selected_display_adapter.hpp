// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/win32/display_adapter.hpp"

// selected_display_adapter.hpp - WIN-DISPLAY, the compile-time
// selection itself, the exact sibling of src/platform/wayland/
// selected_display_adapter.hpp (GODS_LAWS.md L-19 item 2: "adaptador
// escolhido na compilação"). src/platform/CMakeLists.txt only
// add_subdirectory()s this win32/ directory under elseif(WIN32) - a
// build that never enters this directory never sees this alias, and
// there is no #ifdef inside any function body picking between backends
// anywhere in this project (GODS_LAWS.md L-19 armadilha 3). This file
// is the win32/ sibling the Wayland header's own comment promised a
// future backend would provide, aliasing ITS OWN adapter - the Wayland
// file was never edited to make room for it.
namespace glintfx::platform {

using selected_display_adapter = win32_display_adapter;

} // namespace glintfx::platform
