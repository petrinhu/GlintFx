// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// platform/window/display.hpp - W-D' (docs/plano-w6a-janela.md fatia
// 6): gltfx_display, D-W5-1's own decision made concrete - "conexao
// vira handle publico gltfx_display; janela abre a partir dele". This
// is the ONE type any consumer of this library ever names to reach the
// display server, on either platform this project ships a backend for
// today (Wayland, Win32) - never a concrete adapter type, never a
// per-platform #ifdef in consumer code.
//
// OPAQUE, PIMPL (CONTRACT.md's own "Fronteira publica opaca": "handle
// e subsistema com estado ... expostos como handle opaco ou PIMPL,
// nunca como classe publica com layout visivel" - GODS_LAWS.md L-19).
// display_impl's full layout lives ENTIRELY inside display_facade.cpp,
// on the library's own side of the .so/.dll boundary - the exact same
// "allocation and deallocation of the same data live on the SAME side
// of the boundary" reasoning docs/api-conventions.md R5 already gives
// for glintfx::gltfx_err's own m_context, applied here to a whole
// connection instead of a diagnostic payload. A consumer NEVER sees a
// wayland_display_adapter, a win32_display_adapter, nor (later) a
// composed backend type - swapping what display_impl wraps on either
// platform, or composing a shell/seat into it later (docs/plano-w6a-
// janela.md's own future wayland_display_backend), changes NOTHING
// about this header, because nothing here names the concrete type.
//
// RAII, NAMED-CONSTRUCTOR IDIOM (GODS_LAWS.md L-22, the exact same
// shape platform::display_connection<A>::connect() already documents
// one layer down, src/platform/port/display_connection.hpp): open()
// is a FALLIBLE static factory returning gltfx_rslt<gltfx_display>,
// never a bare constructor that could fail. A caller either gets a
// gltfx_display that is genuinely open, or gets a gltfx_err and no
// object at all - there is no half-open gltfx_display for a caller to
// misuse.
//
// pump_events() IS THE ONE THING BEYOND open/close/is_open THIS
// HANDLE NEEDS IN v1 (docs/plano-w6a-janela.md fatia 6's own proof,
// "abre gltfx_display publico, 50 pumps, fecha" - D-W5-1's own
// decision: "a unica forma que deixa LOOP-RUN bombear uma vez por
// quadro"). Backed by platform::display_backend_port (src/platform/
// port/display_backend_port.hpp), the sibling concept this fatia adds
// specifically so this one extra capability does not have to widen
// platform::display_connection_port itself.
//
// noexcept throughout (docs/api-conventions.md R3): every fallible
// signature here is noexcept, the boundary translates, nothing throws
// or aborts across it.

namespace glintfx {

// Opaque (GODS_LAWS.md L-19): the full layout - today, a platform::
// display_connection<platform::selected_display_adapter> - is a
// private implementation detail, defined ONLY in display_facade.cpp.
// A consumer never sees this type; gltfx_display below carries only a
// pointer to one.
struct display_impl;

class gltfx_display {
  public:
    // Connects to the display server and, on success, wraps the
    // NOW-OPEN connection in a gltfx_display the caller owns. On
    // failure, whatever gltfx_err the underlying backend reported is
    // returned UNCHANGED (docs/api-conventions.md, "o erro injetado
    // chega ao chamador intacto" - this factory never re-codes, wraps
    // or replaces it) and no gltfx_display is ever constructed at all.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_display> open() noexcept;

    // Move-only: a gltfx_display OWNS the one open connection its
    // impl holds, and closing it twice (once for the original, once
    // for a copy) is exactly the kind of double-close bug ownership
    // types exist to make unrepresentable - the same reasoning
    // platform::display_connection<A>'s own deleted copy members
    // already document, one layer down.
    gltfx_display(const gltfx_display &) = delete;
    gltfx_display &operator=(const gltfx_display &) = delete;

    GLINTFX_API gltfx_display(gltfx_display &&other) noexcept;
    GLINTFX_API gltfx_display &operator=(gltfx_display &&other) noexcept;

    // Closes on scope exit - RAII, not something the caller has to
    // remember to invoke by hand. Safe on a moved-from instance (the
    // moved-from side's m_impl is left null).
    GLINTFX_API ~gltfx_display();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // The non-blocking event pump a consumer's own main loop calls
    // once per frame (LOOP-RUN, D-W5-1) - never stalls waiting for the
    // display server, safe to call every frame unconditionally. See
    // this header's own "pump_events() IS THE ONE THING" paragraph
    // above for why this is on gltfx_display's v1 surface at all.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> pump_events() noexcept;

  private:
    explicit gltfx_display(display_impl *impl) noexcept : m_impl(impl) {}

    display_impl *m_impl = nullptr;
};

} // namespace glintfx
