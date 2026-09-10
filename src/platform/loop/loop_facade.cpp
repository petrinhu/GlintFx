// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cassert>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/gl/gl_context_impl.hpp"
#include "platform/loop/loop_context_view.hpp"
#include "platform/loop/loop_engine.hpp"
#include "platform/loop/loop_impl.hpp"
#include "platform/loop/steady_loop_clock.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// loop_facade.cpp - LOOP-RUN fatia 6b (docs/plano-w6b-fatias-6-8.md
// sec. 8.3, D-W6b-41/42/43/44/45/49/54/55, GODS_LAWS.md L-17/L-19/
// L-22), cobertura S2 (/var/tmp/glintfx-plan/loop-fix.md sec. S2.2):
// the ONE translation unit that turns the frozen surface include/
// glintfx/platform/loop/loop.hpp declares into a real handle - the
// exact same "allocation and deallocation on the SAME side of the
// boundary" shape display_facade.cpp/window_facade.cpp/gl_context_
// facade.cpp already give their own opaque handles.
//
// SINCE S2, THIS FILE IS THE ONLY TU THAT LINKS gltfx_loop's THREE
// BORROWED IMPLS TO THE MOTOR'S OWN PORTS (loop_ports.hpp): the loop's
// own BODY - what used to be inlined inside step()/present()/run()
// directly - now lives in loop_engine.hpp as loop_step()/loop_present()
// /loop_run(), a function-template over four compile-time ports,
// exercised WITHOUT any operating system by tests/loop_engine_test.cpp
// (S2-F1/S2-F11). step()/present()/run() below are three thin wrappers:
// check the precondition (unchanged), build the ports, call the motor.
// The order-of-operations comments that used to live here (D-W6b-44's
// six steps inside step(), D-W6b-54's order inside run()) moved to
// loop_engine.hpp, next to the code they describe.
//
// FACADE-PIN, ALLOCATE FIRST, VALIDATE THEN FILL IN (the same shape
// window_facade.cpp's own open() already uses, one directory over):
// open() below checks the three preconditions (P10) BEFORE ever
// allocating - display_impl/window_impl/gl_context_impl.hpp's own
// selected_*_adapter types are all pinned (platform::pinned_adapter<A>,
// FACADE-PIN), but loop_impl itself owns none of the three foreign
// adapters directly - it only ever stores POINTERS into them
// (loop_impl.hpp's own header comment) - so there is no "adapter
// registers its own address with the operating system before the move
// completes" hazard here at all, the class of bug FACADE-PIN exists to
// close one layer down. allocate_loop_impl() (loop_impl.hpp/.cpp) is
// still its own atom, extracted purely for TESTABILITY - see that
// header's own comment on the function for the full reasoning.
//
// PRESENT() IS THE ONLY WAY TO PRESENT INSIDE THIS LOOP'S CONTRACT
// (D-W6b-41): it goes through loop_engine.hpp's own loop_present(),
// which writes m_impl->book.last_present ONLY when swap_buffers()
// actually returned a value - the NEXT step()'s own loop_step() call
// reads that field, never re-derives it from anything else (P4).
// Calling gltfx_gl_context::swap_buffers() directly instead (still
// public, P10) never reaches this loop's own bookkeeping at all - the
// loop simply never finds out, exactly as the header promises.

namespace glintfx {

namespace {

// The ONE place gltfx_loop's three borrowed impls are wired to the
// motor's own ports (platform::loop_ports, loop_ports.hpp) - built
// fresh on the STACK by every step()/present()/run() call. `context`
// and `clock` are owned here (both zero-state wrappers, nothing to
// manage beyond the call itself); `display`/`window` are referenced
// straight out of `impl` for the duration of the call, the same
// pointers open() already borrowed.
struct bound_ports {
    explicit bound_ports(loop_impl &impl) noexcept : context{*impl.context} {}

