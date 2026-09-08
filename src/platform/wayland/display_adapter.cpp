// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/display_adapter.hpp"

#include <wayland-client.h>

#include <poll.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <new>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/bounded_output_wait.hpp"
#include "platform/wayland/connection_failure.hpp"
#include "platform/wayland/flush_retry_policy.hpp"

// display_adapter.cpp - see display_adapter.hpp's own header comment
// for scope. ARCH-PORTS's own connect/disconnect (TDD case R3,
// GODS_LAWS.md L-20's declared exception: "adaptador que só encaminha
// chamada ao sistema operacional... o teste honesto é de integração e
// não unitário") is now EXTENDED by WL-DISPLAY fatia B with the
// registry and its listener - proven the same way ARCH-PORTS proved
// its own half: tests/display_connect_failure_test.cpp for the
// no-compositor refusal path (runnable on any Linux CI leg, unchanged
// by this fatia - the refusal happens at wl_display_connect(), before
// any registry code below is ever reached) and the container
// integration case (TEST-WLCONT, GODS_LAWS.md L-09) for the
// connect-to-a-real-compositor path, now also enumerating the globals
// a real compositor announces.
//
// registry_global()/registry_global_remove() below are called back by
// libwayland-client's own C event-dispatch machinery (wl_display_
// roundtrip(), later the pump fatia D adds) with a bare `void *data`
// it does nothing with except hand back unchanged - GODS_LAWS.md L-22:
// NO EXCEPTION may unwind across that C stack frame. global_catalog::
// insert() can fail to allocate via its std::string/std::vector
// members, but it never lets that surface as an exception here: it
// catches std::bad_alloc INTERNALLY (global_catalog.cpp's own comment
// on insert()) and reports the failure through its noexcept bool
// return instead - a degraded catalog is now something a caller CAN
// observe by checking that return, rather than a try/catch at this
// call site swallowing it with no signal at all.
//
// CONSERTO (varredura de 07/09/2026, mesma familia de bugs de gfx_
// open_only_fixation.cpp): insert()'s own try/catch only guards
// ALLOCATIONS INSIDE ITS OWN BODY. The `std::string(interface)`
// temporary below is an ARGUMENT, built in THIS frame - registry_
// global()'s own - before insert() is ever entered. A previous
// version of this comment claimed "no try/catch needed here anymore",
// which was wrong: that construction could still throw std::bad_alloc
// and escape this noexcept callback, calling std::terminate() before
// insert()'s own guard ever had a chance to run. Guarded below with
// the SAME idiom.

namespace glintfx::platform {

namespace {

constexpr wl_registry_listener kRegistryListener = {
    .global = &wayland_display_adapter::registry_global,
    .global_remove = &wayland_display_adapter::registry_global_remove,
};

} // namespace

wayland_display_adapter::~wayland_display_adapter() { close(); }

void wayland_display_adapter::registry_global(void *data, wl_registry * /*registry*/,
                                              std::uint32_t name, const char *interface,
                                              std::uint32_t version) noexcept {
    auto *self = static_cast<wayland_display_adapter *>(data);
    // See this file's own header comment (CONSERTO 07/09/2026):
    // insert()'s own try/catch guards its BODY, never the std::string(
    // interface) argument built HERE, in this frame, before insert()
    // is even called. Guarded the same way - a failed construction
    // simply skips this one global (same observable "not cataloged"
    // shape insert()'s own false==alloc-failed return already gives a
    // caller who checks it); the catalog is left exactly as it was
    // before the call, same guarantee insert()'s own header comment
    // already documents for its own internal failure.
    try {
        self->m_globals.insert(name, interface != nullptr ? std::string(interface) : std::string(),
                               version);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: intentionally empty,
                                       // discarded on purpose, same as insert()'s own bool return
                                       // just above - this callback has no diagnostics channel to
                                       // route either failure through yet (out of this narrow
                                       // fix's own scope).
    }
}

void wayland_display_adapter::registry_global_remove(void *data, wl_registry * /*registry*/,
                                                     std::uint32_t name) noexcept {
    auto *self = static_cast<wayland_display_adapter *>(data);
    self->m_globals.remove(name);
}

gltfx_rslt<void> wayland_display_adapter::open() noexcept {
    // nullptr: resolve the DEFAULT socket the same way every Wayland
    // client does (reads WAYLAND_DISPLAY, falling back to the literal
    // name "wayland-0" resolved INSIDE XDG_RUNTIME_DIR) - never a
    // hand-picked socket name. GODS_LAWS.md L-09 armadilha conhecida,
    // named explicitly so the next reader of this file does not
    // re-derive it the hard way: UNSETTING WAYLAND_DISPLAY does NOT
    // protect a test from reaching a real compositor, because the
    // fallback name is embedded in libwayland-client itself, not read
    // from that variable's absence. The only real protection is
    // pointing XDG_RUNTIME_DIR at an empty, private directory - which
    // is exactly what tests/display_connect_failure_test.cpp does, and
    // exactly why THIS function never has to know it is being tested
    // that way: from here, a private empty XDG_RUNTIME_DIR and a
    // machine that genuinely has no compositor running look identical -
    // wl_display_connect() returns nullptr either way.
    wl_display *display = wl_display_connect(nullptr);
    if (display == nullptr) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }

