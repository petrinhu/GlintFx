// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// platform/window/window.hpp - W-D' (docs/plano-w6a-janela.md fatia 6,
// GODS_LAWS.md L-26 "porta de mao unica"): the PUBLIC vocabulary a
// caller uses to describe and read back a window, frozen ahead of the
// concrete handle that will use it (gltfx_window itself is NOT in this
// header yet - see the "WHAT THIS HEADER DELIBERATELY DOES NOT FREEZE
// YET" paragraph below for why, in detail, since that is the single
// most important thing about this file).
//
// ============================================================
// THE DECISION THIS HEADER EXISTS TO PROTECT (decision 15, docs/
// plano-w6a-janela.md sec. 1, CTO, 05/09/2026)
// ============================================================
//
//   TWO SIZES, NEVER ONE, AND THE UNIT IS IN THE NAME FROM DAY ONE.
//
//   A window on a HiDPI display has a size in the window system's own
//   coordinate space (what the compositor/window manager measures it
//   by) and a DIFFERENT size in framebuffer pixels (what a consumer's
//   renderer has to actually paint into) - the two are equal only when
//   the scale factor is 1. gltfx_window (the handle this vocabulary
//   describes, landing in a later fatia) answers these as
//   logical_size() and pixel_size() - TWO DIFFERENTLY NAMED METHODS,
//   never a single unqualified size() a caller could misread as
//   either. gltfx_window_desc::logical_size below is this same
//   discipline applied to the OPEN-TIME request: the field that asks
//   for a starting size is named for its unit too, matching the
//   accessor it feeds exactly.
//
//   PRIOR ART (docs/plano-w6a-janela.md sec. 0, learned not copied,
//   GODS_LAWS.md L-29): SDL needed a full major version (2.0.1's
//   SDL_GL_GetDrawableSize() bolted on years after SDL_GetWindowSize()
//   already shipped, consolidated properly only in SDL3's
//   SDL_GetWindowSizeInPixels()) to correct exactly the ambiguity this
//   project is refusing to ever introduce. There is no
//   SDL_GetWindowSize()-shaped method anywhere in this vocabulary, on
//   purpose, and there never will be - GODS_LAWS.md L-26: this is a
//   porta de mao unica, and the absence of the ambiguous form IS the
//   mechanism, not a naming convention someone could "helpfully"
//   restore later.
//
// THE HANDLE ITSELF, gltfx_window BELOW, WAS DELIBERATELY LEFT OUT OF
// AN EARLIER VERSION OF THIS HEADER, AND IS NOW FROZEN (WL-WINDOW-
// HANDLE, docs/plano-w6a-janela.md fatias 6/9/11, absorbed by CTO
// decision into one fatia): the precondition the earlier version of
// this comment named for deferring it - "measured, not guessed... the
// Win32 window adapter does not exist anywhere in this tree yet" - is
// satisfied today (wayland_window_adapter AND win32_window_adapter both
// exist and work), and this fatia is the one that closes the gap. See
// gltfx_window's own class comment below for the frozen surface.
//
// PLAIN, PUBLIC LAYOUT (CONTRACT.md's own opacity exception for a
// value type whose STABLE LAYOUT IS the contract - the same exception
// glintfx::version, glintfx::gltfx_rgba and glintfx::vec2 already use):
// gltfx_window_size and gltfx_window_desc are data, not handles over
// state, so GODS_LAWS.md L-19's opacity clause does not reach them.
// gltfx_window itself is the OPPOSITE case - a handle WITH state,
// opaque PIMPL, the same shape gltfx_display (display.hpp, this
// directory) already has.

