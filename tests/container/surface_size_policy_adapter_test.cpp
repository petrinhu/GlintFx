// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>

// WL_EGL_PLATFORM before <EGL/egl.h> (egl_probe_smoke.cpp's own header
// comment, one directory over): without it, <EGL/eglplatform.h>'s own
// #elif chain types EGLNativeWindowType as a bare integer instead of
// `struct wl_egl_window *`.
#define WL_EGL_PLATFORM
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "checked_stdio.hpp"
#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/egl_context_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#include "platform/wayland/window_adapter.hpp"
#include "platform/window/window_state.hpp"

// surface_size_policy_adapter_test.cpp - SURFACE-SIZE-POLICY-ADAPTER-
// GAP (TODO.md, W6b, GODS_LAWS.md L-09/L-17/L-20): closes the gap the
// item itself names verbatim - `gl_surface_size_policy_test` (tests/,
// plain ctest) compiles and exercises ONLY the pure atom (src/platform/
// gl/gl_surface_size_policy.{hpp,cpp}), against SYNTHETIC values,
// never the real caller (egl_context_adapter.cpp's own private
// resize_surface_if_due(), called from swap_buffers() on every frame).
// Measured by mutation on 06/09/2026 (eighth appearance in this onda of
// "afirma que mede e nao mede", feedback_afirma_que_mede_e_nao_mede.md):
// a deliberate change to the pixel size at the REAL call site passed
// with no test reproving. Before this fixture existed, that was
// trivially true for a SECOND reason too, not just "the wrong file is
// compiled": nothing in this project's whole suite ever moved a REAL
// window's pixel_size() after a context was already open, so resize_
// surface_if_due() never took its `should_resize` branch at all -
// gl_context_parity_test's own three open()/reopen() cases all keep
// the SAME logical_size() throughout.
//
// ADAPTADORES INTERNOS, NUNCA A FACHADA PUBLICA (egl_probe_smoke.cpp/
// egl_protocol_error_smoke.cpp's own convention, one directory over):
// this fixture needs window.toplevel() (the raw xdg_toplevel*) to
// trigger a REAL, protocol-driven resize - xdg_toplevel_set_fullscreen()
// - which the public gltfx_window surface deliberately does not expose
// (include/glintfx/platform/window/window.hpp's own "WHAT THIS FATIA
// FREEZES" list: v1 has no resize/fullscreen request at all). This is
// the SAME reasoning egl_protocol_error_smoke.cpp already gives for
// reaching into wayland_window_adapter/wayland_egl_context_adapter
// directly instead of gltfx_display/gltfx_window/gltfx_gl_context.
//
// WHY FULLSCREEN, AND NOT A SMALLER LOGICAL_SIZE AT OPEN() TIME: a
// Wayland client never resizes ITSELF - xdg-shell.xml's own configure
// sequence is compositor-initiated (a client picks the FINAL size only
// in response to a `configure` it received, xdg_surface_configure() in
// window_adapter.cpp already documents this). xdg_toplevel_set_
// fullscreen() is a real client REQUEST that a real compositor answers
// with a SECOND, later xdg_toplevel.configure carrying the OUTPUT's own
// geometry - a genuine, protocol-driven resize this project's own
// kwin_wayland --virtual compositor (CI's SAME image) actually sends,
// never a fabricated or injected size.
//
// THE ONE ASSERTION THAT ONLY THE REAL CALL SITE CAN SATISFY: after the
// resize lands and swap_buffers() runs again, egl_surface_pixel_size()
// (wayland_egl_context_adapter's own TEST-ONLY accessor, this fatia)
// reads the driver's OWN idea of the EGLSurface geometry via
// eglQuerySurface(EGL_WIDTH/EGL_HEIGHT) - independent of this adapter's
// own m_buffer_width/m_buffer_height bookkeeping (which resize_surface_
// if_due() itself writes, and so cannot prove anything about whether
// the real wl_egl_window_resize() call inside it still runs). A mutant
// that guts resize_surface_if_due() (this fatia's own required
// mutation, on a COPY outside the tree, GODS_LAWS.md L-27) leaves the
// EGL surface at the OLD size forever - this fixture's own final
// comparison is exactly where that surfaces.
namespace {

// Deliberately NOT a common screen/virtual-output resolution (kWidth x
// kHeight below): the fullscreen configure this fixture waits for is
// only a REAL resize if the compositor's own output geometry differs
// from the window's initial size - an accidental match would make this
// fixture pass for the wrong reason (no resize ever happened) without
// any mutation ever being exercised.
constexpr std::int32_t kWidth = 401;
constexpr std::int32_t kHeight = 277;

using glintfx::platform::window_size;
using glintfx::platform::window_state_bit;

[[nodiscard]] bool reach_presented(glintfx::platform::wayland_egl_context_adapter &context,
                                   const char *label) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
        if (swapped.has_error()) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "surface_size_policy_adapter_test: swap_buffers() (%s) attempt %d failed: %s\n",
                label, attempt,
                std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str());
            return false;
        }
        if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
            return true;
        }
    }
    glintfx::container_fixture::checked_fprintf(
        stderr,
        "surface_size_policy_adapter_test: never reached `presented` (%s) within 10 attempts\n",
        label);
    return false;
}

} // namespace

