// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/wayland/power_source_adapter.hpp"

// selected_power_source_adapter.hpp - GFX-PRESET, fatia P2
// (docs/plano-w6b-fatias-5.md sec. 5.1, GODS_LAWS.md L-19), the exact sibling
// of selected_gl_context_adapter.hpp one file over
// ("adaptador escolhido na compilacao"): gl_context_impl.hpp (P3) expects to
// find EXACTLY this file, under EXACTLY this name, on a non-Windows build.
// src/platform/CMakeLists.txt only add_subdirectory()s this wayland/ directory
// under if(UNIX), so a Windows build never sees this alias; Windows' own
// sibling (src/platform/win32/selected_power_source_adapter.hpp) does the same
// with its own adapter, and there is no #ifdef inside any function body.
namespace glintfx::platform {

using selected_power_source_adapter = wayland_power_source_adapter;

} // namespace glintfx::platform
