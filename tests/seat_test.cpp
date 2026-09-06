// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <print>

#include <glintfx/core/err.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/seat_adapter.hpp"

// seat_test.cpp - Y-1 (docs/plano-w6a-janela.md fatia 13, "bind real" -
// the plan's own name for THIS file): win32_seat_adapter opened for
// REAL against the windows-latest runner - a real message-only window
// under a real win32_display_adapter's own class, a real
// RegisterRawInputDevices call, and the FIRST capabilities() reading
// coming from the runner's own GetRawInputDeviceList()/
// GetSystemMetrics(SM_DIGITIZER), not a fixture. NAMED "seat_test", NOT
// "win32_seat_test" - tests/container/seat_test.cpp's own header
// comment already names this requirement (docs/plano-w6a-janela.md
// fatia 12's own "Par no portao" column: "seat_test mesmo nome nos
// dois (fixture/ctest)"), the same "no extra platform prefix" shape
// tests/two_displays_test.cpp's own header comment documents for its
// own name. GODS_LAWS.md L-04's own "mecanismo pode diferir;
// comportamento observavel e cobertura, nao" pair: the Linux side is a
// container FIXTURE (tests/container/seat_test.cpp, staged into P-0's
// own inventory, never a `ctest -N` entry itself); this file IS a
// `ctest -N` entry, and check_test_parity.py resolves each side's
// windows_only/linux_only set against the OTHER side's full inventory
// (union of real ctest names and the container's own published
// fixture names), not a 1-for-1 file match - so the identical NAME is
// what makes the two line up, regardless of which mechanism registers
// each one.
//
// WHAT IS ASSERTED, minimally, and WHAT IS ONLY PRINTED: the ONE
// assertion the first case makes is "open() succeeded, is_open() is
// true, capabilities() is READABLE" - the runner's ACTUAL device mix
// (mouse=1 keyboard=1 hid=0 digitizer=0x00, measured by X-GL-0 (b) on
// 05/09/2026, this fatia's own briefing) is PRINTED, never asserted,
// for the same reason tests/win32_runner_probe_test.cpp prints its own
// GL/device findings instead of asserting them: a runner that changes
// its device mix between CI images is a fact to read, not a failure to
// chase (GODS_LAWS.md L-04's own "nao invente capacidade que a maquina
// nao reporta... mas a ausencia tem de ser visivel, nao silenciosa" -
// piso de varredura nao-vazia, GODS_LAWS.md L-40, is why every value is
// printed even when it is exactly zero).
//
// The SECOND case proves the mechanism win32_seat_translation_test.cpp
// cannot reach on its own: WM_INPUT_DEVICE_CHANGE actually routed, by
// the real GWLP_WNDPROC subclass this fatia installs, to THIS adapter
// instance and no other - sent SYNTHETICALLY (SendMessageW, not a real
// unplug/replug the runner cannot be made to do), the same "prove the
// seam with a synthetic message against a real window" shape docs/
// plano-w6a-janela.md sec. 2.2 row 9 names for X-2's own win32_message_
// translation_test (mensagens sinteticas: tamanho, close pegajoso,
// ativacao, DPI 144) - this file's own synthetic WM_INPUT_DEVICE_CHANGE
// is the seat's version of that same technique. The mutation this case
// exists to catch: deleting the WM_INPUT_DEVICE_CHANGE branch in
// seat_window_proc (seat_adapter.cpp) would leave last_change_kind()/
// last_change_device() stuck at their constructed-default 0/nullptr
// forever - the GLINTFX_CHECKs below, reading back the sentinel values
// SendMessageW just sent, are what turn that into a red test rather
// than a silent no-op.
//
// win32_seat_adapter/win32_display_adapter are compiled a SECOND time
// directly into this executable's own object set (tests/CMakeLists.
// txt), the same technique win32_display_connect_test.cpp already
// documents: glintfx's own shared build hides everything without
// GLINTFX_API, so a separate executable linking only against
// glintfx::glintfx could never resolve these methods across that
// boundary.

using glintfx::platform::seat_capability;
using glintfx::platform::win32_display_adapter;
using glintfx::platform::win32_seat_adapter;

