// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

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
// WHAT THIS HEADER DELIBERATELY DOES NOT FREEZE YET, AND WHY (GODS_
// LAWS.md L-26 "se hesitar, nao inclua" applied honestly): the window
// HANDLE itself - a class with logical_size()/pixel_size()/state()/
// close_requested()/set_title() methods, opaque PIMPL over whichever
// backend opened it, the same shape gltfx_display (display.hpp, this
// directory) already has - is NOT declared in this header. Measured,
// not guessed, at the moment this file was written: WL-WINDOW's own
// concrete adapter (wayland_window_adapter, src/platform/wayland/
// window_adapter.hpp, fatia 8) already exists and works, but the Win32
// window adapter (X-2, docs/plano-w6a-janela.md fatia 9) does not
// exist anywhere in this tree yet. A gltfx_window class exported
// GLINTFX_API with a body wired to ONE platform and none for the other
// would either fail to compile for Windows the moment this header is
// used (blocking every other agent's Windows work mid-wave) or ship a
// declared-but-undefined symbol on that platform - exactly the
// "simbolo publico sem definicao" defect this onda already lost hours
// to once today (docs/plano-w6a-janela.md sec. 5, risk 1). Freezing
// the DATA VOCABULARY below costs nothing to add to later (GODS_LAWS.md
// L-26: a value type gains a consumer the moment something references
// it, never a reason by itself to leave it out) and is exactly what
// wayland_window_adapter's own header comment already expects of this
// fatia ("W-D's eventual translation is a field-for-field copy, not a
// redesign") - the HANDLE class and its open()-time translation into
// each backend's own internal descriptor (wayland_window_desc today,
// a future Win32 equivalent) are left for the fatia that lands once
// BOTH platforms have a concrete window adapter to translate into.
//
// PLAIN, PUBLIC LAYOUT (CONTRACT.md's own opacity exception for a
// value type whose STABLE LAYOUT IS the contract - the same exception
// glintfx::version, glintfx::gltfx_rgba and glintfx::vec2 already use):
// gltfx_window_size and gltfx_window_desc are data, not handles over
// state, so GODS_LAWS.md L-19's opacity clause does not reach them.

namespace glintfx {

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

} // namespace glintfx