    wl_registry *registry = wl_display_get_registry(display);
    if (registry == nullptr) {
        wl_display_disconnect(display);
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }

    wl_registry_add_listener(registry, &kRegistryListener, this);

    // ONE roundtrip closes the initial burst of `global` events every
    // compositor emits right after get_registry() (w4-plano.md
    // sec. 1.2/3.1.B, wayland-book's own registry/binding chapter) -
    // by the time this call returns successfully, m_globals holds
    // every global that was already known when this connection opened.
    // A -1 return here is this fatia's OWN open()-failure path only:
    // WL-DISPLAY fatia C is what latches the ADAPTER ITSELF into a
    // permanently-unusable state for a roundtrip failing on an
    // ALREADY-open connection - this call happens strictly before
    // m_display is ever assigned below, so open() simply reports
    // failure and tears down what it had already created, the same
    // shape a refused connect() or get_registry() above already has.
    if (wl_display_roundtrip(display) == -1) {
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::platform_failure));
    }

    m_display = display;
    m_registry = registry;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void *> wayland_display_adapter::bind(const wayland_global &global,
                                                 const wl_interface &interface,
                                                 std::uint32_t supported_version) noexcept {
    if (!is_open()) {
        return gltfx_rslt<void *>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        // Same "never a second real call on a connection already
        // known to be dead" rule roundtrip()/pump_events() already
        // apply (see this class's own header comment on has_fatal_
        // error()) - a bind() attempt is a protocol request exactly
        // like any other, and this connection cannot send one.
        return gltfx_rslt<void *>::err(build_connection_failure(m_display));
    }

    // The one call site this project's clamp_version() rule (global_
    // catalog.hpp's own header comment) exists for: never ask the
    // compositor for a version it did not just announce.
    const std::uint32_t version = global_catalog::clamp_version(supported_version, global.version);
    void *proxy = wl_registry_bind(m_registry, global.name, &interface, version);
    if (proxy == nullptr) {
        // wl_registry_bind() only returns null when it fails to
        // allocate the LOCAL proxy object - this connection's own
        // request/event bookkeeping is now out of sync with whatever
        // the compositor believes was bound, which is exactly the
        // "permanently unusable" shape has_fatal_error() exists to
        // latch (see this file's own header comment on ARCH-PORTS'S
        // exception-safety contract). Reported through the SAME
        // channel as every other refusal here - never a bare nullptr
        // a caller could dereference three calls later.
        m_fatal = true;
        return gltfx_rslt<void *>::err(build_connection_failure(m_display));
    }
    return gltfx_rslt<void *>::ok(proxy);
}

gltfx_rslt<void> wayland_display_adapter::roundtrip() noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        // Already latched by an EARLIER call: report the SAME
        // diagnostic without touching m_display again (see this
        // method's own header comment - "never a second real
        // roundtrip attempt on a connection already known to be
        // dead"). wl_display_get_error() itself is always safe to
        // call, even here - see build_connection_failure()'s own comment.
        return gltfx_rslt<void>::err(build_connection_failure(m_display));
    }
    if (wl_display_roundtrip(m_display) == -1) {
        m_fatal = true;
        return gltfx_rslt<void>::err(build_connection_failure(m_display));
    }
    return gltfx_rslt<void>::ok();
}