namespace glintfx {

// Forward-declared only: gltfx_window::open() below takes a `gltfx_
// display &` by reference, which needs no more than this to declare -
// the same "only a pointer/reference needs a forward declaration"
// discipline this project already applies to wl_display/wl_registry
// (src/platform/wayland/display_adapter.hpp) and to display_impl
// itself (display.hpp, this directory). Consumers that call gltfx_
// window::open() already include <glintfx/platform/window/display.hpp>
// for gltfx_display::open() itself, so this never leaves a caller with
// an incomplete type at the one call site that matters.
class gltfx_display;

// The one shape both logical_size() and pixel_size() share (a future
// gltfx_window's own accessors) - the UNIT is never carried by this
// type itself, only by the NAME of whichever method returned it or
// field that asked for it. Mirrors glintfx::platform::window_state's
// internal window_size field-for-field (src/platform/window/window_
// state.hpp) - two independent types on purpose (GODS_LAWS.md L-19:
// internal code never programs against a public GLINTFX_API type, and
// vice versa), kept in step by hand until a future fatia's own
// translation code needs both in the same expression.
struct gltfx_window_size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

static_assert(std::is_standard_layout_v<gltfx_window_size>,
              "GODS_LAWS.md L-19 item 3: gltfx_window_size's layout is the contract itself");
static_assert(std::is_trivially_copyable_v<gltfx_window_size>,
              "GODS_LAWS.md L-19 item 3: gltfx_window_size is a value type, safe to copy across "
              "the ABI boundary");

// The three boolean facts D-W5-3 keeps as flags on a future gltfx_
// window's own state query - the public mirror of glintfx::platform::
// window_state_bit (src/platform/window/window_state.hpp), same three
// names, same order, two independent enums for the same GODS_LAWS.md
// L-19 reason gltfx_window_size and window_size stay independent
// types above. Deliberately excludes close_requested (D-W5-3's own
// minimum keeps that as its own one-way latch, never a fourth bit
// here - window_state.hpp's own header comment already explains why a
// sticky, never-reset fact and a togglable one are different subjects).
enum class gltfx_window_state_bit : std::uint8_t {
    active,
    maximized,
    fullscreen,
};

// The open-time request (D-W5-4, renamed by decision 15): plain data a
// caller assembles before opening a window - never validated, never
// interpreted by this header itself (docs/api-conventions.md's own
// convention: a struct like this carries no behavior). An EMPTY title
// or application_id is always accepted (v1 never requires either); an
// invalid UTF-8 byte sequence in either field is a DIFFERENT, later
// concern (a future gltfx_window::open() rejects it the same way
// src/platform/window/window_desc_validation.hpp's internal validator
// already does for both backends alike, GODS_LAWS.md L-04) - not
// something this plain-data type enforces on construction.
//
// logical_size IS THE FIELD DECISION 15 RENAMED: never a bare `size`.
// The starting size is asked for in the SAME unit logical_size()
// (a future gltfx_window's own accessor) reports back - a compositor
// or window manager places a window at the size the caller requested
// in ITS OWN coordinate space, never at a framebuffer pixel count the
// caller would have had to know the scale factor in advance to compute.
struct gltfx_window_desc {
    std::string_view title;
    std::string_view application_id;
    gltfx_window_size logical_size;
};

// Opaque (GODS_LAWS.md L-19): the full layout - today, either a
// platform::wayland_window_adapter or a platform::win32_window_adapter,
// picked by src/platform/window/window_facade.cpp's own #if - is a
// private implementation detail, defined ONLY in window_facade.cpp. A
// consumer never sees this type; gltfx_window below carries only a
// pointer to one - the exact same shape display_impl/gltfx_display
// (display.hpp, this directory) already have.
struct window_impl;

// gltfx_window - WL-WINDOW-HANDLE (docs/plano-w6a-janela.md fatias
// 6/9/11, absorbed into one fatia by the CTO's own decision, porta de
// mão única, GODS_LAWS.md L-26): the public window handle this header
// deferred until both platforms had a concrete adapter to translate
// into (see this file's own top comment). Named-constructor idiom, the
// exact same shape gltfx_display::open() (display.hpp) already uses:
// open() is a FALLIBLE static factory returning gltfx_rslt<gltfx_
// window>, never a bare constructor that could fail - a caller either
// gets a gltfx_window that is genuinely open, or a gltfx_err and no
// object at all.
//
// ============================================================
// WHAT THIS FATIA FREEZES (write this down, it does not move later
// without breaking a porta de mão única):
// ============================================================
//
//   1. THE HANDLE IS A RESOURCE: movable only, never copyable - copying
//      a live window handle would hand two owners the same underlying
//      surface/HWND, and closing it twice is a use-after-free in
//      whichever backend's own bookkeeping opened it. Same reasoning
//      gltfx_display's own deleted copy members already document.
//
//   2. IT IS BORN FROM A DISPLAY, AND THE DISPLAY MUST OUTLIVE IT: a
//      gltfx_window never owns the gltfx_display it was opened
//      against - open() takes a `gltfx_display &`, not a value, and
//      keeps no reference of its own past the call (the underlying
//      adapter it wraps holds whatever ties it needs to the display's
//      OWN connection/shell, exactly like wayland_window_adapter and
//      win32_window_adapter already require of their own open()).
//      Destroying the gltfx_display before a gltfx_window opened from
//      it is UNDEFINED BEHAVIOR - this is a real precondition, told to
//      the caller here instead of left for them to discover.
//
//   3. THE DESCRIPTOR IS READ, NEVER STORED: open() copies whatever it
//      needs out of gltfx_window_desc (the title/application_id text
//      fields, specifically) before returning - the gltfx_window_desc
//      the caller passed can be destroyed, reused or mutated the
//      moment open() returns, the same "descriptor is plain data with
//      no behavior, and no lifetime tied to the handle it describes"
//      contract this header's own struct comment above already gives
//      gltfx_window_desc in isolation.
//
//   4. THE NAMES AND RETURN TYPES BELOW ARE THE FROZEN SURFACE - logical_
//      size()/pixel_size() (never a single unqualified size(), this
//      header's own "TWO SIZES" decision above), state(bit)/
//      close_requested() (the one-way latch, never folded into state()
//      itself - window_state.hpp's own reasoning, mirrored here),
//      set_title() (returning gltfx_rslt<void>, the same envelope
//      open() itself uses for a fallible, noexcept public call).
//
//   5. BOTH DIMENSIONS OF THE OPEN-TIME REQUEST MUST BE NON-ZERO. A
//      zero width or height is refused with `invalid_argument`
//      (`rejected_value` = `"logical_size"`) by the common validation
//      layer, before any backend sees the request, so the refusal is
//      identical on every platform by construction, not by
//      measurement. There is no "the system chooses" in this API:
//      xdg-shell has no such notion (a compositor's 0x0 configure
//      means "the client decides"), and Win32's `CW_USEDEFAULT` is a
//      Windows-only one. A library-chosen default size, if ever
//      wanted, will be an ADDITIVE extension of this rule, never a
//      reinterpretation of zero. Refusal proven by window_desc_
//      validation_test.cpp's own both_dimensions_zero_is_rejected/
//      zero_width_only_is_rejected/zero_height_only_is_rejected/
//      both_dimensions_nonzero_is_accepted (four cases, no container,
//      no platform guard) and, through the public API on both
//      systems, by window_parity_test.cpp's own refusal case.
//
// NOT CONGEALED HERE, AND BELONGS TO A LATER FATIA (W6b): input
// delivery, fractional-scale event handling beyond what window_state
// already derives, a graphics context, and any method beyond the ones
// listed above.
class gltfx_window {
  public:
    // Opens a window against an already-open `display` - GODS_LAWS.md
    // L-22: any refusal (an invalid title/application_id, a display
    // that is not open, a platform-specific failure) comes back as an
    // ordinary gltfx_err through this gltfx_rslt<gltfx_window>, never
    // an exception, never a half-open object. See this class's own
    // "WHAT THIS FATIA FREEZES" items 2 and 3 above for the two
    // preconditions on `display` and `desc` this call carries.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_window>
    open(gltfx_display &display, const gltfx_window_desc &desc) noexcept;

