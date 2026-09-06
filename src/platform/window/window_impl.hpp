// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>
#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

#if defined(_WIN32)
#include "platform/win32/selected_window_adapter.hpp"
#else
#include "platform/wayland/selected_window_adapter.hpp"
#endif

// window_impl.hpp - WL-WINDOW-HANDLE + GL-CONTEXT (docs/plano-w6a-
// janela.md, docs/plano-w6b-placa-e-laco.md D-W6b-25, GODS_LAWS.md
// L-17): the concrete type glintfx::window_impl (forward-declared,
// opaque, in the public include/glintfx/platform/window/window.hpp)
// actually IS - moved OUT of window_facade.cpp into this shared,
// INTERNAL header (never installed, never under include/glintfx/,
// invisible to a consumer the same way display_impl.hpp already is,
// one directory over) so gl_context_facade.cpp (src/platform/gl/,
// GL-CONTEXT fatia 2b) can reach the SAME definition through gltfx_
// window::internal_impl() (window.hpp's own window_internal_access
// passkey, added by this same fatia) - GODS_LAWS.md L-17: two
// translation units that both need to know what window_impl IS share
// ONE definition, never a second copy that could drift, the exact
// precedent display_impl.hpp already set for display_facade.cpp/
// window_facade.cpp one fatia ago.
//
// fixed_open_only_gfx_options IS THE NEW FIELD (D-W6b-25, sec. 14.2's
// own "a janela guarda o conjunto em window_impl, nunca em window_
// state"): the resolved set of `open_only` graphics options (gfx_
// option.hpp) the FIRST gltfx_gl_context that ever opened on this
// window fixed - std::nullopt means no context has opened on this
// window yet. window_state (window_state.hpp) is deliberately NOT
// where this lives: window_state is the NEUTRAL, backend-shared VALUE
// TYPE both wayland_window_adapter and win32_window_adapter already
// write into for entirely different facts (size, active/maximized/
// fullscreen, close_requested) - folding an unrelated GL-CONTEXT
// concern into it would be exactly the "monolith de tabela" GODS_
// LAWS.md L-17 already forbids gfx_option.hpp's own registry from
// becoming. This field lives on window_impl itself instead, right
// next to the adapter it has nothing to do with, because window_impl
// - unlike window_state - is not a value type shared across backends;
// it is this project's own PER-WINDOW bookkeeping struct, and D-W6b-25's
// own fixation is exactly that: a fact about ONE window, read and
// written only by gl_context_facade.cpp, through the SAME passkey
// idiom display_internal_access already established for display_impl.
namespace glintfx {

struct window_impl {
    platform::selected_window_adapter adapter;
    std::optional<std::vector<gltfx_gfx_option_entry>> fixed_open_only_gfx_options;
};

} // namespace glintfx