// ARMADILHA 3 (manpage, w4-plano.md sec. 1.1): prepare_read() itself
// REFUSES (returns nonzero) while another thread already has a read
// prepared, or while THIS thread's own pending queue still has
// undispatched events in it - draining that queue first is exactly
// what the loop below does, and it is why this comes BEFORE poll(),
// never a bare poll() first.
gltfx_rslt<void> wayland_display_adapter::drain_pending_and_prepare_read() noexcept {
    while (wl_display_prepare_read(m_display) != 0) {
        if (wl_display_dispatch_pending(m_display) == -1) {
            m_fatal = true;
            return gltfx_rslt<void>::err(build_connection_failure(m_display));
        }
    }
    return gltfx_rslt<void>::ok();
}

// ARMADILHA 6 (w4-plano.md sec. 3.4): EAGAIN here means the KERNEL's
// own socket send buffer is full, not that flush() failed - waiting
// for POLLOUT and retrying is the only correct response; giving up
// would leave an outgoing request stuck forever the first time this
// ran under real load, while every test that never fills the buffer
// would keep passing. Called only after drain_pending_and_prepare_read()
// has already prepared a read, so every failure path here must pair
// it with wl_display_cancel_read() (ARMADILHA 2) before latching fatal
// or returning the transient "nothing done yet" outcome below.
//
// D-W6b-57, CONSERTO POS-REVISAO (docs/plano-w6b-fatias-6-8.md; revisao
// adversarial da fatia 7, 07/09/2026): this function used to carry its
// OWN fixed 100ms budget (k_flush_budget_ms, now gone) and ALWAYS
// latched m_fatal on a timeout no matter what the caller had actually
// asked for - INCLUDING pump_events()'s own budget_ms == 0 ("is the
// kernel send buffer free RIGHT NOW"). `budget_ms` and `deadline` now
// both come from dispatch_ready_events() below, computed ONCE from the
// caller's own request and shared with the step-3 read-side wait in
// wait_for_incoming_data() - the same "one clock, not two independent
// guesses" shape 72754af already applies within egl_context_adapter.
// cpp's own poll_and_dispatch_with_budget().
//
// WHETHER A TIMEOUT IS FATAL is flush_retry_policy.hpp's own decision
// (flush_write_wait_is_fatal()), never this function's own inline
// judgment call: a zero-budget caller only asked to check once, so a
// merely-full buffer (bounded_wait_outcome::timed_out) is normal,
// transient backpressure under load, forgiven here - ok(false), the
// prepared read already canceled, the unsent data staying queued
// INSIDE libwayland (wl_display_flush()'s own manpage contract for
// EAGAIN: nothing is lost) for the NEXT pump_events()/wait_events()
// call to retry. Any other outcome (a real, non-zero budget exhausted,
// or the socket itself having broken - poll_failed) IS fatal, the same
// verdict this function always reached before this conserto.
[[nodiscard]] gltfx_rslt<bool>
wayland_display_adapter::flush_with_retry(std::uint32_t budget_ms,
                                          std::chrono::steady_clock::time_point deadline) noexcept {
    while (wl_display_flush(m_display) == -1) {
        if (errno != EAGAIN) {
            wl_display_cancel_read(m_display);
            m_fatal = true;
            return gltfx_rslt<bool>::err(build_connection_failure(m_display));
        }
        const bounded_wait_outcome outcome =
            wait_for_writable_until(wl_display_get_fd(m_display), deadline);
        if (outcome != bounded_wait_outcome::ready) {
            // Budget exhausted (or the socket itself failed) and the
            // kernel send buffer is still not writable - flush_write_
            // wait_is_fatal() (flush_retry_policy.hpp) is the ONE place
            // that now decides whether that means "compositor is not
            // draining, connection dead" or "transient, try next call".
            wl_display_cancel_read(m_display);
            if (flush_write_wait_is_fatal(budget_ms, outcome)) {
                m_fatal = true;
                return gltfx_rslt<bool>::err(build_connection_failure(m_display));
            }
            return gltfx_rslt<bool>::ok(false);
        }
    }
    return gltfx_rslt<bool>::ok(true);
}

