// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/win32/power_source_adapter.hpp"

// selected_power_source_adapter.hpp - GFX-PRESET, fatia P2
// (docs/plano-w6b-fatias-5.md sec. 5.1, GODS_LAWS.md L-19), the exact Win32
// sibling of src/platform/wayland/ selected_power_source_adapter.hpp one
// directory over: gl_context_impl.hpp (P3) expects to find EXACTLY this file,
// under EXACTLY this name, on a Windows build. src/platform/CMakeLists.txt only
// add_subdirectory()s this win32/ directory under if(WIN32); there is no #ifdef
// inside any function body picking between backends.
namespace glintfx::platform {

using selected_power_source_adapter = win32_power_source_adapter;

} // namespace glintfx::platform
