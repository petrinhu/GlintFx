// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// fake_display_adapter.hpp - ARCH-PORTS, CTO plan sec. 1.4: the
// TEST-ONLY adapter that proves platform::display_connection<A> is
// genuinely substitutable - the actual benefit GODS_LAWS.md L-19 wants
// from modeling the platform boundary as a concept instead of a
// `virtual` interface. tests/display_connection_fake_test.cpp runs the
// exact same open/use/close sequence through display_connection<fake_
// display_adapter> that a real caller runs through display_connection
// <wayland_display_adapter> - if display_connection<A>'s own behavior
// ever diverged by WHICH adapter it wraps, this is what would catch
// it.
//
// NEVER part of glintfx_library's own object set - lives under
// tests/, not src/platform/ (tests/tools/check_port_privacy.sh's own
// closed-list scan only walks src/platform/ for exactly this reason:
// a test-only type has no business appearing there at all).
//
// STATIC CALL COUNTERS, DELIBERATELY, AND THIS IS TEST-ONLY SCAFFOLDING
// (does NOT violate the "no global/static state" leg of the ARCH-PORTS
// gatilho de parada - that rule governs the PRODUCTION adapter, never
// a test double): display_connection<A>::connect() (display_
// connection.hpp) default-constructs A with NO arguments
// (std::default_initializable<A>, part of display_connection_port),
// so there is no constructor parameter through which a per-instance
// call log could be injected - and once connect() succeeds, the
// caller only ever sees the WRAPPING display_connection<A>, which
// exposes no accessor back into its adapter (display_connection<A> is
// deliberately opaque to what it wraps, matching the concept's own
// narrow surface). A static counter, reset by the test before use
// (same idiom tests/err_context_test.cpp's own g_force_alloc_failure/
// g_override_new_call_count already use for the identical reason -
// observing an effect from OUTSIDE an object whose own lifetime the
// test does not fully control), is what lets
// display_connection_fake_test.cpp prove open()/close() were each
// called exactly once across a connect-then-destroy sequence, even
// though the wrapped fake_display_adapter itself is gone by the time
// the destructor has run.
//
// INJECTABLE REFUSAL: open() consults a static "armed" flag and error
// code before touching anything else, so a test can prove "the error
// I injected reaches the caller of display_connection<A>::connect()
// UNCHANGED" (GODS_LAWS.md L-22) without needing a real operating
// system to refuse a real connection - that half is
// tests/display_connect_failure_test.cpp's own job, against the REAL
// adapter.
namespace glintfx::test {

class fake_display_adapter {
  public:
    fake_display_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.2): display_connection_port now requires pinned_
    // adapter<A> instead of std::movable<A> (platform/port/pinned_
    // adapter.hpp) - this fixture has to satisfy the SAME contract the
    // real wayland_display_adapter/win32_display_adapter now do, or
    // display_connection_port_concept_test.cpp's own positive control
    // would stop compiling. There is no `this`-derived pointer here for
    // a move to leave dangling (this fixture never registers itself
    // with anything - this file's own header comment above), but the
    // port no longer distinguishes "safe to move" adadapters from
    // "unsafe to move" ones: every adapter it selects is pinned, D-UAF-2.
    fake_display_adapter(const fake_display_adapter &) = delete;
    fake_display_adapter &operator=(const fake_display_adapter &) = delete;
    fake_display_adapter(fake_display_adapter &&) = delete;
    fake_display_adapter &operator=(fake_display_adapter &&) = delete;

    ~fake_display_adapter() = default;

    // Test setup, called BEFORE the connect() sequence under test:
    // clears every static counter and disarms any injected failure
    // left over from a PREVIOUS case in the same test binary (harness_
    // main.cpp runs every GLINTFX_TEST case in the same process,
    // sequentially - see harness/harness_main.cpp).
    static void reset() noexcept {
        s_open_call_count = 0;
        s_close_call_count = 0;
        s_inject_failure = false;
        s_injected_code = glintfx::gltfx_err_code::unknown;
    }

    // Arms EVERY subsequent open() (until reset() runs again) to
    // refuse with exactly `code` - the injection half of "o erro
    // injetado chega ao chamador intacto".
    static void arm_failure(glintfx::gltfx_err_code code) noexcept {
        s_inject_failure = true;
        s_injected_code = code;
    }

    [[nodiscard]] static int open_call_count() noexcept { return s_open_call_count; }
    [[nodiscard]] static int close_call_count() noexcept { return s_close_call_count; }

    [[nodiscard]] glintfx::gltfx_rslt<void> open() noexcept {
        ++s_open_call_count;
        // FACADE-PIN, T2 (docs/plano-conserto-fachadas-uaf.md sec. 8):
        // records THIS object's own address at the instant open()
        // itself ran - opened_at() below is what
        // display_connection_fake_test.cpp compares against `&conexao.
        // adapter()` to prove the wrapper never moved the adapter after
        // opening it (the FORM this fatia's own D-UAF-1 fixes, proved
        // generically here the same way the real adapters prove it
        // against a live wl_registry/GWLP_USERDATA in the container/
        // Windows fixtures, T0/T4).
        m_opened_at = this;
        if (s_inject_failure) {
            return glintfx::gltfx_rslt<void>::err(glintfx::gltfx_err(s_injected_code));
        }
        m_open = true;
        return glintfx::gltfx_rslt<void>::ok();
    }

    void close() noexcept {
        ++s_close_call_count;
        m_open = false;
    }

    [[nodiscard]] bool is_open() const noexcept { return m_open; }

    // FACADE-PIN, T2: the address this object had when open() ran -
    // null before open() is ever called. Nullptr and "this" can never
    // be confused with a wrong-but-nonnull answer: the whole point of
    // this seam is to compare it against `&adapter`, and a null vs a
    // real address always differs.
    [[nodiscard]] const void *opened_at() const noexcept { return m_opened_at; }

  private:
    bool m_open = false;
    const void *m_opened_at = nullptr;

    static inline int s_open_call_count = 0;
    static inline int s_close_call_count = 0;
    static inline bool s_inject_failure = false;
    static inline glintfx::gltfx_err_code s_injected_code = glintfx::gltfx_err_code::unknown;
};

} // namespace glintfx::test
