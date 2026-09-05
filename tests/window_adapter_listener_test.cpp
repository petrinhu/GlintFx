// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>

#include <wayland-client.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/window_adapter.hpp"
#include "platform/window/window_state.hpp"

// window_adapter_listener_test.cpp - WL-WINDOW fatia W-E (docs/plano-
// w6a-janela.md fatia 8): the pure-listener half of wayland_window_
// adapter, exercised by calling each static callback DIRECTLY with a
// NULL proxy (this fatia's own briefing: "callbacks com proxy nulo") -
// no compositor, no wayland_display_adapter::open(), no wayland_shell_
// adapter::open(). Every callback under test only ever reads its
// arguments and writes into window_state/window_configure_sequence -
// it never dereferences the wl_surface*/xdg_surface*/xdg_toplevel*
// proxy itself (window_adapter.cpp's own callback bodies: see each
// one's header comment in window_adapter.hpp) - which is exactly what
// makes a null proxy a SAFE argument here, the same "no real Wayland
// request fires" property window_configure_sequence_test already has
// one directory over.
//
// wl_array IS constructed for real (this file includes <wayland-
// client.h> for that reason alone) - it is a plain POD struct
// (size/alloc/data), not an opaque proxy, and xdg_toplevel_configure()
// reads straight out of its `data`/`size` fields the same way a real
// xdg_toplevel.configure event's marshalled argument would.
//
// The open()/wait_first_configure() path THIS test does not exercise -
// the one that actually calls into a live compositor, including the
// ack_configure() call this fatia's own mutation targets - is proven
// for real by window_smoke, inside the container (GODS_LAWS.md L-09,
// tests/container/window_smoke.cpp).

namespace {

using glintfx::platform::wayland_window_adapter;
using glintfx::platform::window_state_bit;

// XDG_TOPLEVEL_STATE_* values from xdg-shell.xml's own `state` enum
// (window_configure_sequence.hpp already hardcodes the same three
// numbers this project tracks - see that header's own comment for
// why the raw integer is duplicated here rather than shared: this file
// deliberately stays free of the generated xdg-shell-client-protocol.h
// enum, proving the numbers AGREE from the outside, not by including
// the same header on both sides).
constexpr std::int32_t kStateMaximized = 1;
constexpr std::int32_t kStateFullscreen = 2;
constexpr std::int32_t kStateActivated = 4;

wl_array make_states(std::int32_t *storage, std::size_t count) {
    wl_array states{};
    states.size = count * sizeof(std::int32_t);
    states.alloc = states.size;
    states.data = storage;
    return states;
}

} // namespace

GLINTFX_TEST(toplevel_then_surface_configure_applies_size_and_activated) {
    wayland_window_adapter adapter;

    std::int32_t storage[1] = {kStateActivated};
    wl_array states = make_states(storage, 1);

    wayland_window_adapter::xdg_toplevel_configure(&adapter, nullptr, 640, 480, &states);
    wayland_window_adapter::xdg_surface_configure(&adapter, nullptr, 1);

    GLINTFX_CHECK(adapter.state().logical_size().width == 640);
    GLINTFX_CHECK(adapter.state().logical_size().height == 480);
    GLINTFX_CHECK(adapter.state().state(window_state_bit::active));
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::maximized));
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::fullscreen));
}

GLINTFX_TEST(preferred_buffer_scale_two_doubles_pixel_size) {
    wayland_window_adapter adapter;

    wayland_window_adapter::xdg_toplevel_configure(&adapter, nullptr, 640, 480, nullptr);
    wayland_window_adapter::xdg_surface_configure(&adapter, nullptr, 1);

    wayland_window_adapter::wl_surface_preferred_buffer_scale(&adapter, nullptr, 2);

    GLINTFX_CHECK(adapter.state().logical_size().width == 640);
    GLINTFX_CHECK(adapter.state().pixel_size().width == 1280);
    GLINTFX_CHECK(adapter.state().pixel_size().height == 960);
}

GLINTFX_TEST(zero_by_zero_configure_keeps_the_existing_size) {
    // window_configure_sequence.hpp's own "0x0 rule": a 0x0 configure
    // means "keep whatever size you already have", never "resize to
    // zero" - window_adapter.cpp's own xdg_surface_configure() honors
    // this by simply not touching logical_size when has_size is false.
    wayland_window_adapter adapter;

    wayland_window_adapter::xdg_toplevel_configure(&adapter, nullptr, 640, 480, nullptr);
    wayland_window_adapter::xdg_surface_configure(&adapter, nullptr, 1);

    wayland_window_adapter::xdg_toplevel_configure(&adapter, nullptr, 0, 0, nullptr);
    wayland_window_adapter::xdg_surface_configure(&adapter, nullptr, 2);

    GLINTFX_CHECK(adapter.state().logical_size().width == 640);
    GLINTFX_CHECK(adapter.state().logical_size().height == 480);
}

GLINTFX_TEST(configure_applies_maximized_and_fullscreen_without_activated) {
    wayland_window_adapter adapter;

    std::int32_t storage[2] = {kStateMaximized, kStateFullscreen};
    wl_array states = make_states(storage, 2);

    wayland_window_adapter::xdg_toplevel_configure(&adapter, nullptr, 800, 600, &states);
    wayland_window_adapter::xdg_surface_configure(&adapter, nullptr, 7);

    GLINTFX_CHECK(adapter.state().state(window_state_bit::maximized));
    GLINTFX_CHECK(adapter.state().state(window_state_bit::fullscreen));
    GLINTFX_CHECK(!adapter.state().state(window_state_bit::active));
}

GLINTFX_TEST(toplevel_close_event_sets_the_one_way_close_latch) {
    wayland_window_adapter adapter;
    GLINTFX_CHECK(!adapter.state().close_requested());

    wayland_window_adapter::xdg_toplevel_close(&adapter, nullptr);

    GLINTFX_CHECK(adapter.state().close_requested());
}

GLINTFX_TEST(surface_enter_and_leave_are_harmless_named_noops) {
    // D-W6a-17's own header comment on this fatia's callbacks: output
    // enter/leave tracking is WL-SCALE's job, a later fatia - these two
    // callbacks do nothing today, and this case proves "nothing"
    // includes "no crash on a null wl_output*", not just "no state
    // change".
    wayland_window_adapter adapter;

    wayland_window_adapter::wl_surface_enter(&adapter, nullptr, nullptr);
    wayland_window_adapter::wl_surface_leave(&adapter, nullptr, nullptr);

    GLINTFX_CHECK(!adapter.state().close_requested());
    GLINTFX_CHECK(adapter.state().logical_size().width == 0);
}

GLINTFX_TEST(freshly_constructed_adapter_reports_closed) {
    wayland_window_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());
    GLINTFX_CHECK(adapter.surface() == nullptr);
}
