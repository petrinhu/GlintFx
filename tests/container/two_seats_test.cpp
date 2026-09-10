// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/input/seat_capabilities.hpp"
#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/seat_adapter.hpp"

// two_seats_test.cpp - SF-1 of WIN-SEAT's 09/09/2026 reopening (win-
// seat.md sec. 1.1 defect 3, sec. 3 D-WS-7, D-090918 in DECISOES_
// AUTONOMAS.md): the Linux side of the pair tests/two_seats_test.cpp
// (if(WIN32)) registers as a normal ctest - two wayland_seat_adapter
// instances, each against its OWN wayland_display_adapter connection,
// bound against THIS SAME real compositor, proving neither's teardown
// disturbs the other. NAMED "two_seats_test", not a per-platform
// prefix or "_smoke" suffix - the same "no extra name" requirement
// tests/container/two_displays_test.cpp's own header comment documents
// for its own name, so tests/tools/check_test_parity.py's own P-0
// inventory lines this fixture up against the Windows side's own ctest
// of the identical name.
//
// UNLIKE two_displays_test.cpp (two connections against the same
// compositor, same process): this fixture opens each seat under its
// OWN wayland_display_adapter connection, the same "each seat needs its
// own display" shape seat_test.cpp already uses for its single seat -
// wayland_seat_adapter::open() takes a wayland_display_adapter&, and
// nothing about this fatia's own scope (SF-1, win-seat.md sec. 5) asks
// whether TWO seats can share ONE connection (that question belongs to
// a future window/input fatia, not this one).
//
// wayland_display_adapter, wayland_seat_adapter, seat_capabilities.cpp,
// global_catalog.cpp, err.cpp and err_code.cpp are compiled straight
// into this ONE executable (tests/container/prepare_arch_ports_
// fixture.sh stages the real sources) - no glintfx.so, no CMake, no
// .pc file, same technique every other fixture in this directory
// already uses.

namespace {

// Opens `display`, opens `seat` against it, round-trips once (wl_seat.
// capabilities is sent on bind - one roundtrip is enough to have it
// dispatched, same reasoning tests/container/seat_test.cpp's own main()
// already documents), and confirms at least one announcement fired.
// Returns false and prints the reason on any failure - never partially
// tears down what it already opened (the caller's own cleanup handles
// that uniformly for both seats, same shape open_and_check_catalog()
// already uses in tests/container/two_displays_test.cpp).
bool open_seat_and_check_announced(glintfx::platform::wayland_display_adapter &display,
                                   glintfx::platform::wayland_seat_adapter &seat,
                                   const char *label) {
    const glintfx::gltfx_rslt<void> display_opened = display.open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "two_seats_test: %s display.open() failed: %s\n", label,
            std::string(glintfx::gltfx_err_code_name(display_opened.err().code())).c_str());
        return false;
    }

    const glintfx::gltfx_rslt<void> seat_opened = seat.open(display);
    if (seat_opened.has_error()) {
        std::fprintf(stderr, "two_seats_test: %s seat.open() failed: %s\n", label,
                     std::string(glintfx::gltfx_err_code_name(seat_opened.err().code())).c_str());
        return false;
    }

    const glintfx::gltfx_rslt<void> roundtripped = display.roundtrip();
    if (roundtripped.has_error()) {
        std::fprintf(stderr, "two_seats_test: %s roundtrip() failed: %s\n", label,
                     std::string(glintfx::gltfx_err_code_name(roundtripped.err().code())).c_str());
        return false;
    }

    if (seat.last_change() == 0) {
        std::fprintf(stderr,
                     "two_seats_test: %s seat had no capabilities/name event observed at all\n",
                     label);
        return false;
    }

    std::fprintf(stdout, "two_seats_test: %s seat open, last_change=%llu\n", label,
                 static_cast<unsigned long long>(seat.last_change()));
    return true;
}

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (connect_smoke.cpp's own header
    // comment).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter first_display;
    glintfx::platform::wayland_display_adapter second_display;
    glintfx::platform::wayland_seat_adapter first_seat;
    glintfx::platform::wayland_seat_adapter second_seat;

    if (!open_seat_and_check_announced(first_display, first_seat, "first")) {
        return EXIT_FAILURE;
    }
    if (!open_seat_and_check_announced(second_display, second_seat, "second")) {
        return EXIT_FAILURE;
    }

    // D-WS-7's own guarantee, exercised for real on the Wayland side
    // too: closing the FIRST seat must never disturb the SECOND seat's
    // own, independent connection and listener registration.
    first_seat.close();
    if (first_seat.is_open()) {
        std::fprintf(stderr,
                     "two_seats_test: first_seat.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    if (!second_seat.is_open()) {
        std::fprintf(
            stderr,
            "two_seats_test: closing first_seat left second_seat closed too (shared state)\n");
        return EXIT_FAILURE;
    }
    if (second_seat.last_change() == 0) {
        std::fprintf(
            stderr, "two_seats_test: closing first_seat corrupted second_seat's own last_change\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout,
                 "two_seats_test: closing first_seat left second_seat untouched (is_open() still "
                 "true, last_change=%llu)\n",
                 static_cast<unsigned long long>(second_seat.last_change()));

    second_seat.close();
    if (second_seat.is_open()) {
        std::fprintf(stderr,
                     "two_seats_test: second_seat.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }

    first_display.close();
    second_display.close();

    std::fprintf(stdout, "two_seats_test: both seats closed cleanly\n");
    // MEASURED-COLLECTOR: same one shared key two_displays_test.cpp's
    // own pair already uses (this file's own header comment) - "two
    // independent seats coexisted in the same process without
    // colliding, at all" is the common ground both systems genuinely
    // share; the mechanism-specific facts (device_notification_count()
    // on Windows, last_change() per seat here) are printed above,
    // never as a MEASURED key with no equivalent on the other side.
    std::fprintf(stdout, "MEASURED two_seats_test.both_opened=1\n");

    return EXIT_SUCCESS;
}
