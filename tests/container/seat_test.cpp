// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/input/seat_capabilities.hpp"
#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/seat_adapter.hpp"

// seat_test.cpp - WL-SEAT fatia S-B (docs/plano-w6a-janela.md fatia
// 12): the container-integration case for wayland_seat_adapter - binds
// wl_seat against a REAL compositor (the same kwin_wayland --virtual
// this container already proves announces wl_compositor/xdg_wm_base
// for registry_smoke, and now wl_seat too - see this file's own
// printed line for the measured value) and proves at least one
// capabilities/name event was actually dispatched. Same shape as
// registry_smoke.cpp/shell_smoke.cpp: wayland_display_adapter,
// wayland_seat_adapter, seat_capabilities.cpp, global_catalog.cpp,
// err.cpp and err_code.cpp are compiled straight into this ONE
// executable (tests/container/prepare_arch_ports_fixture.sh stages
// the real sources) - no glintfx.so, no CMake, no .pc file. NAMED
// "seat_test", NOT "seat_smoke" (docs/plano-w6a-janela.md fatia 12's
// own "Par no portao" column: "seat_test mesmo nome nos dois (fixture/
// ctest)") - the eventual Windows-side ctest (Y-1, fatia 13) is meant
// to carry this SAME name, the same "no extra name" shape window_
// smoke.cpp's own header comment documents for window_parity_test.
//
// MEASURED, NEVER ASSERTED (docs/plano-w6a-janela.md sec. "o que ficou
// sem medicao": "valor das capacidades do seat nos dois executores:
// impresso, nao exigido, ate medido") - this fixture prints whatever
// wl_seat.name and wl_seat.capabilities the container's own compositor
// announced and asserts only that SOME event was observed at all
// (seat.last_change() > 0), never a specific bitmask or name. A
// headless kwin_wayland --virtual with no real input device attached
// may reasonably announce zero capabilities - that is a fact about
// THIS environment, not a defect this fixture is entitled to invent an
// expectation against (docs/plano-w6a-janela.md's own "onde esta o
// perigo" (b): "o argumento 'o executor nao tem X, entao a fatia pode
// fechar sem provar X' e exatamente o argumento aceito em silencio" -
// the difference here is that this fixture DOES prove the one thing it
// can: that the bind succeeded and the listener fired, not that any
// particular device class is present).
//
// WHAT THIS FIXTURE DOES NOT PROVE, DECLARED (same shape shell_smoke.
// cpp's own header comment already uses): whether a SECOND
// capabilities event (a device hot-plugged mid-run) is handled
// correctly is out of this fixture's control - a container with no
// real input device attached has no mechanism to trigger one on
// demand. That path is proven at the listener level instead, by
// seat_adapter_listener_test.cpp's own capabilities_event_fired_twice_
// with_different_bitmasks_updates_in_place case (tests/) - see that
// file's own header comment.

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // all ten fixtures in this family (connect_smoke.cpp's own header
    // comment on this exact line, docs/plano-w6b-placa-e-laco.md fatia
    // 1, 06/09/2026; corrected same day, connect_smoke.cpp's own
    // header comment again, after `_IOLBF` with `size` 0 crashed the
    // Windows CI job with 0xC0000409 - MSVC's setvbuf rejects that
    // combination outside its documented 2 <= size <= INT_MAX range,
    // while `_IONBF` ignores `size`/`buffer` entirely).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "seat_test: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_seat_adapter seat;
    glintfx::gltfx_rslt<void> seat_opened = seat.open(adapter);
    if (seat_opened.has_error()) {
        std::fprintf(stderr, "seat_test: seat.open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(seat_opened.error().code())).c_str(),
                     std::string(seat_opened.error().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }
    if (!seat.is_open()) {
        std::fprintf(stderr, "seat_test: seat.open() reported success but is_open() is false\n");
        adapter.close();
        return EXIT_FAILURE;
    }

    // wl_seat.capabilities is sent "on binding to the seat global"
    // (wayland.xml, wl_seat_listener's own capabilities documentation) -
    // one roundtrip is enough to have it (and, if the compositor sends
    // it, wl_seat.name) already dispatched by the time we read either.
    glintfx::gltfx_rslt<void> roundtripped = adapter.roundtrip();
    if (roundtripped.has_error()) {
        std::fprintf(
            stderr, "seat_test: roundtrip() after seat.open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(roundtripped.error().code())).c_str());
        seat.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    using glintfx::platform::seat_capability;
    const int has_pointer = seat.capabilities().has_capability(seat_capability::pointer) ? 1 : 0;
    const int has_keyboard = seat.capabilities().has_capability(seat_capability::keyboard) ? 1 : 0;
    const int has_touch = seat.capabilities().has_capability(seat_capability::touch) ? 1 : 0;
    const auto last_change = static_cast<unsigned long long>(seat.last_change());
    std::fprintf(stdout,
                 "seat_test: name=\"%s\" pointer=%d keyboard=%d touch=%d last_change=%llu "
                 "(measured, not asserted)\n",
                 seat.name().c_str(), has_pointer, has_keyboard, has_touch, last_change);
    // MEASURED-COLLECTOR (tests/tools/collect_measured.py): same four
    // facts, one MEASURED line each - this fixture is Linux-only
    // (Wayland's own wl_seat), so these land in the parity table's
    // "so de um lado" section until win32_seat_translation_test (or a
    // future sibling) measures the same four keys on Windows.
    //
    // capability_events, NOT last_change (achado do CTO, 06/09/2026,
    // fatia de fechamento): the name "last_change" used to be shared
    // with tests/seat_test.cpp's own Windows side, and the parity
    // table compared them as if they were the same fact - they are
    // not. This side counts EVENTS (wl_seat's own capabilities/name
    // announcements, plural, one per bind burst); the Windows side
    // reads a single WM_INPUT_DEVICE_CHANGE message's own wParam CODE.
    // A counter is not a code - renamed so the key's own name says
    // which grandeza it measures, never "the same word, two units".
    std::fprintf(stdout, "MEASURED seat_test.pointer=%d\n", has_pointer);
    std::fprintf(stdout, "MEASURED seat_test.keyboard=%d\n", has_keyboard);
    std::fprintf(stdout, "MEASURED seat_test.touch=%d\n", has_touch);
    std::fprintf(stdout, "MEASURED seat_test.capability_events=%llu\n", last_change);

    // The one assertion this fixture makes (docs/plano-w6a-janela.md,
    // fatia 12's own briefing: "asserçao minima: leitura inicial
    // aconteceu"): SOME event fired at all. Zero would mean the bind
    // silently produced a seat object the compositor never actually
    // talked to - a broken fixture, not a legitimate "no capabilities"
    // answer (a real "no device classes" answer still arrives AS an
    // event, with an empty bitmask, and still increments last_change()).
    if (seat.last_change() == 0) {
        std::fprintf(stderr, "seat_test: no capabilities/name event observed at all "
                             "(last_change() == 0)\n");
        seat.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    seat.close();
    if (seat.is_open()) {
        std::fprintf(stderr, "seat_test: seat.close() ran but is_open() is still true\n");
        adapter.close();
        return EXIT_FAILURE;
    }

    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "seat_test: adapter.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "seat_test: disconnected (is_open() == false)\n");

    return EXIT_SUCCESS;
}
