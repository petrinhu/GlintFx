// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <glintfx/core/err.hpp>

#include "platform/input/seat_capabilities.hpp"
#include "platform/win32/display_adapter.hpp"

// platform/win32/seat_adapter.hpp - Y-1 (docs/plano-w6a-janela.md
// fatia 13, TODO.md WIN-SEAT, GODS_LAWS.md L-04): the Windows
// counterpart of S-B (src/platform/wayland/, fatia 12, not yet
// implemented at the time this file was written - see this project's
// own briefing for this fatia, docs/plano-w6a-janela.md sec. 2.2 row
// 12/13). Wayland's wl_seat announces capabilities through a bitmask
// EVENT the compositor pushes; Win32 has no equivalent push channel
// for "which device CLASSES exist" - the two facts this adapter
// stitches together instead are GetRawInputDeviceList() (an
// ENUMERATION, pulled, not pushed) for pointer/keyboard, and
// GetSystemMetrics(SM_DIGITIZER) (a single bitmask read, also pulled)
// for touch. WM_INPUT_DEVICE_CHANGE (delivered only after
// RegisterRawInputDevices(..., RIDEV_DEVNOTIFY)) is the closest Win32
// analogue of "the set of seats changed" - it fires on a SINGLE
// device's arrival/removal, not a capability bitmask, so this
// adapter's own reaction to it is to RE-ENUMERATE from scratch
// (recompute_capabilities(), same function open() itself calls) and
// let the class-level counts speak, rather than trying to patch one
// bit in place from a device type this project would have to decode
// out of the HANDLE alone. GODS_LAWS.md L-04's own rule ("mecanismo
// pode diferir; comportamento observavel e cobertura, nao") is why
// this file exists at all: the OBSERVABLE fact - does THIS seat have
// a pointer, a keyboard, a touch surface - is the same
// glintfx::platform::seat_capabilities (platform/input/
// seat_capabilities.hpp) S-B feeds from its own, completely different,
// event-driven mechanism.
//
// MEASURED ON THE SERVER TODAY (05/09/2026, the sonda this fatia's own
// briefing names, X-GL-0 case (b), windows-latest runner): a mouse and
// a keyboard, no HID beyond those two, SM_DIGITIZER=0x00 (no
// digitizer), GetDpiForWindow=96. This header's own tests build every
// assertion around what THAT machine reports - touch/digitizer
// presence on THIS runner is false, and seat_test (tests/seat_test.cpp,
// tests/CMakeLists.txt - NAMED to match the Wayland side's own
// container fixture, tests/container/seat_test.cpp's own header
// comment) prints, never asserts, the live numbers so a runner
// that later gains a digitizer is a fact the next reader sees, not a
// silently wrong assumption baked into a boolean here (GODS_LAWS.md
// L-04: "nao invente capacidade que a maquina nao reporta... mas a
// ausencia tem de ser visivel, nao silenciosa" - printed every run,
// piso de varredura nao-vazia, GODS_LAWS.md L-40).
//
// MECHANISM, written from Microsoft's own current documentation (this
// project has no usable Windows toolchain on this machine - display_
// adapter.hpp's own header comment already declares the same thing
// for the fatia this one extends, GODS_LAWS.md L-27):
//   - RAWINPUTDEVICE/RegisterRawInputDevices/RIDEV_DEVNOTIFY:
//     learn.microsoft.com/windows/win32/api/winuser/ns-winuser-rawinputdevice
//     ("RIDEV_DEVNOTIFY - 0x00002000 - enables the caller to receive
//     WM_INPUT_DEVICE_CHANGE notifications"); RIDEV_REMOVE (0x00000001,
//     same page) "requires hwndTarget to be NULL, or RegisterRawInputDevices
//     fails" - close() below follows that exactly.
//   - Usage page/usage pair for mouse and keyboard: learn.microsoft.com/
//     windows-hardware/drivers/hid/hid-usages ("Generic Desktop
//     Controls" usage page 0x01; usage ID 0x02 = Mouse, 0x06 =
//     Keyboard) - the SAME page tests/win32_runner_probe_test.cpp's
//     own RIM_TYPEMOUSE/RIM_TYPEKEYBOARD classification already proved
//     live on this runner (05/09/2026), just read from the other end
//     (declaring interest in a usage instead of classifying a
//     RAWINPUTDEVICELIST entry's dwType - the enum this file's own
//     GetRawInputDeviceList() call classifies is the SAME
//     RIM_TYPEMOUSE/RIM_TYPEKEYBOARD/RIM_TYPEHID that probe already
//     exercised, from <windows.h>, not redeclared here).
//   - WM_INPUT_DEVICE_CHANGE/GIDC_ARRIVAL/GIDC_REMOVAL: learn.microsoft.com/
//     windows/win32/inputdev/wm-input-device-change ("Sent... through
//     its WindowProc function... If an application processes this
//     message, it should return zero").
//   - SM_DIGITIZER/NID_* bitmask: learn.microsoft.com/windows/win32/api/
//     winuser/nf-winuser-getsystemmetrics, Remarks section
//     ("NID_INTEGRATED_TOUCH 0x01, NID_EXTERNAL_TOUCH 0x02,
//     NID_INTEGRATED_PEN 0x04, NID_EXTERNAL_PEN 0x08, NID_MULTI_INPUT
//     0x40, NID_READY 0x80"). This adapter reads touch presence as
//     "either touch bit set" (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)
//     - seat_capability::touch names "touch/digitizer surface"
//     (seat_capabilities.hpp's own header comment), and the pen bits
//     answer a DIFFERENT question (stylus, not finger/touch) that this
//     project's three-value enum has no slot for yet; NID_MULTI_INPUT/
//     NID_READY are qualifiers on an existing digitizer, never proof of
//     one by themselves, so neither is read here.
//   - GWLP_WNDPROC instance subclassing: learn.microsoft.com/windows/
//     win32/winmsg/about-window-procedures#window-procedure-subclassing
//     ("An application subclasses an instance of a window by using
//     SetWindowLongPtr... passes GWL_WNDPROC... SetWindowLongPtr
//     returns the address of the window's original window procedure.
//     The application must save this address... to pass intercepted
//     messages to the original window procedure") - m_previous_wndproc
//     below is exactly that saved address, passed to CallWindowProcW
//     for every message seat_window_proc does not itself handle,
//     rather than assuming DefWindowProcW alone is equivalent: correct
//     TODAY (the class's own window_proc, display_adapter.cpp, only
//     ever acts on WM_NCCREATE, already spent by the time this
//     instance's subclass takes over), but chaining through the saved
//     pointer means this file never has to be revisited if fatia 9
//     (X-2, window_adapter, not yet implemented) ever teaches that
//     SHARED window_proc to do more.
//
// WHY A SECOND HWND, UNDER THE DISPLAY'S OWN CLASS, RATHER THAN A
// SECOND RegisterClassExW: docs/plano-w6a-janela.md fatia 13's own row
// says "janela so de mensagens na classe do display" - this fatia
// reuses win32_display_adapter::window_class_name() (display_
// adapter.hpp's own public seam, already written for exactly this
// reuse by fatia 9/X-2's own header comment - "a future window it
// opens under the same display connection reuses this SAME class")
// instead of registering a second class this project would have to
// track and unregister on its own. The class's own window_proc
// (display_adapter.cpp, anonymous namespace) still runs first for
// THIS window's own WM_NCCREATE - installing GWLP_USERDATA = the
// `this` pointer passed as CreateWindowExW's lpParam below, the SAME
// "Managing Application State" sequence display_adapter.cpp's own
// window_proc comment cites - before open() below ever subclasses the
// INSTANCE (not the class) with seat_window_proc to add
// WM_INPUT_DEVICE_CHANGE handling on top. Two different HWNDs sharing
// one class each have their OWN GWLP_USERDATA slot (a per-WINDOW piece
// of state, not per-class), so the display adapter's own window and
// this seat window's window never collide over it.
namespace glintfx::platform {

class win32_seat_adapter {
  public:
    // Starts CLOSED (m_window null) - same default_initializable<A>
    // shape win32_display_adapter's own default constructor has.
    win32_seat_adapter() noexcept = default;

