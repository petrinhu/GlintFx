// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dbt.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/input/seat_capabilities.hpp"
#include "platform/win32/device_change_message.hpp"
#include "platform/win32/raw_device_facts.hpp"
#include "platform/win32/seat_adapter.hpp"

// win32_seat_translation_test.cpp - Y-1 (docs/plano-w6a-janela.md
// fatia 13, "listas sinteticas" - the plan's own name for THIS file):
// win32_seat_adapter::translate() exercised directly against
// hand-built win32_raw_device_facts arrays (raw_device_facts.hpp) and
// digitizer bitmasks - no window, no RegisterDeviceNotificationW, no
// live device, the same "prove the pure half on its own" role tests/
// window_configure_sequence_test.cpp already plays for the Wayland
// side's own translation logic
// (docs/plano-w6a-janela.md fatia 4's own row). This is the Win32
// counterpart of fatia 12's seat_adapter_listener_test (proxy nulo) -
// GODS_LAWS.md L-04's "mecanismo pode diferir" pair, tests/parity_
// aliases.txt entry to follow once fatia 12 lands (this file's own
// commit message names that as still pending - see this project's own
// aliases-file rule: "confirme que o par realmente roda nos dois
// lados HOJE" before a line is added there).
//
// win32_seat_adapter is compiled a SECOND time directly into this
// executable's own object set (tests/CMakeLists.txt), the same
// technique win32_display_connect_test.cpp already documents for
// win32_display_adapter: glintfx's own shared build hides everything
// without GLINTFX_API, so a separate executable linking only against
// glintfx::glintfx could never resolve translate() across that
// boundary. display_adapter.cpp is linked alongside it purely because
// seat_adapter.hpp includes display_adapter.hpp (win32_seat_adapter::
// open() takes a `const win32_display_adapter &`) - this file's own
// cases never call open(), only the static translate() function, so
// display_adapter.cpp's own symbols are pulled in but never exercised
// here.

using glintfx::platform::classify_device_change;
using glintfx::platform::device_change_kind;
using glintfx::platform::seat_capabilities;
using glintfx::platform::seat_capability;
using glintfx::platform::win32_raw_device_facts;
using glintfx::platform::win32_seat_adapter;

// NID_INTEGRATED_TOUCH/NID_EXTERNAL_TOUCH/NID_INTEGRATED_PEN: SM_
// DIGITIZER's own bitmask (learn.microsoft.com/windows/win32/api/
// winuser/nf-winuser-getsystemmetrics, Remarks - the exact page
// src/platform/win32/seat_adapter.cpp's own header comment already
// cites for these three values). Redeclared here, at test scope, the
// same "spell out the number's origin at the point of use" reasoning
// tests/win32_runner_probe_test.cpp already applies to its own GL
// token constants - <windows.h> already defines the real
// NID_INTEGRATED_TOUCH/NID_EXTERNAL_TOUCH/NID_INTEGRATED_PEN macros,
// so this file uses THOSE directly rather than shadowing them; this
// comment exists only to name the source for whoever reads the
// bitmask literals below without cross-referencing seat_adapter.cpp.