// NON-BLOCKING BY DEFAULT WHEN timeout_ms IS ZERO (w4-plano.md sec.
// 3.0/3.1.D; LOOP-RUN fatia 7, D-W6b-50, widens this to any budget):
// "is there anything to read, and if not, is it worth SLEEPING for up
// to timeout_ms rather than spinning" - pump_events() below always
// asks with 0 ("right now, never wait"); wait_events() (this fatia)
// asks with a caller-supplied budget instead. D-W6b-57's own conserto
// pos-revisao (07/09/2026): `timeout_ms` here is dispatch_ready_
// events()'s own REMAINING time after flush_with_retry() already spent
// part of the SAME shared deadline, never the caller's full budget a
// second time over - already 0 whenever the flush itself used the
// whole thing. ARMADILHA 1 (manpage): a
// BLOCKING dispatch call between prepare_read and read_events/
// cancel_read is a deadlock - poll() with a bounded timeout is
// deliberately NOT that; it is the one call this sequence is allowed
// to wait on. EINTR (D-W6b-50's own text) is treated as "woke up with
// nothing", never as a failure this connection has to latch fatal
// over - a signal arriving mid-wait is not a reason to declare the
// wl_display connection itself unusable, the same "transient, not
// fatal" distinction bounded_output_wait.hpp's own header comment
// already draws for the write-side twin of this exact poll(). Returns
// ok(false) - ARMADILHA 2's "found nothing" half of the mandatory
// pairing, wl_display_cancel_read() already called - when nothing
// arrived (by timeout OR by EINTR), so the caller can return success
// without a read_and_dispatch_incoming() call that has nothing to do.
gltfx_rslt<bool>
wayland_display_adapter::wait_for_incoming_data(std::uint32_t timeout_ms) noexcept {
    pollfd incoming{.fd = wl_display_get_fd(m_display), .events = POLLIN, .revents = 0};
    const int poll_result = poll(&incoming, 1, static_cast<int>(timeout_ms));
    if (poll_result == -1) {
        if (errno == EINTR) {
            wl_display_cancel_read(m_display);
            return gltfx_rslt<bool>::ok(false);
        }
        wl_display_cancel_read(m_display);
        m_fatal = true;
        return gltfx_rslt<bool>::err(build_connection_failure(m_display));
    }
    if (poll_result == 0 || (incoming.revents & POLLIN) == 0) {
        wl_display_cancel_read(m_display);
        return gltfx_rslt<bool>::ok(false);
    }
    return gltfx_rslt<bool>::ok(true);
}

gltfx_rslt<void> wayland_display_adapter::read_and_dispatch_incoming() noexcept {
    if (wl_display_read_events(m_display) == -1) {
        // ARMADILHA 4 (manpage: "erros sao fatais"): a failed read_
        // events() here is exactly the same class of unusable-display
        // condition roundtrip() latches above - the DIFFERENT call
        // site does not make it a different kind of failure.
        m_fatal = true;
        return gltfx_rslt<void>::err(build_connection_failure(m_display));
    }
    if (wl_display_dispatch_pending(m_display) == -1) {
        m_fatal = true;
        return gltfx_rslt<void>::err(build_connection_failure(m_display));
    }
    return gltfx_rslt<void>::ok();
}