    // Move-only, same reasoning as win32_display_adapter: copying a
    // live HWND (and the RegisterRawInputDevices registration tied to
    // it) would hand two owners the same OS resource, and destroying
    // either twice is a double-free of OS-owned state.
    win32_seat_adapter(const win32_seat_adapter &) = delete;
    win32_seat_adapter &operator=(const win32_seat_adapter &) = delete;

    win32_seat_adapter(win32_seat_adapter &&other) noexcept;
    win32_seat_adapter &operator=(win32_seat_adapter &&other) noexcept;

    // Idempotent close() in the destructor, same shape as
    // win32_display_adapter's own destructor.
    ~win32_seat_adapter();

    // `display` must already be open (win32_display_adapter::is_open()
    // true) - window_class_name() is "empty/undefined before open()
    // succeeds" by that header's own accessor comment, and this
    // adapter has nothing of its own to register a class from. Reading
    // that precondition failure back as gltfx_err_code::invalid_argument
    // (never an assert/abort - GODS_LAWS.md L-22, no exception crosses
    // the public boundary) rather than silently creating a window under
    // an empty class name that CreateWindowExW would refuse anyway with
    // a much less specific error.
    //
    // On success: creates the message-only window under `display`'s
    // own class (this header's own "WHY A SECOND HWND" paragraph),
    // subclasses it with seat_window_proc, calls
    // RegisterRawInputDevices() for the mouse and keyboard usages with
    // RIDEV_DEVNOTIFY, and computes the FIRST capabilities() reading
    // via recompute_capabilities() - the same function
    // WM_INPUT_DEVICE_CHANGE re-invokes later. A failure at any step
    // tears down whatever this call had already created before
    // returning (same "close() to undo a partial open()" shape
    // win32_display_adapter::open() already documents for its own
    // class/window pair) - is_open() reads false either way.
    [[nodiscard]] gltfx_rslt<void> open(const win32_display_adapter &display) noexcept;

