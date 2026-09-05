// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/input/seat_capabilities.hpp"

// seat_capabilities_test.cpp - S-A' (docs/plano-w6a-janela.md fatia 4):
// four cases proving seat_capabilities is a plain, independent set of
// three flags - default empty, one flag set, several set at once
// without disturbing each other, and one cleared back to absent. S-B
// (Wayland, fatia 12) and Y-1 (Win32, fatia 13) are the two mechanisms
// that will call set_capability(); neither exists yet, and this file
// tests the shared type on its own, the same way window_state_test.cpp
// tests window_state without any backend.

using glintfx::platform::seat_capabilities;
using glintfx::platform::seat_capability;

GLINTFX_TEST(default_seat_capabilities_has_no_device_class_present) {
    const seat_capabilities capabilities;

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(setting_pointer_present_leaves_the_other_two_absent) {
    seat_capabilities capabilities;
    capabilities.set_capability(seat_capability::pointer, true);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(setting_keyboard_and_touch_together_does_not_affect_pointer) {
    seat_capabilities capabilities;
    capabilities.set_capability(seat_capability::keyboard, true);
    capabilities.set_capability(seat_capability::touch, true);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(clearing_a_previously_set_capability_reports_it_absent_again) {
    seat_capabilities capabilities;
    capabilities.set_capability(seat_capability::pointer, true);
    capabilities.set_capability(seat_capability::pointer, false);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
}
