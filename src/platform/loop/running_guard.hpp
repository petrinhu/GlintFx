// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// platform/loop/running_guard.hpp - LOOP-CONTEXT-OWNERSHIP (S1b,
// /var/tmp/glintfx-plan/loop-fix.md sec. 3.2/S1b, D-LF-6d): arms a
// `bool running` flag for the lifetime of ONE outer loop_run()/
// store_loop_callbacks() call, and disarms it unconditionally on scope
// exit (RAII) - the re-entrance guard tests/loop_engine_test.cpp's own
// T16 proves. Without this, a consumer whose on_frame/on_render calls
// run()/run(callbacks)/set_callbacks() AGAIN, on the SAME loop, would
// - on form 2 (set_callbacks(), loop.hpp's own header comment on that
// method) - destroy the very callbacks context the OUTER call is still
// executing.
//
// THE CALLER, NEVER THIS CLASS, DECIDES WHETHER TO REFUSE: this atom
// only arms and disarms the flag - loop_engine.hpp's own loop_run()
// and store_loop_callbacks.hpp's own store_loop_callbacks() both check
// `book.running`/`impl.book.running` THEMSELVES, BEFORE constructing
// one of these, and return the "running" refusal without ever
// constructing a guard for the re-entrant call. That is what makes
// disarming unconditional here correct: by the time this constructor
// runs, the caller has already proven this IS the outer call.
//
// A SEPARATE ATOM FROM owned_loop_context (this file's own sibling,
// owned_loop_context.hpp): that one owns a (context, destroy) pair;
// this one owns nothing but a bool it did not allocate - one assunto,
// one file (GODS_LAWS.md L-17).
namespace glintfx::platform {

class running_guard {
  public:
    explicit running_guard(bool &running) noexcept : m_running(running) { m_running = true; }

    running_guard(const running_guard &) = delete;
    running_guard &operator=(const running_guard &) = delete;
    running_guard(running_guard &&) = delete;
    running_guard &operator=(running_guard &&) = delete;

    ~running_guard() noexcept { m_running = false; }

  private:
    bool &m_running;
};

} // namespace glintfx::platform
