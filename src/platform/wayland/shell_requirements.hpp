// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>

#include "platform/wayland/global_catalog.hpp"

// platform/wayland/shell_requirements.hpp - WL-WINDOW fatia W-B
// (docs/plano-w6a-janela.md fatia 5): the PURE half of "does this
// compositor announce what a shell needs to exist at all" -
// wl_compositor (the factory a wl_surface is created from) and
// xdg_wm_base (the desktop-shell protocol xdg_surface/xdg_toplevel
// build on top of). Pure in the same sense global_catalog.hpp already
// is: no Wayland type reachable from this header or its own .cpp,
// only glintfx::platform::global_catalog and plain interface-name
// strings - testable without a compositor, without libwayland-client
// linked in at all.
//
// "ONDE ESTA O PERIGO" item 2 (docs/plano-w6a-janela.md, fatia W-B):
// a compositor that never announces xdg_wm_base is a real environment
// this project has to name, never guess at three calls later as a
// null pointer - wayland_shell_adapter::open() (shell_adapter.hpp)
// calls check_shell_requirements() BEFORE it ever calls
// wl_registry_bind(), and the refusal names EXACTLY which interface
// was missing (gltfx_err::rejected_value(), docs/api-conventions.md
// R7: "name always a stable identifier, never a sentence" - the value
// attached is the bare interface string, e.g. "wl_compositor", never
// a sentence describing the failure). Whoever reads the error knows
// what to install or which compositor to switch to.
//
// ORDER IS FIXED, AND DELIBERATE: wl_compositor is checked before
// xdg_wm_base. A compositor missing BOTH always reports wl_compositor
// first - there is only ever one answer to "why did it name this one
// and not the other".

namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void> check_shell_requirements(const global_catalog &catalog) noexcept;

} // namespace glintfx::platform
