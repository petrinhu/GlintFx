// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "fake/fake_display_adapter.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/display_connection_port.hpp"

// display_port_concept_test.cpp - ARCH-PORTS, TDD case R1 (CTO plan
// sec. 2): proves platform::display_connection_port at the TYPE level,
// with a NEGATIVE control as well as a positive one (GODS_LAWS.md
// L-40: "concept que aceita qualquer coisa e o gemeo em compile-time
// do portao que conta zero e passa"). Every check here is a
// static_assert, so the "vermelho" this file's own header comment for
// TDD case R1 requires is a COMPILE FAILURE, not a runtime one:
// deleting fake_display_adapter's open()/close()/is_open(), or
// deleting missing_close's own close(), was verified to make one of
// the static_asserts below fail to compile, before this file's
// production counterparts (display_connection_port.hpp/fake_display_
// adapter.hpp) existed to satisfy them.
//
// This TU no longer also proves the REAL production adapter
// (wayland_display_adapter) against the same concept - that assertion
// lived here through 02/09/2026, duplicating src/platform/wayland/
// selected_display_adapter_check.cpp's own static_assert over
// selected_display_adapter (a plain `using` alias for wayland_display_
// adapter, not a distinct type), which the library build already
// discharges unconditionally on every configure that enters src/
// platform/wayland/ at all (tests/CMakeLists.txt's own comment on
// this ctest case's registration explains why that made the wayland-
// specific half here redundant, not a second, independent proof).
// Dropping it is also what lets this file, and the ctest case it
// registers, stay OS-agnostic: nothing left in this TU names a
// Wayland type. tests/tools/check_port_privacy.sh's own header
// comment still calls fake_display_adapter (tests/fake/fake_display_
// adapter.hpp) the one test double proved against this concept here.

namespace {

// The deliberately DEFICIENT type (CTO plan sec. 1.4, "prova negativa
// no nível de tipo"): everything display_connection_port asks for
// EXCEPT a close() member. A concept that quietly accepted this type
// anyway would be exactly the "porta gorda"/"aceita qualquer coisa"
// defect GODS_LAWS.md L-19/L-40 name.
class missing_close {
  public:
    missing_close() noexcept = default;
    missing_close(missing_close &&) noexcept = default;
    missing_close &operator=(missing_close &&) noexcept = default;

    [[nodiscard]] glintfx::gltfx_rslt<void> open() noexcept {
        return glintfx::gltfx_rslt<void>::ok();
    }
    [[nodiscard]] bool is_open() const noexcept { return false; }
    // No close() member at all.
};

} // namespace

// Positive control: the test-only fake satisfies the port - the SAME
// concept, unchanged, is what lets display_connection<A> treat a fake
// and a real adapter interchangeably (display_connection_fake_test.cpp,
// TDD case R2; the real adapter's own positive control lives in
// src/platform/wayland/selected_display_adapter_check.cpp, per this
// file's own header comment above).
static_assert(glintfx::platform::display_connection_port<glintfx::test::fake_display_adapter>,
              "fake_display_adapter must satisfy display_connection_port");

// Negative control: a type missing close() must NOT satisfy the port.
static_assert(!glintfx::platform::display_connection_port<missing_close>,
              "missing_close has no close() and must NOT satisfy display_connection_port - "
              "a concept that accepts it anyway is the L-40 'aceita qualquer coisa' defect");

// GLINTFX_TEST cases exist so this file is registered as a genuine
// ctest case with observable PASS/FAIL output (the static_asserts
// above already ran, at compile time, by the point this executable
// even exists to run) - GLINTFX_CHECK below re-states each result at
// runtime so `ctest -R display_port_concept_test --output-on-failure`
// prints something a reviewer reads without having to know this file
// also carries compile-time assertions.
GLINTFX_TEST(fake_display_adapter_satisfies_the_port) {
    GLINTFX_CHECK(
        (glintfx::platform::display_connection_port<glintfx::test::fake_display_adapter>));
}

GLINTFX_TEST(a_type_missing_close_does_not_satisfy_the_port) {
    GLINTFX_CHECK(!(glintfx::platform::display_connection_port<missing_close>));
}