GLINTFX_TEST(win32_seat_adapter_opens_against_a_real_display_and_reads_capabilities) {
    win32_display_adapter display;
    const glintfx::gltfx_rslt<void> display_opened = display.open();
    GLINTFX_CHECK(display_opened.has_value());
    GLINTFX_CHECK(display.is_open());

    win32_seat_adapter seat;
    GLINTFX_CHECK(!seat.is_open());

    const glintfx::gltfx_rslt<void> seat_opened = seat.open(display);

    // The ONE assertion this case makes (this file's own header
    // comment): the runner's own device mix, printed below, is a
    // measurement, not a pass/fail condition.
    GLINTFX_CHECK(seat_opened.has_value());
    GLINTFX_CHECK(seat.is_open());

    const bool has_pointer = seat.capabilities().has_capability(seat_capability::pointer);
    const bool has_keyboard = seat.capabilities().has_capability(seat_capability::keyboard);
    const bool has_touch = seat.capabilities().has_capability(seat_capability::touch);
    std::println("seat_test: pointer={} keyboard={} touch={}", has_pointer, has_keyboard,
                 has_touch);
    // MEASURED-COLLECTOR (achado do time-lead, 06/09/2026, item 3 da
    // fatia de fechamento - "o lado Windows ganha as quatro chaves"):
    // tests/container/seat_test.cpp's own Wayland twin already prints
    // these same four keys; this side never did, leaving the parity
    // table's own comparison one-sided despite the SAME test name on
    // both systems (P-0). `last_change_kind()` reads whatever this
    // adapter's own real RegisterRawInputDevices bind left it at - 0
    // (the constructed default, seat_adapter.hpp's own comment) unless
    // a real hotplug already fired before this line ran, the identical
    // "measured, never asserted" shape the surrounding case already
    // uses for pointer/keyboard/touch above.
    std::println("MEASURED seat_test.pointer={}", has_pointer ? 1 : 0);
    std::println("MEASURED seat_test.keyboard={}", has_keyboard ? 1 : 0);
    std::println("MEASURED seat_test.touch={}", has_touch ? 1 : 0);
    std::println("MEASURED seat_test.last_change={}",
                 static_cast<unsigned long long>(seat.last_change_kind()));

    seat.close();
    GLINTFX_CHECK(!seat.is_open());

    // Idempotent, same shape win32_display_adapter::close() already
    // documents for itself.
    seat.close();
    GLINTFX_CHECK(!seat.is_open());

    display.close();
    GLINTFX_CHECK(!display.is_open());
}

GLINTFX_TEST(win32_seat_adapter_routes_a_synthetic_wm_input_device_change_to_itself) {
    win32_display_adapter display;
    GLINTFX_CHECK(display.open().has_value());

    win32_seat_adapter seat;
    GLINTFX_CHECK(seat.open(display).has_value());

    // No real WM_INPUT_DEVICE_CHANGE has arrived yet - constructed
    // default (seat_adapter.hpp's own last_change_kind()/
    // last_change_device() comment).
    GLINTFX_CHECK(seat.last_change_kind() == 0);
    GLINTFX_CHECK(seat.last_change_device() == nullptr);

    // Sentinel HANDLE value, never dereferenced by anything this test
    // or win32_seat_adapter touches (handle_input_device_change() only
    // STORES it, seat_adapter.cpp's own header comment on that
    // function) - a fabricated address is enough to prove routing.
    HANDLE arrival_sentinel = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x1234));

    // Sent with the CALLING thread's own SendMessageW - the message-
    // only window this adapter owns belongs to this same thread, so
    // this is delivered synchronously, straight into seat_window_proc
    // (seat_adapter.cpp), before SendMessageW returns.
    ::SendMessageW(seat.native_handle(), WM_INPUT_DEVICE_CHANGE, GIDC_ARRIVAL,
                   reinterpret_cast<LPARAM>(arrival_sentinel));

    GLINTFX_CHECK(seat.last_change_kind() == static_cast<WPARAM>(GIDC_ARRIVAL));
    GLINTFX_CHECK(seat.last_change_device() == arrival_sentinel);

    HANDLE removal_sentinel = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x5678));
    ::SendMessageW(seat.native_handle(), WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL,
                   reinterpret_cast<LPARAM>(removal_sentinel));

    GLINTFX_CHECK(seat.last_change_kind() == static_cast<WPARAM>(GIDC_REMOVAL));
    GLINTFX_CHECK(seat.last_change_device() == removal_sentinel);

    seat.close();
    display.close();
}

#endif // defined(_WIN32)
