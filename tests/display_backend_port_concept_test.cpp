// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "fake/fake_display_adapter.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/display_backend_port.hpp"

// display_backend_port_concept_test.cpp - W-D' (docs/plano-w6a-
// janela.md fatia 6): proves platform::display_backend_port at the
// TYPE level, the same "positive control plus a negative control"
// shape display_port_concept_test.cpp already establishes one file
// over for the narrower display_connection_port. Every check here is
// a static_assert - the "vermelho" GODS_LAWS.md L-20 requires is a
// COMPILE FAILURE, verified by deleting local_backend_with_pump's own
// pump_events() and confirming the positive-control static_assert
// below stops compiling, before this file's production counterpart
// (display_backend_port.hpp) existed to satisfy it.
//
// glintfx::test::fake_display_adapter (tests/fake/) is reused
// UNCHANGED as the NEGATIVE control here: it already satisfies
// display_connection_port (display_port_concept_test.cpp's own proof)
// but was never given a pump_events() member, which is exactly the ONE
// capability display_backend_port adds - a concept that quietly
// accepted it anyway would be the same "porta gorda"/"aceita qualquer
// coisa" defect GODS_LAWS.md L-19/L-40 name, this time for the
// REFINED concept instead of the base one. Adding pump_events() to the
// shared fixture instead would blur that distinction (every existing
// display_connection_port-only case would start satisfying the wider
// concept too, silently), so the POSITIVE control below is a small
// LOCAL type instead, the same "local, deliberately narrow" pattern
// display_port_concept_test.cpp's own missing_close already uses.

namespace {

// The deliberately CONFORMING type: display_connection_port's own
// three members plus pump_events(), the ONE thing display_backend_
// port adds.
class local_backend_with_pump {
  public:
    local_backend_with_pump() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.1): display_backend_port refines display_
    // connection_port, which now requires pinned_adapter<A> instead of
    // std::movable<A> (platform/port/adapter_pin.hpp) - this local
    // positive control has to satisfy the SAME contract every real
    // adapter now does, or this file's own static_assert below would
    // stop compiling for the wrong reason (a movable type failing
    // pinned_adapter, not a missing pump_events()).
    local_backend_with_pump(local_backend_with_pump &&) = delete;
    local_backend_with_pump &operator=(local_backend_with_pump &&) = delete;

    [[nodiscard]] glintfx::gltfx_rslt<void> open() noexcept {
        return glintfx::gltfx_rslt<void>::ok();
    }
    void close() noexcept {}
    [[nodiscard]] bool is_open() const noexcept { return false; }

    [[nodiscard]] glintfx::gltfx_rslt<void> pump_events() noexcept {
        return glintfx::gltfx_rslt<void>::ok();
    }

    // LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md, D-W6b-50): the
    // ONE member display_backend_port grew this fatia - added here so
    // this positive control keeps satisfying the (now wider) concept
    // it exists to prove.
    [[nodiscard]] glintfx::gltfx_rslt<bool> wait_events(std::uint32_t /*budget_ms*/) noexcept {
        return glintfx::gltfx_rslt<bool>::ok(false);
    }
};

} // namespace

// Positive control: a type with every member display_backend_port
// asks for satisfies it.
static_assert(glintfx::platform::display_backend_port<local_backend_with_pump>,
              "local_backend_with_pump must satisfy display_backend_port");

// Negative control: fake_display_adapter satisfies the NARROWER
// display_connection_port (proved in display_port_concept_test.cpp)
// but has no pump_events() - it must NOT satisfy the wider
// display_backend_port.
static_assert(!glintfx::platform::display_backend_port<glintfx::test::fake_display_adapter>,
              "fake_display_adapter has no pump_events() and must NOT satisfy "
              "display_backend_port - a concept that accepts it anyway is the L-40 'aceita "
              "qualquer coisa' defect");

GLINTFX_TEST(local_backend_with_pump_satisfies_the_port) {
    GLINTFX_CHECK((glintfx::platform::display_backend_port<local_backend_with_pump>));
}

GLINTFX_TEST(fake_display_adapter_without_pump_events_does_not_satisfy_the_port) {
    GLINTFX_CHECK(!(glintfx::platform::display_backend_port<glintfx::test::fake_display_adapter>));
}