// dispatch_ready_events() - LOOP-RUN fatia 7 (D-W6b-50; D-W6b-57's own
// conserto pos-revisao, 07/09/2026): the ONE place the four documented
// steps (this class's own header comment on pump_events()) now run in
// sequence, parametrized by `timeout_ms` - pump_events() and
// wait_events() below are both thin callers of this, never two copies
// of the same four-step sequence (the exact duplication-of-a-wait this
// project's own tests/wait_points.txt header comment names as what let
// the F1 defect's twin survive).
//
// `deadline` is computed ONCE, here, from the caller's own `timeout_ms`
// - D-W6b-57's own text: the write-side poll (inside flush_with_retry)
// and the read-side poll (wait_for_incoming_data) now share that SAME
// total budget instead of each getting its own full `timeout_ms`, so a
// flush that used up part of the budget leaves only what remains for
// the read-side wait - never `timeout_ms` twice over. flush_with_retry
// also gets the RAW `timeout_ms` (not just the deadline): it is the
// only thing that tells flush_write_wait_is_fatal() (flush_retry_
// policy.hpp) whether this was a zero-budget call (pump_events(),
// where a timeout is forgiven) or a real wait (wait_events(), where it
// is not) - the deadline alone cannot answer that once time has
// already elapsed.
gltfx_rslt<bool> wayland_display_adapter::dispatch_ready_events(std::uint32_t timeout_ms) noexcept {
    if (const gltfx_rslt<void> prepared = drain_pending_and_prepare_read(); prepared.has_error()) {
        return gltfx_rslt<bool>::err(prepared.error());
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    gltfx_rslt<bool> flushed = flush_with_retry(timeout_ms, deadline);
    if (flushed.has_error()) {
        return flushed;
    }
    if (!flushed.value()) {
        // D-W6b-57: the write side could not be flushed within its
        // share of the budget and flush_with_retry() itself already
        // decided that is not fatal (budget_ms == 0, transient kernel
        // backpressure) - the prepared read is already canceled
        // (ARMADILHA 2's pairing, done inside flush_with_retry()), so
        // this returns the SAME "nothing happened this call" outcome
        // wait_for_incoming_data()'s own false already represents,
        // never a second, redundant poll with whatever time is left.
        return gltfx_rslt<bool>::ok(false);
    }
    const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  deadline - std::chrono::steady_clock::now())
                                  .count();
    gltfx_rslt<bool> ready = wait_for_incoming_data(
        remaining_ms > 0 ? static_cast<std::uint32_t>(remaining_ms) : std::uint32_t{0});
    if (ready.has_error()) {
        return ready;
    }
    if (!ready.value()) {
        return gltfx_rslt<bool>::ok(false);
    }
    if (const gltfx_rslt<void> dispatched = read_and_dispatch_incoming(); dispatched.has_error()) {
        return gltfx_rslt<bool>::err(dispatched.error());
    }
    return gltfx_rslt<bool>::ok(true);
}

gltfx_rslt<void> wayland_display_adapter::pump_events() noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        return gltfx_rslt<void>::err(build_connection_failure(m_display));
    }

    // "is there anything to read RIGHT NOW" - dispatch_ready_events()'s
    // own timeout_ms 0 (this method's own header comment).
    if (const gltfx_rslt<bool> dispatched = dispatch_ready_events(0); dispatched.has_error()) {
        return gltfx_rslt<void>::err(dispatched.error());
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<bool> wayland_display_adapter::wait_events(std::uint32_t budget_ms) noexcept {
    if (!is_open()) {
        return gltfx_rslt<bool>::err(gltfx_err(gltfx_err_code::invalid_argument));
    }
    if (m_fatal) {
        return gltfx_rslt<bool>::err(build_connection_failure(m_display));
    }
    return dispatch_ready_events(budget_ms);
}

void wayland_display_adapter::close() noexcept {
    if (m_display != nullptr) {
        // Reverse order of creation (w4-plano.md sec. 1.2, "teardown
        // do SDL3 e ordem inversa da criacao"): the registry is an
        // object OWNED BY the connection, so it is destroyed before
        // the connection itself is disconnected. Safe to do even on a
        // fatally-errored display (m_fatal true): wl_registry has no
        // "destroy" REQUEST in the protocol at all - wl_registry_
        // destroy() only frees this process's own local proxy memory,
        // it never sends anything over the (possibly already dead)
        // wire, and wl_display_disconnect() itself is documented safe
        // to call on an errored display - it only releases local
        // resources (the socket fd among them), never sends a further
        // protocol request either.
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_globals = global_catalog{};
        m_fatal = false;
    }
}

} // namespace glintfx::platform
