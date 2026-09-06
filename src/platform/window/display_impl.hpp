// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/port/display_connection.hpp"

#if defined(_WIN32)
#include "platform/win32/selected_display_adapter.hpp"
#else
#include "platform/wayland/selected_display_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#endif

// display_impl.hpp - WL-WINDOW-HANDLE (docs/plano-w6a-janela.md): the
// concrete type glintfx::display_impl (forward-declared, opaque, in
// the public include/glintfx/platform/window/display.hpp) actually IS -
// moved OUT of display_facade.cpp into this shared, INTERNAL header
// (never installed, never under include/glintfx/, invisible to a
// consumer the same way every other src/platform/port/ file already
// is) so window_facade.cpp (this fatia's own new TU) can reach the
// SAME definition through gltfx_display::internal_impl() (display.hpp's
// own internal seam) - GODS_LAWS.md L-17: two translation units that
// both need to know what display_impl IS share ONE definition, never a
// second copy that could drift.
//
// WAYLAND GAINS A SHELL HERE, NOT JUST A CONNECTION (this fatia's own
// scope, decided by the CTO): wayland_window_adapter::open() (src/
// platform/wayland/window_adapter.hpp) needs an already-open wayland_
// shell_adapter (wl_compositor + xdg_wm_base bound) as well as the
// display connection itself - window_smoke.cpp (tests/container/) is
// the proof this composition is real, not assumed (it opens the
// display adapter, THEN a separate wayland_shell_adapter over it, THEN
// the window over both). gltfx_display::open() (display_facade.cpp) is
// where the shell gets opened now, right after the connection - a
// consumer never sees this extra step, exactly the point of hiding it
// inside display_impl. Declared AFTER `connection` on purpose: member
// destruction order is the REVERSE of declaration order, so a plain
// `delete m_impl` in gltfx_display's own destructor already tears the
// shell down BEFORE the connection it was bound against, the same
// "teardown in reverse of creation" convention every adapter's own
// close() in this project already documents - no explicit destructor
// body needed here for that to hold.
//
// Win32 needs no such composition: win32_window_adapter::open() takes
// only the already-open win32_display_adapter (src/platform/win32/
// window_adapter.hpp) - no shell-shaped concept exists on that
// platform, by design (docs/plano-w6a-janela.md fatia 5's own row:
// "mecanismo inexistente (Win32 nao tem registry)").
namespace glintfx {

#if defined(_WIN32)

struct display_impl {
    platform::display_connection<platform::selected_display_adapter> connection;
};

#else

struct display_impl {
    platform::display_connection<platform::selected_display_adapter> connection;
    platform::wayland_shell_adapter shell;
};

#endif

} // namespace glintfx