    platform::loop_context_view context;
    platform::steady_loop_clock clock;
};

using loop_ports_t = platform::loop_ports<platform::selected_display_adapter,
                                          platform::window_state, platform::loop_context_view,
                                          platform::steady_loop_clock>;

[[nodiscard]] loop_ports_t make_loop_ports(loop_impl &impl, bound_ports &bound) noexcept {
    return loop_ports_t{
        .display = impl.display->connection.adapter(),
        .window = impl.window->adapter.state(),
        .context = bound.context,
        .clock = bound.clock,
    };
}

} // namespace

gltfx_rslt<gltfx_loop> gltfx_loop::open(gltfx_display &display, gltfx_window &window,
                                        gltfx_gl_context &context) noexcept {
    // P10: refuses BY NAME whichever of the three is not open - the
    // same precondition shape window_facade.cpp's own open() already
    // uses for its own single `display` parameter, one layer up.
    if (!display.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("display"));
    }
    if (!window.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }
    if (!context.is_open()) {
        return gltfx_rslt<gltfx_loop>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("context"));
    }

    gltfx_rslt<loop_impl *> allocated = allocate_loop_impl();
    if (allocated.has_error()) {
        return gltfx_rslt<gltfx_loop>::err(allocated.err());
    }

    // Borrowed pointers only (D-W6b-55/loop_impl.hpp's own header
    // comment) - obtained through the SAME three internal-access
    // passkeys display_facade.cpp/window_facade.cpp/gl_context_facade.
    // cpp already establish for every other cross-facade caller.
    loop_impl *impl = allocated.value();
    impl->display = display_internal_access::get(display);
    impl->window = window_internal_access::get(window);
    impl->context = gl_context_internal_access::get(context);
    // Runs ONCE, outside any loop_step()/loop_present()/loop_run()
    // call, so it has no ports object to go through yet - still reads
    // gltfx_now() directly (steady_loop_clock.hpp's own header comment
    // names this as the one other site under src/platform/loop/ that
    // does; tests/wait_points.txt's own row for this line stays).
    impl->book.previous_now = gltfx_now();
    impl->book.last_present = gltfx_present_outcome::presented;
    impl->book.frame_index = 0;

    return gltfx_rslt<gltfx_loop>::ok(gltfx_loop(impl));
}

gltfx_loop::gltfx_loop(gltfx_loop &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_loop &gltfx_loop::operator=(gltfx_loop &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

// Never closes display/window/context (P10) - only this loop's own
// scheduling state (loop_impl.hpp's own header comment) is this
// handle's to destroy.
gltfx_loop::~gltfx_loop() { delete m_impl; }

bool gltfx_loop::is_open() const noexcept { return m_impl != nullptr; }

gltfx_rslt<gltfx_frame_tick> gltfx_loop::step() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::step() called on a moved-from loop - the object no longer owns an impl");
    bound_ports bound{*m_impl};
    return platform::loop_step(make_loop_ports(*m_impl, bound), m_impl->book);
}

gltfx_rslt<gltfx_present_outcome> gltfx_loop::present() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::present() called on a moved-from loop - the object no longer owns an "
           "impl");
    bound_ports bound{*m_impl};
    return platform::loop_present(make_loop_ports(*m_impl, bound), m_impl->book);
}

gltfx_rslt<void> gltfx_loop::run(gltfx_loop_callbacks callbacks) noexcept {
    assert(m_impl != nullptr &&
           "gltfx_loop::run() called on a moved-from loop - the object no longer owns an impl");
    bound_ports bound{*m_impl};
    return platform::loop_run(make_loop_ports(*m_impl, bound), m_impl->book, callbacks);
}

// loop_internal_access::get() - the ONLY definition of this symbol in
// the whole library, the exact same reasoning display_internal_access::
// get()/window_internal_access::get()/gl_context_internal_access::
// get() already document for themselves. No GLINTFX_API on this line,
// for the identical reason.
loop_impl *loop_internal_access::get(gltfx_loop &loop) noexcept { return loop.m_impl; }

} // namespace glintfx