GLINTFX_TEST(translate_reports_nothing_present_for_an_empty_device_list_and_zero_bitmask) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate({}, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

// D-WS-5 (SF-3): keyboard_key_count is IGNORED for RIM_TYPEMOUSE -
// translate_keeps_pointer_unfiltered_by_key_count below proves the same
// fact from the other direction (a nonzero count on a MOUSE entry).
GLINTFX_TEST(translate_reports_pointer_present_from_a_single_rim_typemouse_entry) {
    win32_raw_device_facts devices[1]{{RIM_TYPEMOUSE, 0}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 0, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

// RIM_TYPEHID entries this project's three-value seat_capability enum
// has no slot for (seat_adapter.cpp's own translate() comment, "default:
// counted by nothing here") must not be silently folded into pointer
// or keyboard - the mutation this case exists to catch is exactly
// that fold.
GLINTFX_TEST(translate_ignores_rim_typehid_entries_for_pointer_and_keyboard) {
    win32_raw_device_facts devices[1]{{RIM_TYPEHID, 0}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
}

GLINTFX_TEST(translate_reports_touch_present_when_the_integrated_touch_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate({}, NID_INTEGRATED_TOUCH, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_touch_present_when_the_external_touch_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate({}, NID_EXTERNAL_TOUCH, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

// The mutation this case exists to catch: folding NID_INTEGRATED_PEN
// (a STYLUS digitizer, a different question than "touch" -
// seat_adapter.hpp's own header comment) into seat_capability::touch
// just because both live in the same SM_DIGITIZER bitmask.
GLINTFX_TEST(translate_reports_touch_absent_when_only_the_pen_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate({}, NID_INTEGRATED_PEN, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_all_three_capabilities_present_together) {
    win32_raw_device_facts devices[3]{{RIM_TYPEMOUSE, 0}, {RIM_TYPEKEYBOARD, 32}, {RIM_TYPEHID, 0}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, NID_INTEGRATED_TOUCH | NID_READY, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

// translate() overwrites `out` from scratch rather than only setting
// bits already true - a caller re-using the same seat_capabilities
// across a routed WM_DEVICECHANGE recompute (win32_seat_adapter::
// recompute_capabilities()) depends on a PREVIOUSLY present device
// that has since vanished from the list being cleared, not left
// stale.
GLINTFX_TEST(translate_clears_a_previously_present_capability_no_longer_in_the_list) {
    seat_capabilities capabilities;
    capabilities.set_capability(seat_capability::pointer, true);
    capabilities.set_capability(seat_capability::keyboard, true);
    capabilities.set_capability(seat_capability::touch, true);

    win32_seat_adapter::translate({}, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

// D-WS-5 (SF-3, win-seat.md sec. 1.6/3): the udev/systemd parity rule -
// a keyboard-class device must positively report AT LEAST 32 keys
// (raw_device_facts.hpp's own header comment) to count as a keyboard.
// The mutation this case exists to catch: dropping the key-count filter
// entirely (treating every RIM_TYPEKEYBOARD as present, the pre-SF-3
// behavior this test would NOT have caught).
GLINTFX_TEST(
    translate_rejects_a_keyboard_class_device_with_fewer_keys_than_the_wayland_rule_needs) {
    win32_raw_device_facts devices[1]{{RIM_TYPEKEYBOARD, 12}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
}

// "nao invente ausencia" (GODS_LAWS.md L-04): a driver that never
// answers GetRawInputDeviceInfoW (keyboard_key_count == 0, query_
// keyboard_key_count()'s own degrade-to-0 path, seat_adapter.cpp) must
// count as PRESENT, never as excluded for lack of proof. The mutation
// this case exists to catch: treating 0 as "fewer than 32" instead of
// "unknown".
GLINTFX_TEST(translate_accepts_a_keyboard_class_device_with_unknown_key_count) {
    win32_raw_device_facts devices[1]{{RIM_TYPEKEYBOARD, 0}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 0, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::keyboard));
}

// The exact fronteira, and one step past it (GODS_LAWS.md L-43: testar
// sempre um passo alem da fronteira) - the mutation this case exists to
// catch: using `>` instead of `>=` at the minimum.
GLINTFX_TEST(translate_accepts_a_keyboard_at_exactly_the_minimum_key_count) {
    win32_raw_device_facts at_minimum[1]{{RIM_TYPEKEYBOARD, 32}};
    seat_capabilities capabilities_at_minimum;
    win32_seat_adapter::translate(at_minimum, 0, capabilities_at_minimum);
    GLINTFX_CHECK(capabilities_at_minimum.has_capability(seat_capability::keyboard));

    win32_raw_device_facts one_below_minimum[1]{{RIM_TYPEKEYBOARD, 31}};
    seat_capabilities capabilities_one_below;
    win32_seat_adapter::translate(one_below_minimum, 0, capabilities_one_below);
    GLINTFX_CHECK(!capabilities_one_below.has_capability(seat_capability::keyboard));
}

// The mutation this case exists to catch: applying the key-count filter
// to RIM_TYPEMOUSE too (a mouse entry never carries a meaningful
// keyboard_key_count, and this project's own recompute_capabilities()
// never queries one for a mouse - seat_adapter.cpp's own query_
// keyboard_key_count() comment).
GLINTFX_TEST(translate_keeps_pointer_unfiltered_by_key_count) {
    win32_raw_device_facts devices[1]{{RIM_TYPEMOUSE, 0}};

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 0, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
}

// classify_device_change() (device_change_message.hpp/.cpp) - SF-1 of
// WIN-SEAT's 09/09/2026 reopening (win-seat.md sec. 1.3/3.1, D-090918):
// the pure half of deciding what a WM_DEVICECHANGE message means, fed
// synthetic (WPARAM, DEV_BROADCAST_HDR*) pairs, no window, no live
// registration at all - the counterpart of translate()'s own "pure
// half" role, for the OTHER mechanism win32_seat_adapter now uses.

// Guard 1 (win-seat.md sec. 1.3): DBT_DEVNODES_CHANGED and other
// unregistered broadcasts carry no block at all - a null header must
// never be dereferenced, regardless of which wParam code arrives with
// it. The mutation this case exists to catch: removing the null-header
// guard in classify_device_change() (a real deref-of-null in
// production, not just a test artifact).
GLINTFX_TEST(classify_ignores_broadcast_wparam_and_null_block) {
    GLINTFX_CHECK(classify_device_change(DBT_DEVNODES_CHANGED, nullptr) ==
                  device_change_kind::none);
    GLINTFX_CHECK(classify_device_change(DBT_DEVICEARRIVAL, nullptr) == device_change_kind::none);
}

// Guard 2 (win-seat.md sec. 1.3): even a non-null header may name a
// broadcast type other than DBT_DEVTYP_DEVICEINTERFACE (e.g.
// DBT_DEVTYP_VOLUME) - this adapter registered interest in device
// INTERFACE changes only, so a differently-typed block must classify as
// none even though it carries a recognized wParam. The mutation this
// case exists to catch: comparing wParam alone and ignoring
// dbch_devicetype (trading the setup-class-vs-interface-class trap,
// learn.microsoft.com/windows-hardware/drivers/install/comparison-of-
// setup-classes-and-interface-classes, for a type-blind one).
GLINTFX_TEST(classify_reports_arrival_and_removal_for_device_interface_blocks) {
    DEV_BROADCAST_HDR device_interface_header{};
    device_interface_header.dbch_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    GLINTFX_CHECK(classify_device_change(DBT_DEVICEARRIVAL, &device_interface_header) ==
                  device_change_kind::arrival);
    GLINTFX_CHECK(classify_device_change(DBT_DEVICEREMOVECOMPLETE, &device_interface_header) ==
                  device_change_kind::removal);

    DEV_BROADCAST_HDR volume_header{};
    volume_header.dbch_devicetype = DBT_DEVTYP_VOLUME;

    GLINTFX_CHECK(classify_device_change(DBT_DEVICEARRIVAL, &volume_header) ==
                  device_change_kind::none);
}

#endif // defined(_WIN32)
