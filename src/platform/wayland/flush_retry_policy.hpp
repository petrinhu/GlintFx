// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "platform/wayland/bounded_output_wait.hpp"

// platform/wayland/flush_retry_policy.hpp - LOOP-RUN fatia 7, conserto
// pos-revisao (docs/plano-w6b-fatias-6-8.md, D-W6b-57; GODS_LAWS.md
// L-17/L-20): the ONE pure atom display_adapter.cpp::flush_with_retry()
// now calls to decide what a write-side wait that did NOT resolve
// `ready` means for the connection - the same "policy pulled out of
// the syscall-touching function into its own testable atom" shape this
// project already uses for frame_callback_sequence.hpp (present-or-
// skip a frame) and gl_version_policy.hpp (accept-or-reject a driver's
// answer). Neither wl_display nor a real socket is reachable from this
// header or its .cpp - only bounded_output_wait.hpp's own outcome enum
// and a plain budget, exactly what a caller already has in hand right
// after calling wait_for_writable_until().
//
// THE DEFECT THIS ATOM EXISTS TO MAKE UNTESTABLE AGAIN (revisao
// adversarial da fatia 7, 07/09/2026): flush_with_retry() used to carry
// its OWN fixed 100ms budget, ignoring whatever the caller (pump_
// events() or wait_events()) actually asked for, and always latched
// m_fatal whenever the write-wait did not resolve `ready` - INCLUDING
// when the caller was pump_events() itself (budget_ms == 0, "is the
// kernel send buffer free RIGHT NOW"). D-W6b-57's own text: "com
// orcamento zero... EAGAIN... cancel_read e ok(false), SEM trancar
// m_fatal" - a full kernel send buffer is normal, transient
// backpressure under load (the plan's own words), never proof the
// compositor died. Nothing in this project's suite exercised flush_
// with_retry() under a zero budget with a write end that never drains,
// which is exactly why the regression survived a whole fatia: this
// atom, and flush_retry_policy_test.cpp's own red-then-green witness
// for it, close that gap directly, without needing a live wl_display
// or a container to do it.
//
// budget_ms == 0 IS the one case this atom forgives, and ONLY for
// bounded_wait_outcome::timed_out: the caller explicitly asked for "no
// wait at all", so a buffer that is merely still full right now carries
// no information about whether the compositor is alive. Any OTHER
// budget (wait_events()'s own caller-chosen wait) exhausting without
// `ready` is treated exactly like 72754af's own verdict on egl_
// context_adapter.cpp's twin write-wait (poll_and_dispatch_with_
// budget(), always a real, non-zero budget there): the caller already
// gave the compositor real time to drain and it still did not, which
// is this project's own definition of "connection no longer usable".
// bounded_wait_outcome::poll_failed is fatal under ANY budget, zero
// included: it means the SOCKET itself broke (POLLERR/POLLHUP/
// POLLNVAL, or poll() itself returning a real error, bounded_output_
// wait.hpp's own header comment) - never "the buffer happens to be
// full right now", so budget_ms == 0 buys it no forgiveness.
//
// PRECONDITION (not checked - the same "caller already knows" contract
// wait_for_writable_until() itself documents for `fd`): call this ONLY
// with an `outcome` that is NOT bounded_wait_outcome::ready - the
// caller's own retry loop already keeps flushing whenever it is.

namespace glintfx::platform {

[[nodiscard]] bool flush_write_wait_is_fatal(std::uint32_t budget_ms,
                                             bounded_wait_outcome outcome) noexcept;

} // namespace glintfx::platform
