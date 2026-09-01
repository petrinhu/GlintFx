// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"

// fatal_error_smoke.cpp - WL-DISPLAY fatia C, TDD case C (w4-plano.md
// sec. 3.1.C): the container-integration case the plan requires -
// "teste em container que corta a conexao (fechar o compositor
// aninhado do teste) e exige o codigo certo em vez de crash". Same
// staging shape as connect_smoke.cpp/registry_smoke.cpp (ARCH-PORTS
// R4, WL-DISPLAY B): the REAL wayland_display_adapter/global_catalog/
// err.cpp/err_code.cpp production sources, compiled straight into this
// one executable, no glintfx.so involved.
//
// std::system("pkill -x kwin_wayland") is standard C library
// (<cstdlib>), not a third-party dependency - GODS_LAWS.md L-07 does
// not reach it, same reasoning tests/container/check_isolation.sh's
// own header already applies to its own host-side tooling calls.
// procps-ng (providing pkill) is already in this image, installed for
// check_isolation.sh's own `docker exec ... pgrep` calls.
//
// GODS_LAWS.md L-09: this binary never touches the leader's own
// session - it kills a compositor process running INSIDE this same
// isolated container, the one run_compositor.sh (this directory)
// started for THIS test alone.

int main() {
    glintfx::platform::wayland_display_adapter adapter;

    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "fatal_error_smoke: open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    if (adapter.has_fatal_error()) {
        std::fprintf(stderr, "fatal_error_smoke: has_fatal_error() true right after a "
                              "successful open() - the latch fired too early\n");
        return EXIT_FAILURE;
    }

    // A roundtrip against a compositor that is still alive must
    // succeed, and must NOT latch has_fatal_error().
    glintfx::gltfx_rslt<void> first_roundtrip = adapter.roundtrip();
    if (first_roundtrip.has_error()) {
        std::fprintf(stderr, "fatal_error_smoke: roundtrip() against a live compositor failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(first_roundtrip.error().code())).c_str());
        return EXIT_FAILURE;
    }
    if (adapter.has_fatal_error()) {
        std::fprintf(stderr, "fatal_error_smoke: has_fatal_error() true after a roundtrip "
                              "that reported success\n");
        return EXIT_FAILURE;
    }

    // Cuts the connection out from under this adapter: kills the
    // compositor process THIS SAME CONTAINER'S run_compositor.sh
    // started, closing the socket this adapter is connected to.
    if (std::system("pkill -x kwin_wayland") != 0) {
        std::fprintf(stderr, "fatal_error_smoke: pkill -x kwin_wayland did not report success\n");
        return EXIT_FAILURE;
    }
    // Give the kernel a moment to actually tear the socket down
    // before this same process tries to use it again - a signal sent
    // is not synchronous with the peer socket closing.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // THIS is the case the plan's own TDD case C is about: a
    // roundtrip on a connection whose peer just died must come back
    // as an ORDINARY gltfx_rslt<void> error - never a crash, never an
    // abort - and must latch has_fatal_error().
    glintfx::gltfx_rslt<void> after_cut = adapter.roundtrip();
    if (after_cut.has_value()) {
        std::fprintf(stderr, "fatal_error_smoke: roundtrip() reported SUCCESS after the "
                              "compositor was killed - the fatal path never fired\n");
        return EXIT_FAILURE;
    }
    if (after_cut.error().code() != glintfx::gltfx_err_code::platform_failure) {
        std::fprintf(stderr, "fatal_error_smoke: wrong error code after the cut: %s\n",
                     std::string(glintfx::gltfx_err_code_name(after_cut.error().code())).c_str());
        return EXIT_FAILURE;
    }
    if (!adapter.has_fatal_error()) {
        std::fprintf(stderr, "fatal_error_smoke: has_fatal_error() still false after a "
                              "roundtrip() that reported the connection dead\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "fatal_error_smoke: roundtrip() after the cut reported %s "
                          "(os_error_code=%lld), has_fatal_error() == true\n",
                 std::string(glintfx::gltfx_err_code_name(after_cut.error().code())).c_str(),
                 static_cast<long long>(after_cut.error().os_error_code()));

    // A SECOND roundtrip() call, on the already-latched adapter, must
    // ALSO come back as an ordinary error - never crash - proving the
    // latch actually holds rather than firing once and then trying
    // libwayland again.
    glintfx::gltfx_rslt<void> second_after_cut = adapter.roundtrip();
    if (second_after_cut.has_value()) {
        std::fprintf(stderr, "fatal_error_smoke: SECOND roundtrip() after the cut reported "
                              "success - the latch did not hold\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "fatal_error_smoke: second roundtrip() after the cut also reported "
                          "an error, as expected - no crash\n");

    // close() on a fatally-errored adapter must still work cleanly.
    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "fatal_error_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "fatal_error_smoke: closed cleanly after a fatal error - no crash\n");

    return EXIT_SUCCESS;
}
