// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/input/seat_capabilities.hpp"
#include "platform/win32/seat_adapter.hpp"

// win32_seat_translation_test.cpp - Y-1 (docs/plano-w6a-janela.md
// fatia 13, "listas sinteticas" - the plan's own name for THIS file):
// win32_seat_adapter::translate() exercised directly against
// hand-built RAWINPUTDEVICELIST arrays and digitizer bitmasks - no
// window, no RegisterRawInputDevices, no live device, the same "prove
// the pure half on its own" role tests/window_configure_sequence_
// test.cpp already plays for the Wayland side's own translation logic
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

using glintfx::platform::seat_capabilities;
using glintfx::platform::seat_capability;
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
    win32_seat_adapter::translate(nullptr, 0, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_pointer_present_from_a_single_rim_typemouse_entry) {
    RAWINPUTDEVICELIST devices[1]{};
    devices[0].dwType = RIM_TYPEMOUSE;

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 1, 0, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_keyboard_present_from_a_single_rim_typekeyboard_entry) {
    RAWINPUTDEVICELIST devices[1]{};
    devices[0].dwType = RIM_TYPEKEYBOARD;

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 1, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

// RIM_TYPEHID entries this project's three-value seat_capability enum
// has no slot for (seat_adapter.cpp's own translate() comment, "default:
// counted by nothing here") must not be silently folded into pointer
// or keyboard - the mutation this case exists to catch is exactly
// that fold.
GLINTFX_TEST(translate_ignores_rim_typehid_entries_for_pointer_and_keyboard) {
    RAWINPUTDEVICELIST devices[1]{};
    devices[0].dwType = RIM_TYPEHID;

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 1, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
}

GLINTFX_TEST(translate_reports_touch_present_when_the_integrated_touch_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate(nullptr, 0, NID_INTEGRATED_TOUCH, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_touch_present_when_the_external_touch_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate(nullptr, 0, NID_EXTERNAL_TOUCH, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

// The mutation this case exists to catch: folding NID_INTEGRATED_PEN
// (a STYLUS digitizer, a different question than "touch" -
// seat_adapter.hpp's own header comment) into seat_capability::touch
// just because both live in the same SM_DIGITIZER bitmask.
GLINTFX_TEST(translate_reports_touch_absent_when_only_the_pen_bit_is_set) {
    seat_capabilities capabilities;
    win32_seat_adapter::translate(nullptr, 0, NID_INTEGRATED_PEN, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

GLINTFX_TEST(translate_reports_all_three_capabilities_present_together) {
    RAWINPUTDEVICELIST devices[3]{};
    devices[0].dwType = RIM_TYPEMOUSE;
    devices[1].dwType = RIM_TYPEKEYBOARD;
    devices[2].dwType = RIM_TYPEHID;

    seat_capabilities capabilities;
    win32_seat_adapter::translate(devices, 3, NID_INTEGRATED_TOUCH | NID_READY, capabilities);

    GLINTFX_CHECK(capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(capabilities.has_capability(seat_capability::touch));
}

// translate() overwrites `out` from scratch rather than only setting
// bits already true - a caller re-using the same seat_capabilities
// across a WM_INPUT_DEVICE_CHANGE recompute (win32_seat_adapter::
// recompute_capabilities()) depends on a PREVIOUSLY present device
// that has since vanished from the list being cleared, not left
// stale.
GLINTFX_TEST(translate_clears_a_previously_present_capability_no_longer_in_the_list) {
    seat_capabilities capabilities;
    capabilities.set_capability(seat_capability::pointer, true);
    capabilities.set_capability(seat_capability::keyboard, true);
    capabilities.set_capability(seat_capability::touch, true);

    win32_seat_adapter::translate(nullptr, 0, 0, capabilities);

    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::pointer));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::keyboard));
    GLINTFX_CHECK(!capabilities.has_capability(seat_capability::touch));
}

#endif // defined(_WIN32)