    // Idempotent-safe, same shape as win32_display_adapter::close():
    // RIDEV_REMOVE first (hwndTarget MUST be NULL for that flag, per
    // RAWINPUTDEVICE's own documentation quoted in this header's
    // "MECHANISM" paragraph - done BEFORE DestroyWindow, while the
    // window handle this registration named is still valid, exactly
    // the ordering RIDEV_REMOVE's own precondition requires), then
    // DestroyWindow.
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return m_window != nullptr; }

    // The one shared value type S-B (Wayland) and this adapter both
    // feed (seat_capabilities.hpp's own header comment) - read-only
    // here, because THIS adapter is the only writer of its own
    // instance's copy.
    [[nodiscard]] const seat_capabilities &capabilities() const noexcept { return m_capabilities; }

    // Test seam only (win32_seat_translation_test, seat_test):
    // the raw window handle, so a test can confirm GWLP_USERDATA and
    // the GWLP_WNDPROC subclass installed correctly - never used by
    // open()/close()/recompute_capabilities() themselves.
    [[nodiscard]] HWND native_handle() const noexcept { return m_window; }

    // "guarda o diff" (docs/plano-w6a-janela.md fatia 13's own row):
    // the wParam/lParam of the LAST WM_INPUT_DEVICE_CHANGE this
    // adapter's window procedure observed - GIDC_ARRIVAL (1),
    // GIDC_REMOVAL (2), or 0 if none has arrived yet. This is
    // DELIBERATELY not "which capability changed": recompute_
    // capabilities() re-reads the WHOLE device list from scratch on
    // every notification (this header's own top comment explains why
    // - a single HANDLE alone does not carry enough information to
    // patch one bit of seat_capabilities in place without another
    // GetRawInputDeviceInfo round trip this fatia's own scope does not
    // call for), so these two accessors exist only to let a test PROVE
    // the notification was routed to the right instance at all -
    // nothing internal reads them back.
    [[nodiscard]] WPARAM last_change_kind() const noexcept { return m_last_change_kind; }
    [[nodiscard]] HANDLE last_change_device() const noexcept { return m_last_change_device; }

    // Pure translation seam (win32_seat_translation_test,
    // tests/CMakeLists.txt): the SAME logic open() and the
    // WM_INPUT_DEVICE_CHANGE handler both call against the REAL
    // GetRawInputDeviceList()/GetSystemMetrics(SM_DIGITIZER) results,
    // exposed here so a test can feed a SYNTHETIC device list and
    // bitmask - no window, no RegisterRawInputDevices, no syscall at
    // all - the same "prove the pure half without a live backend"
    // shape wayland_window_adapter's own listener tests already use
    // for window_configure_sequence (this fatia's own briefing: "a
    // sonda X-GL-0 (b) ja tera dito o que o executor devolve", this is
    // the counterpart that lets a test assert something stronger than
    // print-and-hope against that one fixed machine). `devices` may be
    // nullptr when `device_count` is 0 (an empty device list is a
    // legitimate answer, not a caller error).
    static void translate(const RAWINPUTDEVICELIST *devices, UINT device_count,
                          int digitizer_bitmask, seat_capabilities &out) noexcept;

    // INTERNAL SEAM, public only because seat_window_proc (seat_
    // adapter.cpp, anonymous namespace) is a plain WNDPROC callback -
    // the system calls it directly, so it cannot be a private member,
    // and these two are what it needs to route WM_INPUT_DEVICE_CHANGE
    // and everything else correctly (this header's own "WHY A SECOND
    // HWND" paragraph). Never call either from anywhere but that
    // callback - open()/close() do not need them, and no other caller
    // has a reason to.
    void handle_input_device_change(WPARAM kind, HANDLE device) noexcept;
    [[nodiscard]] WNDPROC previous_wndproc() const noexcept;

  private:
    void recompute_capabilities() noexcept;

    HWND m_window = nullptr;
    WNDPROC m_previous_wndproc = nullptr;
    seat_capabilities m_capabilities;
    WPARAM m_last_change_kind = 0;
    HANDLE m_last_change_device = nullptr;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
