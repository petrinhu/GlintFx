// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>

#include "platform/window/window_state.hpp"

// window_adapter_port.hpp - WL-WINDOW-HANDLE (docs/plano-w6a-janela.md
// fatias 6/9/11, absorvidas nesta fatia por decisão do CTO): the
// compile-time contract window_facade.cpp's own gltfx_window checks
// wayland_window_adapter/win32_window_adapter against - the sibling
// concept display_connection_port.hpp's own header comment predicted
// ("cresce como concept IRMAO ao lado deste, nunca colado nele").
//
// DELIBERATELY NARROW, AND DELIBERATELY EXCLUDES open() - GODS_LAWS.md
// L-19 armadilha 2 ("porta gorda"), the SAME lesson display_connection_
// port.hpp already names, enforced here on purpose because forcing
// open() into this concept is exactly what an earlier reviewer had to
// remove twice: wayland_window_adapter::open() takes (wayland_display_
// adapter&, wayland_shell_adapter&, const wayland_window_desc&);
// win32_window_adapter::open() takes (const win32_display_adapter&,
// const win32_window_desc&) - genuinely different SHAPES, not just
// different types plugged into one signature, because Wayland's own
// window needs an already-bound shell (wl_compositor + xdg_wm_base)
// that Win32 has no equivalent of. window_facade.cpp's own #if branch
// calls each adapter's real open() directly; this concept only covers
// the surface BOTH adapters already share, fully formed, AFTER
// whichever platform-specific open() succeeded.
//
// window_state RETURNED BY CONST REFERENCE, NEVER COPIED (GODS_LAWS.md
// L-19): both adapters already expose the exact same `const window_
// state &state() const noexcept` - the ONE shared, #if-free value type
// this project's own window_configure_sequence.cpp/win32/window_
// adapter.cpp both write into (docs/plano-w6a-janela.md sec. 5 risk 4,
// "a derivacao vive em window_state, comum").
//
// set_title() IS PART OF THE PORT, NOT AN OPTIONAL EXTRA (WL-WINDOW-
// HANDLE's own paridade finding, this fatia): before this fatia,
// wayland_window_adapter had no set_title() at all - a public
// gltfx_window::set_title() built on top of a port that only required
// it on ONE side would have shipped a method that silently did nothing
// on the other. Requiring it here means the SELECTION mechanism itself
// (selected_window_adapter_check.cpp, one per platform) fails to
// compile the moment either adapter falls behind, the same "the build
// breaks instead of linking something incomplete" contract GODS_LAWS.md
// L-19/L-40 already gives display_connection_port.hpp.
//
// noexcept throughout - same reasoning display_connection_port.hpp's
// own header comment gives: every adapter this project ships reports
// failure through gltfx_rslt<T>, never a C++ exception.
namespace glintfx::platform {

template <typename A>
concept window_adapter_port = std::movable<A> && requires(A &adapter, std::string_view text) {
    { std::as_const(adapter).is_open() } noexcept -> std::same_as<bool>;
    { std::as_const(adapter).state() } noexcept -> std::same_as<const window_state &>;
    { adapter.set_title(text) } noexcept -> std::same_as<gltfx_rslt<void>>;
    { adapter.close() } noexcept -> std::same_as<void>;
};

} // namespace glintfx::platform
