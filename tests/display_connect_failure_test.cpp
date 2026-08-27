// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/display_adapter.hpp"

// display_connect_failure_test.cpp - ARCH-PORTS, TDD case R3 (CTO plan
// sec. 2): proves wayland_display_adapter's refusal path against the
// REAL system call, on ANY Linux CI leg - no compositor required, no
// container required (that is R4/TEST-WLCONT's own job, the OPPOSITE
// half: connecting to a compositor that genuinely IS there).
//
// THE ARMADILHA THIS FILE EXISTS TO AVOID (GODS_LAWS.md L-09,
// CLAUDE.md's own "Isolamento obrigatório de teste", named again in
// the ARCH-PORTS service order): UNSETTING WAYLAND_DISPLAY does NOT
// make wl_display_connect(nullptr) fail on a machine that happens to
// have a REAL compositor running (this developer's own desktop, for
// one) - libwayland-client's own fallback resolves the LITERAL name
// "wayland-0" INSIDE XDG_RUNTIME_DIR regardless of whether
// WAYLAND_DISPLAY was ever set. The only environment variable whose
// ABSENCE-OF-A-MATCH genuinely forces a refusal is XDG_RUNTIME_DIR
// itself: pointed at a directory that is real, PRIVATE (mode 0700,
// same as tests/container/run_compositor.sh's own convention) and
// EMPTY, there is no "wayland-0" socket inside it for
// wl_display_connect() to find, on any machine, compositor running or
// not. This is a "no compositor" test in the sense that MATTERS -
// this adapter genuinely gets no socket to connect to - even when run
// on a desktop that has a real one elsewhere.
//
// GODS_LAWS.md L-09: this test touches NO real device, socket or
// process belonging to the leader's own session - it only creates and
// points at an empty scratch directory, then makes ONE library call
// that is designed, by wl_display_connect()'s own documented contract,
// to fail cleanly when nothing is listening.
//
// wayland_display_adapter is compiled a SECOND time directly into
// THIS executable's own object set (tests/CMakeLists.txt), the same
// technique tests/gfss_color_parse_test.cpp and tests/wl_proto_link_
// test.cpp already use for a symbol with no GLINTFX_API: glintfx.so is
// built with -fvisibility=hidden (cmake/GlintfxLibrary.cmake) and
// nothing marks wayland_display_adapter's methods for export
// (deliberately - GODS_LAWS.md L-19/ARCH-PORTS, "nada e exportado"),
// so a SEPARATE executable linking only against glintfx::glintfx could
// never resolve them across that .so boundary.

namespace {

// RAII scratch XDG_RUNTIME_DIR - created fresh, torn down on scope
// exit regardless of how the test case ends, so a failing case never
// leaves a stray directory under /var/tmp behind for the next run to
// trip over.
class private_empty_runtime_dir {
  public:
    private_empty_runtime_dir() {
        const std::filesystem::path base = std::filesystem::temp_directory_path();
        m_path = base / "glintfx-arch-ports-empty-runtime-XXXXXX";
        std::string template_str = m_path.string();
        // mkdtemp mutates its argument in place and returns the SAME
        // pointer on success - the standard, race-free way to create a
        // directory whose name is guaranteed not to collide with a
        // concurrent test run, unlike hand-rolling a name from a PID.
        const char *created = mkdtemp(template_str.data());
        if (created == nullptr) {
            std::println(stderr, "display_connect_failure_test: mkdtemp failed for template {}",
                         template_str);
            std::abort();
        }
        m_path = created;
        // 0700, same convention as tests/container/run_compositor.sh's
        // own create_private_runtime_dir(): private to this process,
        // never group/world-readable.
        std::filesystem::permissions(m_path, std::filesystem::perms::owner_all);
    }

    private_empty_runtime_dir(const private_empty_runtime_dir &) = delete;
    private_empty_runtime_dir &operator=(const private_empty_runtime_dir &) = delete;

    ~private_empty_runtime_dir() {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] const std::filesystem::path &path() const noexcept { return m_path; }

  private:
    std::filesystem::path m_path;
};

// setenv/unsetenv are not reentrant-safe against a CONCURRENT test
// process touching the SAME process environment, but glintfx_add_test()
// (cmake/GlintfxTest.cmake) gives every case its own executable and
// harness_main.cpp runs every GLINTFX_TEST case in this one file
// sequentially in a single thread - there is nothing else in this
// process racing this call.
class scoped_xdg_runtime_dir_override {
  public:
    explicit scoped_xdg_runtime_dir_override(const std::filesystem::path &dir) {
        const char *previous = std::getenv("XDG_RUNTIME_DIR");
        m_had_previous = previous != nullptr;
        if (m_had_previous) {
            m_previous_value = previous;
        }
        setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);
    }

    scoped_xdg_runtime_dir_override(const scoped_xdg_runtime_dir_override &) = delete;
    scoped_xdg_runtime_dir_override &operator=(const scoped_xdg_runtime_dir_override &) = delete;

    ~scoped_xdg_runtime_dir_override() {
        if (m_had_previous) {
            setenv("XDG_RUNTIME_DIR", m_previous_value.c_str(), 1);
        } else {
            unsetenv("XDG_RUNTIME_DIR");
        }
    }

  private:
    bool m_had_previous = false;
    std::string m_previous_value;
};

} // namespace

GLINTFX_TEST(open_against_an_empty_private_runtime_dir_returns_platform_failure) {
    const private_empty_runtime_dir empty_dir;
    const scoped_xdg_runtime_dir_override override_guard(empty_dir.path());

    glintfx::platform::wayland_display_adapter adapter;
    const glintfx::gltfx_rslt<void> opened = adapter.open();

    // "sem abort, sem excecao": reaching this line at all already
    // proves both halves - open() is noexcept (an escaped exception
    // would have called std::terminate() before this line), and the
    // process is still running (a real abort() would have ended it).
    GLINTFX_CHECK(opened.has_error());
    GLINTFX_CHECK(opened.error().code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(!adapter.is_open());

    // Destroying a NEVER-successfully-opened adapter must not attempt
    // to disconnect anything - close() is a no-op when m_display is
    // still null (display_adapter.cpp), so this line is here to prove
    // the destructor path is exercised at all, not to assert a new
    // fact: a crash inside ~wayland_display_adapter() here would end
    // this process before harness_main.cpp ever reported a result.
}
