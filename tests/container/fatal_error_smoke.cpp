// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdint>
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
//
// CONT-WARMUP C-2/C-3 (docs/plano-fecho-w7b.md D-10, GODS_LAWS.md
// L-17 "gemeo", L-44): two things changed here in the same fatia. (1)
// the fixed `sleep_for(500ms)` after the `pkill` below only ever
// proved "half a second passed", never that the process actually died
// - on a loaded CI runner a slow-to-die process could still be alive
// when the sleep ended, so this now POLLS `pgrep` until the process is
// genuinely gone (bounded, GODS_LAWS.md L-40: loudly fails rather than
// looping forever if it refuses to die), the same "prove it, do not
// assume a delay was enough" fix `wait_for_compositor_ready()`
// (run_compositor.sh, CONT-WARMUP C-1) already applies to the mirror
// problem of waiting for the compositor to come UP. (2) display_
// adapter.cpp's own wait_for_incoming_data() conserto (this SAME
// commit) is correct by poll(2)'s own general contract (POLLHUP/
// POLLERR without POLLIN now fall through to read_and_dispatch_
// incoming() instead of being absorbed as "nothing to read yet";
// POLLNVAL latches directly) - but D-10's own PROMISE that this makes
// the FIRST pump_events()/wait_events() call after the kill latch is
// REFUTED BY MEASUREMENT (this fatia's own report, four independent
// container runs, deterministic every time): instrumented poll()
// shows revents=POLLIN|POLLHUP on the FIRST call, never POLLHUP alone
// - this kernel reports POLLIN alongside POLLHUP for a peer-closed
// AF_UNIX stream socket (POLLIN here also means "read() will not
// block", which covers returning 0 for EOF). The first read_events()
// call genuinely, successfully drains one last legitimately-buffered
// protocol chunk from the compositor's own teardown; only the SECOND
// call observes the true EOF and latches. Measured identical against
// the code BEFORE this conserto too - the old composite condition
// never took its buggy branch here either, since POLLIN was always
// present. What actually closes: the untamed `kPumpAttemptsAfterCut`
// loop this file used to need (up to TEN tries, "whichever eventually
// catches it" - D-10's own diagnosis: "o teste se ajustando ao
// programa", the exact "teste que copia o valor da implementacao"
// family) becomes a TIGHT, EXPLAINED, DETERMINISTIC bound of TWO - the
// exact number measured, never a guess with slack padded in. A second
// adapter (`gemeo_adapter` below) proves the SAME shape holds for
// `wait_events()` too (L-17: the write-side twin, bounded_output_
// wait.cpp, already treated POLLERR|POLLHUP|POLLNVAL as failure - the
// two read-side entry points now agree with each other on the LOGIC,
// even though this exact kill-timing never exercises the specific
// branch that changed).

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

    // CONT-WARMUP C-3 (docs/plano-fecho-w7b.md D-10): a SECOND, freshly-
    // opened adapter, connected to the SAME compositor - opened BEFORE
    // the kill below, same as `adapter` above. This one proves the
    // wait_events() gemeo of the pump_events() fix, and it has to be a
    // DIFFERENT adapter instance: once `adapter` latches has_fatal_
    // error() further down, every one of ITS OWN calls short-circuits
    // through the "already known dead" guard (roundtrip()'s/pump_
    // events()'s/wait_events()'s own header comments) without ever
    // reaching wait_for_incoming_data() again - that guard would make
    // a wait_events() call on the SAME adapter trivially fast and
    // trivially fatal for the WRONG reason, never exercising the real
    // poll()/read_events() path this fatia actually changed.
    glintfx::platform::wayland_display_adapter gemeo_adapter;
    const glintfx::gltfx_rslt<void> gemeo_opened = gemeo_adapter.open();
    if (gemeo_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: gemeo_adapter.open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(gemeo_opened.err().code())).c_str());
        return EXIT_FAILURE;
    }
    if (gemeo_adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: gemeo_adapter.has_fatal_error() true right "
                    "after a successful open()\n");
        return EXIT_FAILURE;
    }

    // Cuts the connection out from under BOTH adapters above: kills the
    // compositor process THIS SAME CONTAINER'S run_compositor.sh
    // started, closing the sockets both `adapter` and `gemeo_adapter`
    // are connected to (one process, two independent client sockets).
    // LINT-CONTAINER-SMOKES: fixed string literal, no environment/user/
    // argv input ever reaches this command (see this file's own header
    // comment above for why std::system() itself is in bounds), and this
    // binary only ever runs inside the throwaway wayland-container image
    // (GODS_LAWS.md L-09) - pkill by exact process name is the direct way
    // to kill the compositor run_compositor.sh started, to prove
    // has_fatal_error() latches when the connection dies underneath each
    // adapter.
    // NOLINTNEXTLINE(bugprone-command-processor,cert-env33-c) reason: see comment immediately above
    if (std::system("pkill -x kwin_wayland") != 0) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: pkill -x kwin_wayland did not report success\n");
        return EXIT_FAILURE;
    }

    // CONT-WARMUP C-2 (docs/plano-fecho-w7b.md D-10): waits for the
    // compositor PROCESS to actually be gone, proven by `pgrep` itself
    // reporting nothing found - a fixed `sleep_for(500ms)` (this file's
    // history before this fatia) only ever proved "half a second
    // passed", never that the kill landed; a slow-to-die process on a
    // loaded CI runner could still be alive when the sleep ended, and
    // the pump_events()/wait_events() calls below need the socket to be
    // genuinely, deterministically gone - the fixture's whole point
    // after this fatia is a TIGHT, DETERMINISTIC bound on how many
    // calls it takes to latch (measured: exactly two, see this file's
    // own header comment - not "the first call", D-10's own promise,
    // refuted by measurement; and never the untamed "up to ten" this
    // file had before), so a racy "probably dead by now" wait would
    // undermine the exact determinism being proven. Bounded (GODS_LAWS.md
    // L-40): fails loudly instead of looping forever if the process
    // refuses to die.
    constexpr int kProcessExitPollAttempts = 50;
    bool compositor_process_gone = false;
    for (int attempt = 0; attempt < kProcessExitPollAttempts; ++attempt) {
        // NOLINTNEXTLINE(bugprone-command-processor,cert-env33-c) reason: see file header above
        if (std::system("pgrep -x kwin_wayland >/dev/null 2>&1") != 0) {
            compositor_process_gone = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!compositor_process_gone) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "fatal_error_smoke: kwin_wayland ainda visivel ao pgrep apos %d "
            "tentativa(s) de 100ms - pkill nao terminou o processo a tempo\n",
            kProcessExitPollAttempts);
        return EXIT_FAILURE;
    }

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
    // CONT-WARMUP C-2's own fechamento, CORRIGIDO (docs/plano-fecho-w7b.md
    // D-10, GODS_LAWS.md L-44): D-10's own text promised the FIRST
    // pump_events() call would already latch. Measured against a REAL
    // kwin_wayland, four independent runs, deterministic every time
    // (this fatia's own report, /var/tmp/glintfx-plan/impl-cont-warmup.md):
    // that promise is REFUTED. Instrumented poll() shows revents=POLLIN|
    // POLLHUP on the connection's fd on the FIRST call - never POLLHUP
    // ALONE, the shape D-10's own research assumed - because this
    // kernel reports POLLIN alongside POLLHUP for a peer-closed AF_UNIX
    // stream socket (POLLIN meaning "read() will not block", which
    // covers returning 0 for EOF too). The first read_events() call
    // genuinely, successfully drains one last legitimately-buffered
    // protocol chunk from the compositor's own teardown (rc=0, no
    // error); only the SECOND call, with nothing left to drain, sees
    // wl_display_read_events() fail with EPIPE and latches. This holds
    // for the code BEFORE this fatia's conserto too (measured the same
    // way, same 2-call shape) - for THIS exact scenario, POLLIN was
    // always present, so wait_for_incoming_data()'s old composite
    // condition never took its buggy branch either. The conserto to
    // wait_for_incoming_data() (this same commit) is kept - it is
    // correct by poll(2)'s own general contract and closes a REAL gap
    // (a hangup with genuinely NO trailing data, or POLLNVAL, which the
    // old composite condition mishandled) - it simply is not the thing
    // THIS specific fixture's kill-timing exercises. What actually
    // closes here, replacing the old file's own header comment: the
    // untamed "up to ten attempts, whichever eventually catches it"
    // tolerance (D-10's own diagnosis: "o teste se ajustando ao
    // programa") becomes a TIGHT, EXPLAINED, DETERMINISTIC bound of
    // two - the exact number this fatia measured, not a guess with
    // slack padded in.
    constexpr int kPumpAttemptsAfterCut = 2;
    glintfx::gltfx_rslt<void> pumped_after_cut = glintfx::gltfx_rslt<void>::ok();
    int pump_calls_taken = 0;
    for (int attempt = 0; attempt < kPumpAttemptsAfterCut && !adapter.has_fatal_error();
         ++attempt) {
        pumped_after_cut = adapter.pump_events();
        pump_calls_taken = attempt + 1;
    }
    if (!adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "fatal_error_smoke: pump_events() never latched has_fatal_error() within "
            "%d attempt(s) (measured contract: exactly 2) after the process was "
            "confirmed gone\n",
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
        "fatal_error_smoke: pump_events() latched on call #%d (medido: exatamente 2) "
        "reportando %s (os_error_code=%lld), has_fatal_error() == true\n",
        pump_calls_taken,
        std::string(glintfx::gltfx_err_code_name(pumped_after_cut.err().code())).c_str(),
        static_cast<long long>(pumped_after_cut.err().os_error_code()));
    // MEASURED-COLLECTOR: same shape as fatal_error_smoke.os_error_code
    // below, for the pump_events() path specifically - Linux-only key
    // (no Windows equivalent fixture severs its own transport yet).
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.pump_os_error_code=%lld\n",
        static_cast<long long>(pumped_after_cut.err().os_error_code()));
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.pump_calls_until_fatal=%d\n", pump_calls_taken);

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

    // CONT-WARMUP C-3, CORRIGIDO (docs/plano-fecho-w7b.md D-10,
    // GODS_LAWS.md L-17 "gemeo", L-44): a mesma refutacao medida do
    // bloco pump_events() acima se aplica aqui - a mesma sonda
    // instrumentada (relatorio desta fatia) mostrou wait_events()
    // seguindo IDENTICO: primeira chamada com revents=POLLIN|POLLHUP,
    // read_events() bem-sucedido (uma ultima mensagem legitimamente
    // bufferizada), latch so' na SEGUNDA. `gemeo_adapter` - um cliente
    // com NADA na fila para escrever (o cenario do plano: "cliente sem
    // nada a escrever") - continua provando o que de fato importa: NAO
    // um laco ocupado (cada chamada volta bem ANTES do orcamento
    // inteiro - se tivesse esperado o orcamento todo, seria sinal de
    // que POLLHUP nao acordou o poll() na hora) e um bound APERTADO e
    // DETERMINISTICO (2, nao "ate dez" nem "para sempre"), nunca a
    // suposicao errada de "so' uma chamada".
    constexpr std::uint32_t kGemeoWaitEventsBudgetMs = 2000;
    constexpr int kGemeoWaitAttemptsAfterCut = 2;
    glintfx::gltfx_rslt<bool> gemeo_waited = glintfx::gltfx_rslt<bool>::ok(false);
    int gemeo_wait_calls_taken = 0;
    long long gemeo_wait_elapsed_ms = 0;
    for (int attempt = 0; attempt < kGemeoWaitAttemptsAfterCut && !gemeo_adapter.has_fatal_error();
         ++attempt) {
        const auto gemeo_wait_started = std::chrono::steady_clock::now();
        gemeo_waited = gemeo_adapter.wait_events(kGemeoWaitEventsBudgetMs);
        gemeo_wait_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - gemeo_wait_started)
                                    .count();
        gemeo_wait_calls_taken = attempt + 1;
        // NAO um laco ocupado: CADA chamada volta bem ANTES do orcamento
        // inteiro - se alguma tivesse esperado o orcamento todo, isso
        // provaria que POLLHUP nao acordou o poll() na hora (o defeito
        // antigo, so' que absorvido de outra forma).
        if (gemeo_wait_elapsed_ms >= kGemeoWaitEventsBudgetMs) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "fatal_error_smoke: wait_events() call #%d gastou o orcamento inteiro "
                "(%lldms >= %ums) em vez de acordar assim que o poll() teve algo a "
                "reportar\n",
                gemeo_wait_calls_taken, gemeo_wait_elapsed_ms, kGemeoWaitEventsBudgetMs);
            return EXIT_FAILURE;
        }
    }
    if (!gemeo_adapter.has_fatal_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "fatal_error_smoke: wait_events() de gemeo_adapter nunca latchou "
            "has_fatal_error() dentro de %d tentativa(s) (contrato medido: exatamente 2) "
            "apos o processo confirmado morto\n",
            kGemeoWaitAttemptsAfterCut);
        return EXIT_FAILURE;
    }
    if (!gemeo_waited.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: gemeo_adapter.has_fatal_error() latched, but the "
                    "wait_events() call that latched it did not itself report an error\n");
        return EXIT_FAILURE;
    }
    if (gemeo_waited.err().code() != glintfx::gltfx_err_code::platform_failure) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: wrong error code from wait_events() after the cut: %s\n",
            std::string(glintfx::gltfx_err_code_name(gemeo_waited.err().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "fatal_error_smoke: wait_events(%u) de gemeo_adapter latchou na chamada #%d "
        "(medido: exatamente 2) reportando %s (os_error_code=%lld) em %lldms, "
        "has_fatal_error() == true\n",
        kGemeoWaitEventsBudgetMs, gemeo_wait_calls_taken,
        std::string(glintfx::gltfx_err_code_name(gemeo_waited.err().code())).c_str(),
        static_cast<long long>(gemeo_waited.err().os_error_code()), gemeo_wait_elapsed_ms);
    // MEASURED-COLLECTOR: same shape as fatal_error_smoke.os_error_code
    // above, for the wait_events() path specifically - Linux-only key
    // (no Windows equivalent fixture severs its own transport yet).
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.wait_events_os_error_code=%lld\n",
        static_cast<long long>(gemeo_waited.err().os_error_code()));
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.wait_events_elapsed_ms=%lld\n", gemeo_wait_elapsed_ms);
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED fatal_error_smoke.wait_events_calls_until_fatal=%d\n",
        gemeo_wait_calls_taken);

    // close() on a fatally-errored adapter must still work cleanly -
    // both adapters.
    adapter.close();
    if (adapter.is_open()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    gemeo_adapter.close();
    if (gemeo_adapter.is_open()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "fatal_error_smoke: gemeo_adapter.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout, "fatal_error_smoke: closed cleanly after a fatal error - no crash\n");

    return EXIT_SUCCESS;
}
