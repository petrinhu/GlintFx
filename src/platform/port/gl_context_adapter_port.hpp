// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/port/adapter_pin.hpp"

// gl_context_adapter_port.hpp - GL-CONTEXT (docs/plano-w6b-placa-e-
// laco.md fatia 2b, D-W6b-1, GODS_LAWS.md L-04/L-19): the compile-time
// contract gl_context_facade.cpp's own gltfx_gl_context will check a
// concrete Wayland EGL adapter (fatia 3) / Win32 WGL adapter (fatia 4)
// against, once either exists - the sibling concept window_adapter_
// port.hpp/display_backend_port.hpp's own header comments already
// predicted this project would keep growing, one PER DOMAIN, never
// widening an existing one to cover a new subsystem.
//
// DELIBERATELY EXCLUDES open(), FOR THE SAME REASON window_adapter_
// port.hpp ALREADY DOES (that header's own comment, "porta gorda",
// GODS_LAWS.md L-19 armadilha 2): a Wayland gl-context adapter's own
// open() needs the ALREADY-OPEN wayland_window_adapter it renders onto
// (to reach the wl_surface D-W6b-1's own F2 fact names); a Win32
// gl-context adapter's own open() needs the win32_window_adapter (to
// reach its HWND) - the CONCRETE PARAMETER TYPE differs by platform,
// and this fatia's own mandate ("nada aqui toca o sistema operacional
// ... se voce sentir vontade de escrever codigo de EGL ou WGL, pare")
// forbids this common, OS-agnostic port header from #include-ing
// either platform's own window-adapter header just to name that type
// in a requires-clause. gl_context_facade.cpp's own #if branch (the
// SAME shape window_facade.cpp's open() already uses) is where each
// platform's genuinely different open() call happens directly, never
// abstracted through this concept - see this header's own COVERAGE
// list below for exactly what IS checked here instead.
//
// COVERAGE: every method BOTH adapters share, fully formed, AFTER
// whichever platform-specific open() succeeded - is_open, close,
// make_current, swap_buffers, proc_address, apply_option, option_
// support, gpu. Same shape window_adapter_port.hpp already documents
// for the identical reason, one layer up.
//
// TAKES A RESOLVED gltfx_gfx_option_entry, NEVER THE RAW gltfx_gl_
// context_desc: by the time apply_option() below is ever called,
// gl_context_facade.cpp has already run gfx_option_validation.hpp -
// what an adapter receives through apply_option() is a single,
// already-shape-checked entry (docs/plano-w6b-placa-e-laco.md D-W6b-16:
// "nunca degrada em silencio" - an adapter that cannot honor a `live`
// entry answers through apply_option()'s own gltfx_rslt<void>, naming
// the option the same way gfx_option_validation.hpp's own refusals
// already do).
//
// apply_option()/option_support()/gpu() ARE PER-SYSTEM, NEVER PURE:
// gl_context_facade.cpp's own set_option()/option_support()/gpu()
// (context.hpp) forward directly to these three - the adapter is the
// ONLY thing that knows whether THIS driver honors `adaptive` v-sync,
// what `unsupported_here` looks like on this system, and which GPU a
// now-current context is actually rendering on (gpu_kind_state.hpp is
// the SHARED atom an adapter is expected to embed to answer gpu(),
// never duplicated logic per platform).
//
// swap_buffers() RETURNS gltfx_present_outcome (D-W6b-6), NEVER A BARE
// gltfx_rslt<void>: "did this frame reach the screen" is not a binary
// success/failure the way make_current() is - a window this system
// cannot currently repaint is not a FAILURE, it is `skipped_hidden`
// (context.hpp's own header comment, item 2).
//
// noexcept throughout - same reasoning display_connection_port.hpp/
// window_adapter_port.hpp already give: every adapter this project
// ships reports failure through gltfx_rslt<T>, never a C++ exception.
//
// FACADE-PIN (docs/plano-conserto-fachadas-uaf.md sec. 6/7.1): this
// concept required std::movable<A> until this fatia - the same class of
// defect window_adapter_port.hpp's own header comment names (a proxy
// pointing at a dead stack address after a move) is latent here too,
// derrubado today only by call ORDER (wayland_egl_context_adapter's own
// attach_frame_listener() runs only from swap_buffers(), always after
// gl_context_facade.cpp's own move into the heap - this plan's own
// sec. 3 #7). pinned_adapter<A> (platform/port/adapter_pin.hpp)
// closes this by construction instead of by ordering.
namespace glintfx::platform {

template <typename A>
concept gl_context_adapter_port =
    pinned_adapter<A> && requires(A &adapter, const A &const_adapter, gltfx_gfx_option_entry entry,
                                  gltfx_gfx_option id, std::string_view name) {
        { adapter.close() } noexcept -> std::same_as<void>;
        { const_adapter.is_open() } noexcept -> std::same_as<bool>;
        { adapter.make_current() } noexcept -> std::same_as<gltfx_rslt<void>>;
        { adapter.swap_buffers() } noexcept -> std::same_as<gltfx_rslt<gltfx_present_outcome>>;
        { const_adapter.proc_address(name) } noexcept -> std::same_as<void *>;
        { adapter.apply_option(entry) } noexcept -> std::same_as<gltfx_rslt<void>>;
        { const_adapter.option_support(id) } noexcept -> std::same_as<gltfx_gfx_option_support>;
        { const_adapter.gpu() } noexcept -> std::same_as<gltfx_gpu_info>;
    };

} // namespace glintfx::platform