    // Move-only - see this class's own "WHAT THIS FATIA FREEZES" item 1
    // above.
    gltfx_window(const gltfx_window &) = delete;
    gltfx_window &operator=(const gltfx_window &) = delete;

    GLINTFX_API gltfx_window(gltfx_window &&other) noexcept;
    GLINTFX_API gltfx_window &operator=(gltfx_window &&other) noexcept;

    // Closes on scope exit - RAII, not something the caller has to
    // remember to invoke by hand. Safe on a moved-from instance.
    GLINTFX_API ~gltfx_window();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // TWO SIZES, NEVER ONE - this header's own top-of-file decision,
    // now answered for real instead of only vocabulary: logical_size()
    // is the window-system coordinate space; pixel_size() is the
    // framebuffer a consumer's renderer actually paints into. See
    // platform::window_state's own header comment (src/platform/
    // window/window_state.hpp) for the exact derivation formula both
    // backends share.
    [[nodiscard]] GLINTFX_API gltfx_window_size logical_size() const noexcept;
    [[nodiscard]] GLINTFX_API gltfx_window_size pixel_size() const noexcept;

    [[nodiscard]] GLINTFX_API bool state(gltfx_window_state_bit bit) const noexcept;

    // One-way latch - see gltfx_window_state_bit's own comment above
    // for why this is not a fourth bit on that enum instead.
    [[nodiscard]] GLINTFX_API bool close_requested() const noexcept;

    // Changes the title AFTER open() - see this class's own "WHAT THIS
    // FATIA FREEZES" item 4 above. Empty is a real, always-accepted
    // request (clears the title), the same as an empty gltfx_window_
    // desc::title at open() time.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> set_title(std::string_view title) noexcept;

  private:
    explicit gltfx_window(window_impl *impl) noexcept : m_impl(impl) {}

    window_impl *m_impl = nullptr;
};

} // namespace glintfx