int main() {
    glintfx::container_fixture::checked_setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;
    if (const glintfx::gltfx_rslt<void> opened = adapter.open(); opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "surface_size_policy_adapter_test: display open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    if (const glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
        shell_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: shell.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(shell_opened.err().code())).c_str(),
            std::string(shell_opened.err().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_window_adapter window;
    const glintfx::platform::wayland_window_desc desc{
        .logical_width = static_cast<std::uint32_t>(kWidth),
        .logical_height = static_cast<std::uint32_t>(kHeight),
        .title = "surface_size_policy_adapter_test",
        .application_id = "org.glintfx.surface_size_policy_adapter_test",
    };
    if (const glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
        window_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: window.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
            std::string(window_opened.err().rejected_value()).c_str());
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_egl_context_adapter context;
    if (const glintfx::gltfx_rslt<void> context_opened =
            context.open(window, std::span<const glintfx::gltfx_gfx_option_entry>{});
        context_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: context.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(context_opened.err().code())).c_str(),
            std::string(context_opened.err().rejected_value()).c_str());
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // First presented frame, BEFORE any resize - the baseline this
    // fixture's own final comparison needs a real "it worked once
    // already" starting point.
    if (!reach_presented(context, "before resize")) {
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    const window_size initial_pixel_size = window.state().pixel_size();
    const auto [initial_surface_width, initial_surface_height] = context.egl_surface_pixel_size();
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.initial_pixel_width=%u\n",
        initial_pixel_size.width);
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.initial_pixel_height=%u\n",
        initial_pixel_size.height);
    if (initial_surface_width != initial_pixel_size.width ||
        initial_surface_height != initial_pixel_size.height) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: baseline EGL surface size %ux%u != window "
            "pixel_size %ux%u BEFORE any resize - open()'s own initial sizing is already wrong, "
            "unrelated to the resize path this fixture exists to prove\n",
            initial_surface_width, initial_surface_height, initial_pixel_size.width,
            initial_pixel_size.height);
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // THE REAL, PROTOCOL-DRIVEN RESIZE (this file's own header comment):
    // a genuine client REQUEST, answered by a SECOND xdg_toplevel.
    // configure carrying the compositor's own output geometry.
    xdg_toplevel_set_fullscreen(window.toplevel(), nullptr);
    wl_surface_commit(window.surface());

    bool resized = false;
    constexpr int kMaxRoundtrips = 50; // same budget wait_first_configure() uses internally
    for (int i = 0; i < kMaxRoundtrips; ++i) {
        if (const glintfx::gltfx_rslt<void> roundtripped = adapter.roundtrip();
            roundtripped.has_error()) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "surface_size_policy_adapter_test: roundtrip() while waiting for the "
                "fullscreen configure failed: %s\n",
                std::string(glintfx::gltfx_err_code_name(roundtripped.err().code())).c_str());
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        const window_size current = window.state().pixel_size();
        if (current.width != initial_pixel_size.width ||
            current.height != initial_pixel_size.height) {
            resized = true;
            break;
        }
    }
    if (!resized) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: pixel_size() never changed after "
            "xdg_toplevel_set_fullscreen() within %d roundtrips (fullscreen_state=%d) - this "
            "fixture cannot prove anything without a real resize (GODS_LAWS.md L-42)\n",
            kMaxRoundtrips, window.state().state(window_state_bit::fullscreen) ? 1 : 0);
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    window.ack_pending_configure();

    const window_size resized_pixel_size = window.state().pixel_size();
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.resized_pixel_width=%u\n",
        resized_pixel_size.width);
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.resized_pixel_height=%u\n",
        resized_pixel_size.height);

    // swap_buffers() -> resize_surface_if_due() (private, egl_context_
    // adapter.cpp) is the ONE place this new pixel_size() is supposed to
    // reach wl_egl_window_resize() - the exact call SURFACE-SIZE-
    // POLICY-ADAPTER-GAP says no test alcancava.
    if (!reach_presented(context, "after resize")) {
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    const auto [final_surface_width, final_surface_height] = context.egl_surface_pixel_size();
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.final_surface_width=%u\n",
        final_surface_width);
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED surface_size_policy_adapter_test.final_surface_height=%u\n",
        final_surface_height);

    context.close();
    window.close();
    shell.close();
    adapter.close();

    // THE ASSERTION THIS ENTIRE FIXTURE EXISTS FOR: the REAL EGL surface
    // (read from the driver, never from this adapter's own bookkeeping)
    // matches the window's NEW pixel_size() - proof that resize_
    // surface_if_due()'s real wl_egl_window_resize() call actually ran.
    if (final_surface_width != resized_pixel_size.width ||
        final_surface_height != resized_pixel_size.height) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "surface_size_policy_adapter_test: EGL surface stayed %ux%u after the resize, "
            "expected %ux%u (the window's new pixel_size()) - resize_surface_if_due() did not "
            "reach the real wl_egl_window_resize() call\n",
            final_surface_width, final_surface_height, resized_pixel_size.width,
            resized_pixel_size.height);
        return EXIT_FAILURE;
    }

    glintfx::container_fixture::checked_fprintf(
        stdout,
        "surface_size_policy_adapter_test: EGL surface followed the real resize "
        "(%ux%u -> %ux%u), como esperado\n",
        initial_pixel_size.width, initial_pixel_size.height, final_surface_width,
        final_surface_height);
    return EXIT_SUCCESS;
}
