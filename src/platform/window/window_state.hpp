// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/window/window_state.hpp - W-C' (docs/plano-w6a-janela.md
// fatia 4; D-W5-14 ampliada by decision 15 and D-W6a-20): the ONE
// window state type both backends write into and every caller reads
// from - GODS_LAWS.md L-04's "comportamento igual em todo sistema"
// applied to a value type instead of to a function. There is exactly
// one copy of this class, no #if anywhere in it, so the two backends
// cannot drift apart by construction.
//
// D-W5-2 (confirmed): a window exposes CONSULTABLE STATE, never an
// event queue - a queue is INPUT-EVENTS' job (W7), with the delivery
// guarantee GODS_LAWS.md L-35 will require; a callback here would run
// consumer code from inside a libwayland/Win32 callback, which this
// project's window layer never does.
//
// TWO SIZES, NEVER ONE (decision 15, porta de mao unica): logical_
// size() is the size in the window system's own coordinate space
// (Wayland: surface-local units; Win32: the pixel size scaled back to
// the 96-DPI baseline). pixel_size() is the size of the framebuffer a
// consumer must actually paint into (Wayland: logical times the
// compositor's preferred_buffer_scale; Win32: the raw GetClientRect
// pixel rectangle). A query that returns "the size" with no unit in
// its name does not exist here - SDL needed a full major version
// (2.0.1 to 3.0, see the plan's own prior-art note) to correct exactly
// this ambiguity; this project starts already split.
//
// THE DERIVATION LIVES HERE, NOT IN EITHER BACKEND (sec. 5 risk 4 of
// the plan: "passa verde e esta quebrado"): on both platforms this
// project's CI runs today, buffer scale is 1 and DPI is 96 -
// pixel_size() and logical_size() would come out EQUAL no matter what
// formula a backend used, including the wrong one ("pixel_size just
// returns logical_size"). window_state_test.cpp proves the real
// formula with SYNTHETIC values (buffer_scale 2, dpi 144) that neither
// CI executor ever reports on its own.
//
// ONE FORMULA, TWO FACTORS: apply_buffer_scale() carries Wayland's
// integer wl_surface.preferred_buffer_scale (neutral value 1,
// meaningless on Win32 - left at 1 there); apply_dpi() carries Win32's
// GetDpiForWindow()/WM_DPICHANGED value (neutral value 96, Win32's own
// unscaled baseline, meaningless on Wayland - left at 96 there).
// pixel_size() is always `logical_size * buffer_scale * dpi / 96` -
// this reduces to the right answer using either platform's own factor
// alone, because the OTHER platform's factor always sits at neutral.

namespace glintfx::platform {

// The four boolean facts D-W5-3/D-W6a-20/D-W6b-51 keep as flags on
// window_state - distinct from the one-way close_requested() latch
// below (GODS_LAWS.md L-17: a sticky, never-reset fact and a togglable
// one are different subjects, not the same bit set).
//
// suspended (D-W6b-51, docs/plano-w6b-fatias-6-8.md): the system's own
// affirmative signal that this window is not being repainted right
// now - Wayland's xdg_toplevel_state SUSPENDED (xdg-shell.xml, `since`
// version 6, src/platform/wayland/window_configure_sequence.hpp's own
// header comment) or Win32's WM_SIZE/SIZE_MINIMIZED (src/platform/
// win32/window_message_route.cpp). It is the FIRST of the two criteria
// LOOP-RUN's own present_would_skip() probe (a later fatia) consults -
// see that fatia's own header comment for why a frame callback going
// silent alone is not enough on a compositor that never sends this bit
// (SDL#12156, read for the technique, not copied, GODS_LAWS.md L-29).
enum class window_state_bit : std::uint8_t {
    active,
    maximized,
    fullscreen,
    suspended,
};

// Plain, public-layout size (GODS_LAWS.md L-19's opacity clause is
// scoped to handles/subsystems WITH STATE - this is neither, the same
// exception glintfx::version and core/vec2.hpp's value types use).
struct window_size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class window_state {
  public:
    window_state() noexcept = default;

    // Sets the size in the SAME unit logical_size() reports (Wayland:
    // the xdg_toplevel configure's surface-local width/height, applied
    // directly; Win32: the win32 window_adapter, X-2, a later fatia,
    // converts WM_SIZE's pixel rectangle to this unit with
    // GetDpiForWindow BEFORE calling this - see this header's own
    // "ONE FORMULA, TWO FACTORS" paragraph for why the same setter
    // works for both).
    void apply_logical_size(std::uint32_t width, std::uint32_t height) noexcept;

    // Wayland's wl_surface.preferred_buffer_scale (v6, D-W6a-17); the
    // neutral value (1) until a backend ever calls this.
    void apply_buffer_scale(std::uint32_t scale) noexcept;

    // Win32's GetDpiForWindow()/WM_DPICHANGED (D-W6a-20); the neutral
    // value (96, Win32's own unscaled baseline) until a backend ever
    // calls this.
    void apply_dpi(std::uint32_t dpi) noexcept;

    void set_state(window_state_bit bit, bool on) noexcept;

    // One-way latch (sec. 5 risk 6 of the plan: "WM_CLOSE devolvido a
    // DefWindowProc"): once the system asks to close, close_requested()
    // stays true. There is no unset_close_request() - nothing in this
    // project's window layer is ever supposed to take that fact back
    // once the system has said it.
    void request_close() noexcept;

    [[nodiscard]] window_size logical_size() const noexcept;
    [[nodiscard]] window_size pixel_size() const noexcept;
    [[nodiscard]] bool state(window_state_bit bit) const noexcept;
    [[nodiscard]] bool close_requested() const noexcept;

  private:
    window_size m_logical_size;
    std::uint32_t m_buffer_scale = 1;
    std::uint32_t m_dpi = 96;
    bool m_active = false;
    bool m_maximized = false;
    bool m_fullscreen = false;
    bool m_suspended = false;
    bool m_close_requested = false;
};

} // namespace glintfx::platform
