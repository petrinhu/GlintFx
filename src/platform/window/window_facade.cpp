// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cassert>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/port/window_adapter_port.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_desc_validation.hpp"
#include "platform/window/window_impl.hpp"
#include "platform/window/window_state.hpp"

#if defined(_WIN32)
#include "platform/win32/selected_window_adapter.hpp"
#else
#include "platform/wayland/selected_window_adapter.hpp"
#endif

// window_facade.cpp - WL-WINDOW-HANDLE (docs/plano-w6a-janela.md
// fatias 6/9/11, absorbed into one fatia by the CTO's own decision):
// the ONE translation unit that knows what glintfx::window_impl
// actually IS - the exact same "allocation and deallocation on the
// SAME side of the boundary" shape display_facade.cpp already gives
// glintfx::display_impl, one file over. Every other TU in this
// library, and every consumer, only ever sees the opaque glintfx::
// window_impl declared in include/glintfx/platform/window/window.hpp.
//
// selected_window_adapter CHOSEN BY #if defined(_WIN32), NOT BY CMAKE
// SUBDIRECTORY SELECTION: same reasoning display_facade.cpp's own
// header comment already gives for itself - this file is compiled on
// every platform (src/platform/window/CMakeLists.txt adds it
// unconditionally), so the per-platform choice happens at the
// preprocessor level, inside the file, not by which directory even
// entered the build.
//
// open()'S OWN #if BRANCH IS WHERE THE TWO PLATFORMS' GENUINELY
// DIFFERENT open() SHAPES MEET (window_adapter_port.hpp's own header
// comment: "porta gorda", deliberately excluded from the concept) -
// wayland_window_adapter::open() needs the connection AND an already-
// open shell; win32_window_adapter::open() needs only the connection.
// Both branches translate the SAME public gltfx_window_desc into each
// backend's own internal descriptor, field for field - wayland_window_
// adapter.hpp's own header comment already promised this translation
// would be "a field-for-field copy, not a redesign" the day both
// platforms had a concrete adapter to translate into.
//
// DISPLAY-PASSKEY (this file's own use of display.hpp's display_
// internal_access::get(), replacing an earlier gltfx_display::
// internal_impl() public method): this file, being one of the two
// TRUSTED siblings display_internal_access names by comment, links
// fine - it sits on the LIBRARY's own side of the .so/.dll boundary,
// the same side display_facade.cpp defines display_internal_access::
// get() on. The NEGATIVE proof (a standalone consumer TU, given only
// the public headers, calling glintfx::display_internal_access::get()
// itself and FAILING TO LINK with an undefined-reference error against
// the public glintfx::glintfx target) lives outside this source tree,
// in tests/container/_arch_ports_src/ as a disposable probe compiled
// and torn down by the same mutation-testing session that measured it
// - the same "sabotage a copy, capture the literal red/green output,
// never leave the mutation in the tracked tree" shape GODS_LAWS.md
// L-27 already requires, applied here to a NEGATIVE case (compiling
// on purpose is the RED, failing to link is the GREEN) instead of the
// usual positive one.

namespace glintfx {

// window_impl itself moved to window_impl.hpp (GL-CONTEXT, D-W6b-25) -
// see that header's own comment for why gl_context_facade.cpp now
// needs to share this same definition, and window.hpp's own window_
// internal_access passkey for how it reaches it from outside this TU.

namespace {

static_assert(platform::window_adapter_port<platform::selected_window_adapter>,
              "selected_window_adapter must satisfy window_adapter_port - GODS_LAWS.md L-19/"
              "L-40: gltfx_window's own facade wraps it directly, and a selection missing "
              "set_title()/state()/is_open()/close() has to fail to COMPILE here, never link a "
              "window handle whose set_title() silently does nothing on one platform");

// gltfx_window_state_bit -> platform::window_state_bit, the same
// "public mirror, kept in step by hand" pairing window.hpp's own
// header comment already documents for the two enums existing
// independently (GODS_LAWS.md L-19: internal code never programs
// against a public GLINTFX_API type, and vice versa). A `default:`
// case is deliberately absent: adding a fourth gltfx_window_state_bit
// enumerator without updating this switch is a missing-case compiler
// WARNING (-Wswitch, -Werror project-wide, GODS_LAWS.md L-23) on this
// exact line, not a silent fallthrough to some arbitrary native bit.
[[nodiscard]] platform::window_state_bit to_native_bit(gltfx_window_state_bit bit) noexcept {
    switch (bit) {
    case gltfx_window_state_bit::active:
        return platform::window_state_bit::active;
    case gltfx_window_state_bit::maximized:
        return platform::window_state_bit::maximized;
    case gltfx_window_state_bit::fullscreen:
        return platform::window_state_bit::fullscreen;
    }
    // Unreachable given the switch above covers every enumerator
    // (GODS_LAWS.md L-22: no exception crosses ANY boundary in this
    // file, public or internal) - the same "closed enum, closed
    // switch" shape this project's own window_configure_sequence.cpp
    // already uses for a *_STATE_* code space, applied here to an
    // enum class instead of raw protocol integers.
    return platform::window_state_bit::active;
}

[[nodiscard]] gltfx_window_size to_public_size(platform::window_size size) noexcept {
    return gltfx_window_size{size.width, size.height};
}

} // namespace

gltfx_rslt<gltfx_window> gltfx_window::open(gltfx_display &display,
                                            const gltfx_window_desc &desc) noexcept {
    // WHAT THIS FATIA FREEZES item 2 (window.hpp's own class comment):
    // the display must already be open - refused as invalid_argument,
    // the same precondition shape win32_window_adapter::open() already
    // uses for the identical dependency one layer down.
    if (!display.is_open()) {
        return gltfx_rslt<gltfx_window>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("display"));
    }

