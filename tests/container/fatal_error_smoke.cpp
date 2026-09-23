// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "checked_stdio.hpp"
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
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // all ten fixtures in this family (connect_smoke.cpp's own header
    // comment on this exact line, docs/plano-w6b-placa-e-laco.md fatia
    // 1, 06/09/2026; corrected same day, connect_smoke.cpp's own
    // header comment again, after `_IOLBF` with `size` 0 crashed the
    // Windows CI job with 0xC0000409 - MSVC's setvbuf rejects that
    // combination outside its documented 2 <= size <= INT_MAX range,
    // while `_IONBF` ignores `size`/`buffer` entirely).
    glintfx::container_fixture::checked_setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;

    const glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }
    if (adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: has_fatal_error() true right after a "
                    "successful open() - the latch fired too early\n");
        return EXIT_FAILURE;
    }

    // A roundtrip against a compositor that is still alive must
    // succeed, and must NOT latch has_fatal_error().
    const glintfx::gltfx_rslt<void> first_roundtrip = adapter.roundtrip();
    if (first_roundtrip.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: roundtrip() against a live compositor failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(first_roundtrip.err().code())).c_str());
        return EXIT_FAILURE;
    }
    if (adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: has_fatal_error() true after a roundtrip "
                    "that reported success\n");
        return EXIT_FAILURE;
    }

    // Cuts the connection out from under this adapter: kills the
    // compositor process THIS SAME CONTAINER'S run_compositor.sh
    // started, closing the socket this adapter is connected to.
    // LINT-CONTAINER-SMOKES: fixed string literal, no environment/user/
    // argv input ever reaches this command (see this file's own header
    // comment above for why std::system() itself is in bounds), and this
    // binary only ever runs inside the throwaway wayland-container image
    // (GODS_LAWS.md L-09) - pkill by exact process name is the direct way
    // to kill the compositor run_compositor.sh started, to prove
    // has_fatal_error() latches when the connection dies underneath this
    // adapter.
    // NOLINTNEXTLINE(bugprone-command-processor,cert-env33-c) reason: see comment immediately above
    if (std::system("pkill -x kwin_wayland") != 0) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: pkill -x kwin_wayland did not report success\n");
        return EXIT_FAILURE;
    }
    // Give the kernel a moment to actually tear the socket down
    // before this same process tries to use it again - a signal sent
    // is not synchronous with the peer socket closing.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // FATAL-SMOKE-PUMP (TODO.md, GODS_LAWS.md L-35/L-36/L-40): before
    // this block, NOTHING in this fixture family ever called
    // pump_events() on a connection whose peer just died - `grep -c
    // pump tests/container/fatal_error_smoke.cpp` returned zero
    // (pump_smoke.cpp's own 50-iteration loop only ever runs against a
    // LIVE compositor, GODS_LAWS.md L-17's own "gemeo" check). pump_
    // events() is a WHOLLY DIFFERENT code path from the roundtrip()
    // below it - display_adapter.cpp's own four-step dispatch_ready_
    // events() sequence (drain_pending_and_prepare_read() / flush_
    // with_retry() / wait_for_incoming_data() / read_and_dispatch_
    // incoming()), never wl_display_roundtrip() itself.
    //
    // MEASURED live against a real kwin_wayland, 22/09/2026 (CONT-
    // WARMUP drenagem): the FIRST pump_events() call right after the
    // kill comes back ok(), has_fatal_error() still false - it lands
    // on wait_for_incoming_data()'s own composite condition (display_
    // adapter.cpp: "poll_result == 0 || (incoming.revents & POLLIN) ==
    // 0"), on the branch a plain roundtrip() never takes at all (a
    // POLLHUP-without-POLLIN wakeup absorbed as "nothing to read yet",
    // never latched fatal by itself). The CTO confirmed this is its
    // OWN gap, not a twin of the read()-on-a-dead-descriptor defect
    // the roundtrip() calls below already cover. The SECOND call is
    // what actually latches: flush_with_retry()'s own wl_display_
    // flush() finally observes the socket is gone (EPIPE, MEASURED-
    // COLLECTOR below) and sets m_fatal. The bounded loop below never
    // hard-codes "exactly the second try" - that count is an observed
    // fact of THIS kernel/timing, not a contract this fixture gets to
    // assume holds everywhere else.
    constexpr int kPumpAttemptsAfterCut = 10;
    glintfx::gltfx_rslt<void> pumped_after_cut = glintfx::gltfx_rslt<void>::ok();
    for (int attempt = 0; attempt < kPumpAttemptsAfterCut && !adapter.has_fatal_error();
         ++attempt) {
        pumped_after_cut = adapter.pump_events();
        if (pumped_after_cut.has_error()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "fatal_error_smoke: pump_events() never latched has_fatal_error() "
            "within %d attempt(s) after the compositor was killed\n",
            kPumpAttemptsAfterCut);
        return EXIT_FAILURE;
    }
    if (!pumped_after_cut.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: has_fatal_error() latched, but the pump_events() "
                    "call that latched it did not itself report an error\n");
        return EXIT_FAILURE;
    }
    if (pumped_after_cut.err().code() != glintfx::gltfx_err_code::platform_failure) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: wrong error code from pump_events() after the cut: %s\n",
            std::string(glintfx::gltfx_err_code_name(pumped_after_cut.err().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "fatal_error_smoke: pump_events() after the cut reported %s "
        "(os_error_code=%lld), has_fatal_error() == true\n",
        std::string(glintfx::gltfx_err_code_name(pumped_after_cut.err().code())).c_str(),
        static_cast<long long>(pumped_after_cut.err().os_error_code()));
    // MEASURED-COLLECTOR: same shape as fatal_error_smoke.os_error_code
    // below, for the pump_events() path specifically - Linux-only key
    // (no Windows equivalent fixture severs its own transport yet).
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.pump_os_error_code=%lld\n",
        static_cast<long long>(pumped_after_cut.err().os_error_code()));

    // The case the plan's own TDD case C is about: a roundtrip on a
    // connection whose peer just died must come back as an ORDINARY
    // gltfx_rslt<void> error - never a crash, never an abort. By this
    // point pump_events() above has ALREADY latched has_fatal_error()
    // (it is what first discovered the cut) - this call now exercises
    // roundtrip()'s OWN m_fatal guard ("never a second real roundtrip
    // attempt on a connection already known to be dead", this class's
    // own header comment on the method), proving that guard reports
    // the SAME diagnostic shape a caller who never touches pump_
    // events() at all would see.
    const glintfx::gltfx_rslt<void> after_cut = adapter.roundtrip();
    if (after_cut.has_value()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: roundtrip() reported SUCCESS after the "
                    "compositor was killed - the fatal path never fired\n");
        return EXIT_FAILURE;
    }
    if (after_cut.err().code() != glintfx::gltfx_err_code::platform_failure) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: wrong error code after the cut: %s\n",
            std::string(glintfx::gltfx_err_code_name(after_cut.err().code())).c_str());
        return EXIT_FAILURE;
    }
    if (!adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: has_fatal_error() still false after a "
                    "roundtrip() that reported the connection dead\n");
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "fatal_error_smoke: roundtrip() after the cut reported %s "
        "(os_error_code=%lld), has_fatal_error() == true\n",
        std::string(glintfx::gltfx_err_code_name(after_cut.err().code())).c_str(),
        static_cast<long long>(after_cut.err().os_error_code()));
    // MEASURED-COLLECTOR: the raw OS error code a severed Wayland
    // socket reports on this kernel - Linux-only key (Windows has no
    // equivalent fixture that severs its own transport yet), lands in
    // "so de um lado" until one exists.
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.os_error_code=%lld\n",
        static_cast<long long>(after_cut.err().os_error_code()));

    // A SECOND roundtrip() call, on the already-latched adapter, must
    // ALSO come back as an ordinary error - never crash - proving the
    // latch actually holds rather than firing once and then trying
    // libwayland again.
    const glintfx::gltfx_rslt<void> second_after_cut = adapter.roundtrip();
    if (second_after_cut.has_value()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: SECOND roundtrip() after the cut reported "
                    "success - the latch did not hold\n");
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout, "fatal_error_smoke: second roundtrip() after the cut also reported "
                "an error, as expected - no crash\n");

    // close() on a fatally-errored adapter must still work cleanly.
    adapter.close();
    if (adapter.is_open()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout, "fatal_error_smoke: closed cleanly after a fatal error - no crash\n");

    return EXIT_SUCCESS;
}
