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

#include <cstdint>

#include <glintfx/core/err.hpp>

#include "platform/input/seat_capabilities.hpp"
#include "platform/win32/device_change_message.hpp"
#include "platform/win32/display_adapter.hpp"

// platform/win32/seat_adapter.hpp - Y-1 (docs/plano-w6a-janela.md
// fatia 13, TODO.md WIN-SEAT, GODS_LAWS.md L-04). REDESENHADO em
// 09/09/2026 (D-090918, DECISOES_AUTONOMAS.md; plano completo em
// /var/tmp/glintfx-plan/win-seat.md, produto da pesquisa que GODS_LAWS.md
// L-43 exige ANTES de fatiar): a fatia original (05/09/2026) registrava
// RAW INPUT (RegisterRawInputDevices + RIDEV_DEVNOTIFY) para saber
// quando um dispositivo mudava - e a propria Microsoft desaconselha isso
// PARA UMA BIBLIOTECA, verbatim (learn.microsoft.com/windows/win32/api/
// winuser/nf-winuser-registerrawinputdevices, Remarks): "Only one window
// per raw input device class may be registered to receive raw input
// within a process... RegisterRawInputDevices should not be used from a
// library, as it may interfere with any raw input processing logic
// already present in applications that load it." Este projeto E'
// exatamente essa biblioteca (CLAUDE.md's own LEI ZERO deste
// repositorio: consumidor externo desconhecido) - um consumidor que
// registrasse entrada bruta para o proprio jogo desligava o aviso deste
// adaptador em silencio; o close() deste adaptador desligava a entrada
// bruta DELE (RIDEV_REMOVE e' por CLASSE de dispositivo, no PROCESSO
// inteiro, nunca por janela - RAWINPUTDEVICE's own documentation:
// hwndTarget "If NULL, raw input events follow the keyboard focus", e
// RIDEV_REMOVE "removes the top level collection from the inclusion
// list"); e dois assentos no mesmo processo roubavam o registro um do
// outro. Nenhum teste desta fatia jamais pegou os tres, porque em todos
// eles a biblioteca era a UNICA registrante.
//
// O MECANISMO NOVO: RegisterDeviceNotificationW (learn.microsoft.com/
// windows/win32/api/winuser/nf-winuser-registerdevicenotificationw) na
// MESMA janela so' de mensagens este adaptador ja abria - por HANDLE,
// sem estado de processo ("The same window handle can be used in
// multiple calls" - a mesma pagina, e a razao de D-WS-7: dois assentos
// no mesmo processo guardam DOIS PARES de HDEVNOTIFY independentes,
// nunca um estado compartilhado). Cada chamada registra um FILTRO
// DEV_BROADCAST_DEVICEINTERFACE_W (dbcc_devicetype =
// DBT_DEVTYP_DEVICEINTERFACE) contra o GUID da classe de interface -
// teclado, GUID_DEVINTERFACE_KEYBOARD; mouse, GUID_DEVINTERFACE_MOUSE
// (learn.microsoft.com/windows-hardware/drivers/install/
// guid-devinterface-keyboard e .../guid-devinterface-mouse) - NUNCA um
// GUID de setup class, a armadilha que a mesma documentacao nomeia: "If
// a driver registers for notification using a setup class GUID instead
// of an interface class GUID, it isn't notified." Os dois GUIDs vivem
// como `constexpr GUID` no .cpp (D-WS-2: escritos a mao a partir do
// valor documentado, em vez de <ntddkbd.h>/<ntddmou.h> + <initguid.h> -
// DEFINE_GUID so' produz a definicao na TU que inclui <initguid.h>
// primeiro, a mesma armadilha que ja custou caro para PKEY_AppUserModel_
// ID, src/platform/win32/CMakeLists.txt's own comment).
//
// A janela recebe WM_DEVICECHANGE com wParam
// DBT_DEVICEARRIVAL/DBT_DEVICEREMOVECOMPLETE e lParam apontando para o
// bloco - device_change_message.hpp's own classify_device_change() e' a
// metade PURA que decide isso a partir do (wParam, lParam) sozinho, sem
// janela nem registro algum, aplicando as duas guardas que win-seat.md
// sec. 1.3 exige (lParam pode ser nulo; o bloco pode nomear um tipo de
// difusao diferente de DBT_DEVTYP_DEVICEINTERFACE).
//
// D-WS-3: so' teclado e mouse sao registrados aqui - toque continua
// vindo de GetSystemMetrics(SM_DIGITIZER) na releitura
// (recompute_capabilities(), abaixo, inalterado por esta fatia).
// Registrar GUID_DEVINTERFACE_HID acordaria o assento em toda chegada de
// gamepad/joystick para reler uma metrica que a propria Microsoft
// documenta como nao-PnP (learn.microsoft.com/windows/win32/wintouch/
// getting-started-with-multi-touch-messages: "there is no support for
// plug and play").
//
// SDL3 lido para a TECNICA, nada copiado (GODS_LAWS.md L-29, win-seat.md
// sec. 1.5): SDL_hid.c's own WIN_InitDeviceNotification usa CM_Register_
// Notification (cfgmgr32.dll carregada a mao, thread propria); este
// adaptador NAO adota isso - RegisterDeviceNotificationW entrega pela
// MESMA bomba de mensagens da thread do display (pump_events),
// preservando a entrega deterministica da GODS_LAWS.md L-35 sem cadeado
// algum. O que SIM foi aprendido do SDL3 e' mantido: o aviso e' so' um
// GATILHO, a verdade sempre vem de RE-ENUMERAR do zero
// (recompute_capabilities(), a mesma funcao open() ja chamava e
// continua chamando).
//
// O QUE SO' O SERVIDOR WINDOWS PROVA (win-seat.md sec. 4, GODS_LAWS.md
// L-09/L-27): que RegisterDeviceNotificationW aceita esta janela so' de
// mensagens como destinatario (handle nao-nulo); que o assento DEIXA de
// ocupar o slot de raw input do processo
// (GetRegisteredRawInputDevices == 0, o teste que prova o defeito de
// verdade consertado); que o roteamento sintetico de WM_DEVICECHANGE
// chega a` instancia certa; e que dois assentos no mesmo processo tem
// vidas independentes. NAO provado por nenhum ambiente de CI: que o
// sistema de fato ENVIA WM_DEVICECHANGE registrado quando um teclado/
// mouse real e' conectado/removido - so' o laboratorio Windows opcional
// (D-WS-8, win-seat.md sec. 4) prova isso, nunca a sessao viva do lider
// (GODS_LAWS.md L-09). Nada neste arquivo foi compilado nem executado
// nesta maquina (sem toolchain Windows utilizavel, mesma limitacao
// declarada por display_adapter.cpp's own header comment, GODS_LAWS.md
// L-27).
//
// WHY A SECOND HWND, UNDER THE DISPLAY'S OWN CLASS, RATHER THAN A SECOND
// RegisterClassExW: unchanged by this redesign - see this header's own
// predecessor comment, preserved in git history, and display_adapter.
// hpp's own window_class_name() accessor comment for the full reasoning.
// Two different HWNDs sharing one class each have their OWN GWLP_USERDATA
// slot (a per-WINDOW piece of state, not per-class), so the display
// adapter's own window and this seat window's window never collide over
// it.
namespace glintfx::platform {

class win32_seat_adapter {
  public:
    // Starts CLOSED (m_window null) - same default_initializable<A>
    // shape win32_display_adapter's own default constructor has.
    win32_seat_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.2) - copying a live HWND (and the two
    // RegisterDeviceNotificationW registrations tied to it) would hand
    // two owners the same OS resources.
    win32_seat_adapter(const win32_seat_adapter &) = delete;
    win32_seat_adapter &operator=(const win32_seat_adapter &) = delete;
    win32_seat_adapter(win32_seat_adapter &&) = delete;
    win32_seat_adapter &operator=(win32_seat_adapter &&) = delete;

    // Idempotent close() in the destructor, same shape as
    // win32_display_adapter's own destructor.
    ~win32_seat_adapter();

    // `display` must already be open (win32_display_adapter::is_open()
    // true) - reading that precondition failure back as
    // gltfx_err_code::invalid_argument (never an assert/abort -
    // GODS_LAWS.md L-22, no exception crosses the public boundary).
    //
    // On success: creates the message-only window under `display`'s own
    // class, subclasses it with seat_window_proc, registers TWO device
    // interface notifications (keyboard, mouse - this header's own
    // "D-WS-3" paragraph explains why touch is not a third), and
    // computes the FIRST capabilities() reading via
    // recompute_capabilities() - the same function a routed
    // WM_DEVICECHANGE re-invokes later. A failure at any step tears
    // down whatever this call had already created before returning
    // (including unregistering a notification handle a previous step
    // already won) - is_open() reads false either way.
    [[nodiscard]] gltfx_rslt<void> open(const win32_display_adapter &display) noexcept;

    // Idempotent-safe, same shape as win32_display_adapter::close():
    // UnregisterDeviceNotification on both handles this INSTANCE holds
    // (D-WS-7 - per-instance handles, so closing THIS seat never
    // touches a SIBLING seat's own registration in the same process,
    // the exact defect this header's own top comment names for the
    // predecessor's process-wide RIDEV_REMOVE), then DestroyWindow.
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

    // Test seam only (SF-1, D-WS-7, two_seats_test): how many of the
    // TWO RegisterDeviceNotificationW calls the SYSTEM actually
    // accepted (a non-null HDEVNOTIFY), never a constant this class
    // asserts about itself - so a test can prove that TWO independent
    // seats in one process each hold their OWN pair, rather than one
    // seat's open() silently stealing (or being starved of) the
    // other's registration.
    [[nodiscard]] int device_notification_count() const noexcept;

    // "guarda o diff" (docs/plano-w6a-janela.md fatia 13's own row): the
    // KIND of the LAST WM_DEVICECHANGE this adapter's window procedure
    // routed to itself - device_change_kind::none until the first one
    // arrives. This is DELIBERATELY not "which capability changed":
    // recompute_capabilities() re-reads the WHOLE device list from
    // scratch on every notification (this header's own top comment
    // explains why), so this accessor exists only to let a test PROVE
    // the notification was routed to the right instance at all -
    // nothing internal reads it back.
    [[nodiscard]] device_change_kind last_device_change() const noexcept {
        return m_last_device_change;
    }

    // D-WS-4 (SF-2, win-seat.md sec. 3): a plain monotonic counter of
    // system announcements - incremented once inside
    // recompute_capabilities() (below), which BOTH open()'s own first
    // read and every routed WM_DEVICECHANGE call, so open() counts 1
    // and each device change counts +1 with no separate bookkeeping.
    // Never reset by close() (same convention wayland_seat_adapter::
    // last_change() already keeps, src/platform/wayland/seat_
    // adapter.hpp). NOT comparable by EQUALITY against the Wayland
    // side's own last_change() - the two count different grandezas
    // (tests/measured_exceptions.txt's own seat_test.capability_events
    // entry, lado="ambos": Wayland counts capabilities+name events on
    // bind, Win32 counts the initial synchronous read) - only the NAME
    // and the "at least one announcement happened" shape are shared.
    [[nodiscard]] std::uint64_t last_change() const noexcept { return m_last_change; }

    // Pure translation seam (win32_seat_translation_test,
    // tests/CMakeLists.txt): the SAME logic open() and the
    // WM_DEVICECHANGE handler both call against the REAL
    // GetRawInputDeviceList()/GetSystemMetrics(SM_DIGITIZER) results,
    // exposed here so a test can feed a SYNTHETIC device list and
    // bitmask - no window, no RegisterDeviceNotificationW, no syscall
    // at all. `devices` may be nullptr when `device_count` is 0 (an
    // empty device list is a legitimate answer, not a caller error).
    static void translate(const RAWINPUTDEVICELIST *devices, UINT device_count,
                          int digitizer_bitmask, seat_capabilities &out) noexcept;

    // INTERNAL SEAM, public only because seat_window_proc (seat_
    // adapter.cpp, anonymous namespace) is a plain WNDPROC callback -
    // the system calls it directly, so it cannot be a private member.
    // Never call either from anywhere but that callback - open()/
    // close() do not need them, and no other caller has a reason to.
    void handle_device_change(device_change_kind kind) noexcept;
    [[nodiscard]] WNDPROC previous_wndproc() const noexcept;

  private:
    void recompute_capabilities() noexcept;

    HWND m_window = nullptr;
    WNDPROC m_previous_wndproc = nullptr;
    HDEVNOTIFY m_keyboard_notification = nullptr;
    HDEVNOTIFY m_mouse_notification = nullptr;
    seat_capabilities m_capabilities;
    device_change_kind m_last_device_change = device_change_kind::none;
    std::uint64_t m_last_change = 0;
};

} // namespace glintfx::platform

#endif // defined(_WIN32)