    // WINDOW-SIZE-REFUSE (docs/plano-w6a-janela.md, WL-WINDOW-HANDLE):
    // called ONCE, here, before either backend ever sees the request -
    // this is what makes the refusal identical on every platform BY
    // CONSTRUCTION, not by each backend separately choosing to reject
    // it. See window_desc_validation.hpp's own header comment on this
    // function, and window.hpp's own "WHAT THIS FATIA FREEZES" item 5.
    if (const gltfx_rslt<void> size_ok = platform::validate_window_logical_size(
            desc.logical_size.width, desc.logical_size.height);
        size_ok.has_error()) {
        return gltfx_rslt<gltfx_window>::err(size_ok.error());
    }

    display_impl *display_impl_ptr = display_internal_access::get(display);
    // display.is_open() already proved m_impl != nullptr above (is_
    // open() itself dereferences it) - this assert documents that
    // invariant at THIS call site too, the same "precondition, not a
    // recoverable failure" shape gltfx_display::pump_events() already
    // carries for the identical class of bug (a moved-from/null impl).
    assert(display_impl_ptr != nullptr &&
           "gltfx_window::open() called with a gltfx_display that reported is_open() but has a "
           "null internal_impl() - precondition violated, not a recoverable error");

#if defined(_WIN32)
    const platform::win32_window_desc native_desc{
        .logical_width = desc.logical_size.width,
        .logical_height = desc.logical_size.height,
        .title = desc.title,
        .application_id = desc.application_id,
    };
    platform::win32_window_adapter adapter;
    const gltfx_rslt<void> opened =
        adapter.open(display_impl_ptr->connection.adapter(), native_desc);
#else
    const platform::wayland_window_desc native_desc{
        .logical_width = desc.logical_size.width,
        .logical_height = desc.logical_size.height,
        .title = desc.title,
        .application_id = desc.application_id,
    };
    platform::wayland_window_adapter adapter;
    const gltfx_rslt<void> opened =
        adapter.open(display_impl_ptr->connection.adapter(), display_impl_ptr->shell, native_desc);
#endif

    if (opened.has_error()) {
        return gltfx_rslt<gltfx_window>::err(opened.error());
    }

    // window_impl now carries a second field (D-W6b-25's own fixed_
    // open_only_gfx_options, window_impl.hpp) - spelled out explicitly
    // (never a bare `{std::move(adapter)}` relying on the field's own
    // default member initializer) for the SAME -Wmissing-field-
    // initializers reason display_facade.cpp's own aggregate-init
    // comment already documents for display_impl's `shell` member.
    auto *impl = new (std::nothrow) window_impl{std::move(adapter), std::nullopt};
    if (impl == nullptr) {
        return gltfx_rslt<gltfx_window>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    return gltfx_rslt<gltfx_window>::ok(gltfx_window(impl));
}

gltfx_window::gltfx_window(gltfx_window &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_window &gltfx_window::operator=(gltfx_window &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

gltfx_window::~gltfx_window() { delete m_impl; }

bool gltfx_window::is_open() const noexcept {
    return m_impl != nullptr && m_impl->adapter.is_open();
}

gltfx_window_size gltfx_window::logical_size() const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_window::logical_size() called on a moved-from window - the object no longer "
           "owns an adapter");
    return to_public_size(m_impl->adapter.state().logical_size());
}

gltfx_window_size gltfx_window::pixel_size() const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_window::pixel_size() called on a moved-from window - the object no longer owns "
           "an adapter");
    return to_public_size(m_impl->adapter.state().pixel_size());
}

bool gltfx_window::state(gltfx_window_state_bit bit) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_window::state() called on a moved-from window - the object no longer owns an "
           "adapter");
    return m_impl->adapter.state().state(to_native_bit(bit));
}

bool gltfx_window::close_requested() const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_window::close_requested() called on a moved-from window - the object no longer "
           "owns an adapter");
    return m_impl->adapter.state().close_requested();
}

gltfx_rslt<void> gltfx_window::set_title(std::string_view title) noexcept {
    assert(m_impl != nullptr &&
           "gltfx_window::set_title() called on a moved-from window - the object no longer owns "
           "an adapter");
    return m_impl->adapter.set_title(title);
}

// window_internal_access::get() - the ONLY definition of this symbol
// in the whole library, the exact same reasoning display_internal_
// access::get() (display_facade.cpp) already documents for itself: a
// consumer's translation unit sees only the declaration in the public
// window.hpp, never this body, so it cannot compile the access itself
// - it can only ask the LINKER for a symbol this project deliberately
// never exports. No GLINTFX_API on this line, for the identical reason.
window_impl *window_internal_access::get(gltfx_window &window) noexcept { return window.m_impl; }

} // namespace glintfx
