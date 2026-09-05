// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>

#include <wayland-client.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/input/seat_capabilities.hpp"
#include "platform/wayland/seat_adapter.hpp"

// seat_adapter_listener_test.cpp - WL-SEAT fatia S-B (docs/plano-w6a-
// janela.md fatia 12): the pure-listener half of wayland_seat_adapter,
// exercised by calling each static callback DIRECTLY with a NULL proxy
// (same technique window_adapter_listener_test.cpp already uses one
// directory over) - no compositor, no wayland_display_adapter::open().
// Both callbacks under test only ever write into seat_capabilities/
// std::string/a plain counter, never dereference the wl_seat* proxy
// itself (seat_adapter.cpp's own callback bodies) - which is exactly
// what makes a null proxy a safe argument here.
//
// "ONDE ESTA O PERIGO" item 1 of this fatia's own briefing - "as
// capacidades chegam por evento, e podem mudar depois... um teste que
// so verifique o primeiro anuncio nao prova que a mudanca e tratada" -
// is answered by
// capabilities_event_fired_twice_with_different_bitmasks_updates_in_place
// below: it calls wl_seat_capabilities() TWICE, with two different
// bitmasks, and checks the SECOND call's result is what capabilities()
// reports, proving a later re-announcement overwrites the first rather
// than only ever being observed once. The real-compositor half (does a
// live compositor actually SEND a second capabilities event mid-run) is
// declared out of reach for this onda in docs/plano-w6a-janela.md sec.
// 4 item 4 ("valor das capacidades do seat... impresso, nao exigido,
// ate medido") - tests/container/seat_test.cpp, inside the container
// (GODS_LAWS.md L-09), proves the bind against a REAL compositor and
// prints whatever it announced, exactly once, because a headless test
// compositor with no input devices attached has no mechanism this
// fixture's own short lifetime can use to trigger a SECOND capabilities
// event on demand.
//
// open()/close() themselves (the paths that DO call into a live
// compositor, including the version-aware wl_seat_release()/wl_seat_
// destroy() choice in close()) are proven for real by seat_test, inside
// the container - never by this file, same division of labor window_
// adapter_listener_test.cpp already documents for wayland_window_
// adapter.

namespace {

using glintfx::platform::seat_capability;
using glintfx::platform::wayland_seat_adapter;

} // namespace

GLINTFX_TEST(freshly_constructed_seat_adapter_reports_closed_with_no_capabilities) {
    wayland_seat_adapter adapter;

    GLINTFX_CHECK(!adapter.is_open());
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::touch));
    GLINTFX_CHECK(adapter.name().empty());
    GLINTFX_CHECK(adapter.last_change() == 0);
}

GLINTFX_TEST(capabilities_event_with_pointer_and_keyboard_leaves_touch_absent) {
    wayland_seat_adapter adapter;

    wayland_seat_adapter::wl_seat_capabilities(
        &adapter, nullptr, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);

    GLINTFX_CHECK(adapter.capabilities().has_capability(seat_capability::pointer));
    GLINTFX_CHECK(adapter.capabilities().has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::touch));
    GLINTFX_CHECK(adapter.last_change() == 1);
}

GLINTFX_TEST(capabilities_event_fired_twice_with_different_bitmasks_updates_in_place) {
    // "ONDE ESTA O PERIGO" item 1, proven directly - see this file's
    // own header comment above.
    wayland_seat_adapter adapter;

    wayland_seat_adapter::wl_seat_capabilities(&adapter, nullptr, WL_SEAT_CAPABILITY_POINTER);
    GLINTFX_CHECK(adapter.capabilities().has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::keyboard));

    wayland_seat_adapter::wl_seat_capabilities(
        &adapter, nullptr, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_TOUCH);

    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::pointer));
    GLINTFX_CHECK(adapter.capabilities().has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(adapter.capabilities().has_capability(seat_capability::touch));
    GLINTFX_CHECK(adapter.last_change() == 2);
}

GLINTFX_TEST(capabilities_event_with_zero_bitmask_clears_every_flag) {
    wayland_seat_adapter adapter;

    wayland_seat_adapter::wl_seat_capabilities(
        &adapter, nullptr,
        WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_TOUCH);
    wayland_seat_adapter::wl_seat_capabilities(&adapter, nullptr, 0);

    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!adapter.capabilities().has_capability(seat_capability::touch));
}

GLINTFX_TEST(name_event_sets_the_seat_name_and_counts_as_a_change) {
    wayland_seat_adapter adapter;

    wayland_seat_adapter::wl_seat_name(&adapter, nullptr, "seat0");

    GLINTFX_CHECK(adapter.name() == "seat0");
    GLINTFX_CHECK(adapter.last_change() == 1);
}

GLINTFX_TEST(name_event_with_null_pointer_leaves_the_name_empty_without_crashing) {
    // wayland.xml gives no guarantee wl_seat.name is ever sent - this
    // proves a null `name` argument (which no real compositor sends,
    // but nothing in the generated C ABI forbids) never dereferences a
    // null pointer.
    wayland_seat_adapter adapter;

    wayland_seat_adapter::wl_seat_name(&adapter, nullptr, nullptr);

    GLINTFX_CHECK(adapter.name().empty());
    GLINTFX_CHECK(adapter.last_change() == 1);
}

GLINTFX_TEST(freshly_constructed_seat_adapter_has_no_live_seat_pointer) {
    wayland_seat_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());
}
