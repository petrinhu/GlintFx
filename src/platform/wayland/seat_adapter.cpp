// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/seat_adapter.hpp"

#include <wayland-client.h>

#include <new>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/global_catalog.hpp"

// seat_adapter.cpp - see seat_adapter.hpp's own header comment for
// scope. wl_seat_capabilities()/wl_seat_name() below are called back
// by libwayland-client's own C event-dispatch machinery - GODS_LAWS.md
// L-22: no exception may unwind across that C stack frame. Both only
// ever write into seat_capabilities (noexcept setters, platform/input/
// seat_capabilities.cpp) and this class's own plain fields - the same
// "pure reads/writes of a value type, never a Wayland request" shape
// wayland_window_adapter's own callbacks already document one
// directory over. m_name = name is the one line in wl_seat_name() that
// CAN throw std::bad_alloc despite this function's own noexcept.
//
// CONSERTO (varredura de 07/09/2026, mesma familia de bugs de gfx_
// open_only_fixation.cpp): unlike wayland_window_adapter::apply_desc()
// /set_title() (both HAVE a gltfx_rslt<void> to carry the honest
// failure through, and were guarded in the same sweep), wl_seat_name()
// is a bare `void` C-callback with no error channel at all - the same
// shape global_catalog.hpp's own insert() exists to give an OBSERVABLE
// signal for, but this signature cannot be changed (libwayland's own
// wl_seat_listener.name function-pointer type dictates it). Guarded
// below: a failed assignment simply leaves `m_name` at whatever it
// held before this call, the same "left exactly as it was" guarantee
// global_catalog.hpp's own insert() already documents for its own
// internal failure - never a guess at the compositor-supplied name.

namespace glintfx::platform {

namespace {

constexpr wl_seat_listener kSeatListener = {
    .capabilities = &wayland_seat_adapter::wl_seat_capabilities,
    .name = &wayland_seat_adapter::wl_seat_name,
};

} // namespace

wayland_seat_adapter::~wayland_seat_adapter() { close(); }

void wayland_seat_adapter::wl_seat_capabilities(void *data, wl_seat * /*seat*/,
                                                std::uint32_t capabilities) noexcept {
    auto *self = static_cast<wayland_seat_adapter *>(data);
    self->m_capabilities.set_capability(seat_capability::pointer,
                                        (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0);
    self->m_capabilities.set_capability(seat_capability::keyboard,
                                        (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0);
    self->m_capabilities.set_capability(seat_capability::touch,
                                        (capabilities & WL_SEAT_CAPABILITY_TOUCH) != 0);
    ++self->m_last_change;
}

void wayland_seat_adapter::wl_seat_name(void *data, wl_seat * /*seat*/, const char *name) noexcept {
    auto *self = static_cast<wayland_seat_adapter *>(data);
    try {
        self->m_name = (name != nullptr) ? name : "";
    } catch (const std::bad_alloc &) {
        return; // m_name left exactly as it was - see this file's own header comment.
    }
    ++self->m_last_change;
}

gltfx_rslt<void> wayland_seat_adapter::open(wayland_display_adapter &connection) noexcept {
    // "ONDE ESTA O PERIGO" item 1, applied to the bind refusal path
    // (same "refusal, not a null pointer three calls later" shape
    // wayland_shell_adapter::open() already documents for wl_
    // compositor/xdg_wm_base): a compositor that never announces
    // wl_seat at all is a real, nameable environment, never a silent
    // null wl_seat* three calls later.
    const wayland_global *seat_global = connection.globals().find_by_interface("wl_seat");
    if (seat_global == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::not_found).with_rejected_value("wl_seat"));
    }

    gltfx_rslt<void *> seat_proxy = connection.bind(*seat_global, wl_seat_interface, 5);
    if (seat_proxy.has_error()) {
        return gltfx_rslt<void>::err(seat_proxy.error());
    }

    m_seat = static_cast<wl_seat *>(seat_proxy.value());
    wl_seat_add_listener(m_seat, &kSeatListener, this);
    return gltfx_rslt<void>::ok();
}

void wayland_seat_adapter::close() noexcept {
    if (m_seat != nullptr) {
        // VERSION-AWARE TEARDOWN - see this method's own declaration
        // comment in seat_adapter.hpp for why a bound version below 5
        // (a compositor offering less, clamped by wayland_display_
        // adapter::bind()) must fall back to wl_seat_destroy() instead
        // of the newer wl_seat_release() request.
        if (wl_seat_get_version(m_seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
            wl_seat_release(m_seat);
        } else {
            wl_seat_destroy(m_seat);
        }
        m_seat = nullptr;
    }
}

} // namespace glintfx::platform
