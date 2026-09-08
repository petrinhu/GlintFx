// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/platform/gl/context.hpp>

// platform/wayland/frame_callback_sequence.hpp - W-EGL (docs/plano-
// w6b-placa-e-laco.md fatia 3, D-W6b-6, GODS_LAWS.md L-17/L-19/L-20):
// the ONE pure atom wayland_egl_context_adapter::swap_buffers() (egl_
// context_adapter.hpp, this fatia) drives to decide "does this frame
// present now, or does it degrade to gltfx_present_outcome::skipped_
// hidden" - the mechanism D-W6b-6 chose to keep the whole process from
// blocking indefinitely: intervalo zero (eglSwapInterval never called
// at 1, this fatia's own busca, docs/plano-w6b-placa-e-laco.md sec. 0)
// PLUS this project's OWN management of the wl_surface.frame callback,
// budgeted.
//
// NO EGL, NO WAYLAND TYPE REACHABLE FROM THIS HEADER (GODS_LAWS.md
// L-17, "sem SO" - the same reasoning gl_surface_size_policy.hpp/gl_
// version_policy.hpp already establish one directory over): this atom
// does not know what a wl_callback or an EGLDisplay is. It knows
// exactly one bit of state - "is a callback from the PREVIOUS frame
// still outstanding" - and how to turn that bit, plus a caller-supplied
// budget, into a decision. The adapter is the one that actually calls
// wl_surface_frame()/wl_callback_add_listener() (arming) and the one
// that actually calls poll()+wl_display_dispatch_pending() (draining) -
// this atom only decides WHETHER that real work is worth doing, and
// WHAT the outcome is once it has (or has not) happened.
//
// TWO STEPS, NEVER ONE (this is what makes the atom testable with no
// real socket at all, "com proxy nulo" - docs/plano-w6b-placa-e-laco.md
// fatia 3's own test-case list): plan_before_wait() answers "is there
// anything to wait for, and is the budget worth spending on it" BEFORE
// the adapter ever touches wl_display's own fd; decide_after_wait()
// answers "did it arrive in time" AFTER the adapter's own real
// poll()+dispatch attempt (which may have called mark_frame_done()
// below, from inside the wl_callback listener). give_up_without_
// polling is its OWN plan value, never folded into poll_then_decide
// with budget_ms=0 handed to poll() itself - this is D-W6b-6's own
// "o orcamento e lido, nao presumido": a mutant that always plans
// poll_then_decide and lets poll(fd, 0) return instantly would still
// produce the same skipped_hidden OUTCOME, so frame_callback_sequence_
// test.cpp asserts the PLAN value itself, not only the outcome that
// follows it.

namespace glintfx::platform {

enum class frame_wait_plan : std::uint8_t {
    // No wl_surface.frame callback from a previous frame is
    // outstanding (either this is the very first frame this context
    // has ever presented, or the compositor already acked the last
    // one) - the adapter may call eglSwapBuffers() right now, no
    // socket I/O needed first.
    present_immediately,
    // A callback IS outstanding, and budget_ms is worth spending one
    // real poll(fd, budget_ms) + dispatch attempt on. The adapter runs
    // that attempt, then calls decide_after_wait() below.
    poll_then_decide,
    // A callback IS outstanding, and budget_ms is 0: this call NEVER
    // attempts a poll of its own accord (a poll with timeout 0 already
    // means "check and return instantly" to the KERNEL, but that
    // shortcut belongs to poll() itself, never to this atom silently
    // assuming it is safe to skip straight to skipped_hidden).
    give_up_without_polling,
};

class frame_callback_sequence {
  public:
    frame_callback_sequence() noexcept = default;

    // Called from inside the wl_callback listener's own `done`
    // callback (egl_context_adapter.cpp, this fatia) when the
    // compositor acks the outstanding frame. IDEMPOTENT (this fatia's
    // own test case "dois done seguidos nao contam duas vezes"): a
    // second done() with nothing pending is a no-op, never underflows
    // into some notion of "negative pending".
    void mark_frame_done() noexcept;

    // Called once a frame is genuinely presented (both when no
    // callback was outstanding, and after decide_after_wait() below
    // answered `presented`): arms the bookkeeping bit for the NEXT
    // wl_surface.frame the adapter is about to request. The adapter's
    // own job is the real wl_surface_frame()+add_listener() call this
    // marks the INTENT for - this atom only tracks the bit.
    //
    // now_ns - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md, D-W6b-46):
    // the instant the caller armed it, in the SAME clock's nanosecond
    // ticks pending_older_than() below is later asked to compare
    // against. This atom still knows nothing about WHICH clock (GODS_
    // LAWS.md L-17, "sem SO" - this header's own top comment): the
    // caller (egl_context_adapter.cpp, a platform-layer file that is
    // already allowed to read std::chrono::steady_clock) is the one
    // that reads it and hands the raw tick count in.
    void arm_pending(std::int64_t now_ns) noexcept;

    [[nodiscard]] bool has_pending_callback() const noexcept;

    // See this header's own top comment for the full contract.
    [[nodiscard]] frame_wait_plan plan_before_wait(std::uint32_t budget_ms) const noexcept;

    // Called ONLY after plan_before_wait() answered poll_then_decide
    // AND the adapter has already run its own real poll()+dispatch
    // attempt (which may or may not have called mark_frame_done()
    // above). Reads the SAME m_pending bit that call may have
    // cleared - never a second, independent notion of "did it work".
    [[nodiscard]] gltfx_present_outcome decide_after_wait() const noexcept;

    // LOOP-RUN fatia 7 (D-W6b-46): the SECOND of the sonda's two
    // criteria (present_would_skip(), src/platform/port/gl_context_
    // adapter_port.hpp's own header comment) - "has the callback THIS
    // instance is still waiting on aged past budget_ms, measured
    // against now_ns". `false` whenever nothing is pending at all (an
    // outstanding callback that does not exist cannot be "older than"
    // anything - the sonda's own suspended-state criterion is what
    // answers oculto in that case, never this one). now_ns and the
    // value arm_pending() above was last called with MUST come from
    // the SAME clock - the same single-clock precondition glintfx::
    // gltfx_duration_between() already documents one layer up, core/
    // time.hpp, applied here to a caller-supplied pair instead of two
    // gltfx_time_point readings.
    [[nodiscard]] bool pending_older_than(std::int64_t now_ns,
                                          std::uint32_t budget_ms) const noexcept;

  private:
    bool m_pending = false;
    std::int64_t m_armed_at_ns = 0;
};

} // namespace glintfx::platform
