// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#include "platform/wayland/window_adapter.hpp"

// window_smoke.cpp - WL-WINDOW fatia W-E (docs/plano-w6a-janela.md
// fatia 8): the container-integration case for wayland_window_adapter -
// opens a REAL wl_surface/xdg_surface/xdg_toplevel against the same
// kwin_wayland --virtual compositor registry_smoke/shell_smoke already
// prove, one directory over, and - the whole reason this fixture exists
// rather than being a ninth callback-only unit test - attaches a REAL
// wl_shm buffer and commits it (D-W5-9: "buffer wl_shm do window_smoke
// fica so no fixture", never wayland_window_adapter's own job).
//
// THE PERIGO THIS FIXTURE IS BUILT TO CATCH (docs/plano-w6a-janela.md,
// this fatia's own briefing, sec. 5 risk 3 of the plan): the ack_
// configure() call inside wayland_window_adapter::wait_first_configure()
// is INVISIBLE to a fixture that never attaches content - a compositor
// with nothing committed to the surface never has anything to complain
// about, so a fixture that stops at "wait_first_configure() returned
// ok()" would still be green with the ack silently removed. Attaching
// and committing a REAL buffer is what turns a missing ack into a real
// protocol violation the compositor is entitled to kill the connection
// over (xdg-shell.xml, xdg_surface's own `error` enum,
// "unconfigured_buffer" = 3: "the surface has not been configured" -
// commit with a buffer attached before ack_configure()) - and display_
// adapter.cpp's own protocol_error() (this file's own connection.
// roundtrip() call below) attaches the interface name that raised it as
// rejected_value(), which is EXACTLY "xdg_surface" for this error. A
// REMOVED ack_configure() call must make THIS fixture's final roundtrip()
// fail with rejected_value() == "xdg_surface" - the adversarial
// reviewer's own mutation case for this fatia.
//
// wayland_display_adapter, wayland_shell_adapter, wayland_window_
// adapter, window_configure_sequence, window_state, window_desc_
// validation, utf8_validation, global_catalog, shell_requirements,
// err.cpp and err_code.cpp are compiled straight into this ONE
// executable (tests/container/prepare_arch_ports_fixture.sh stages the
// real sources) - no glintfx.so, no CMake, no .pc file, same shape
// connect_smoke.cpp/registry_smoke.cpp/shell_smoke.cpp already have one
// directory over.
//
// wl_shm ITSELF is bound directly through wayland_display_adapter::
// bind() below, never through wayland_shell_adapter (which only ever
// gathers wl_compositor/xdg_wm_base, shell_adapter.hpp's own header
// comment) - this fixture is the ONE place in this fatia that needs
// shared memory at all, and D-W5-9 keeps it that way on purpose.

namespace {

constexpr std::int32_t kWidth = 200;
constexpr std::int32_t kHeight = 100;
constexpr std::int32_t kBytesPerPixel = 4; // xrgb8888
constexpr std::int32_t kStride = kWidth * kBytesPerPixel;
constexpr std::int32_t kPoolSize = kStride * kHeight;

// Anonymous, memory-backed file descriptor - the same "no real file on
// disk" property shm_open()+O_TMPFILE tricks exist to give, but
// memfd_create() (Linux-only, glibc >= 2.27, this container's own
// Fedora base) needs neither a path nor an unlink() dance. Zero-filled
// with an explicit memset() rather than left as whatever memfd_create()
// happens to hand back: the compositor is entitled to actually render
// these bytes, and an uninitialized wl_shm pool is not a fixture this
// project wants to hand a real compositor, mutation test or not.
[[nodiscard]] int make_zeroed_shm_fd(std::int32_t size) {
    int fd = memfd_create("glintfx-window-smoke", 0);
    if (fd < 0) {
        return -1;
    }
    if (ftruncate(fd, size) != 0) {
        close(fd);
        return -1;
    }
    void *mapped =
        mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        return -1;
    }
    std::memset(mapped, 0, static_cast<std::size_t>(size));
    munmap(mapped, static_cast<std::size_t>(size));
    return fd;
}

} // namespace

int main() {
    glintfx::platform::wayland_display_adapter adapter;
    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "window_smoke: display open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.error().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        std::fprintf(stderr, "window_smoke: shell.open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(shell_opened.error().code())).c_str(),
                     std::string(shell_opened.error().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }

    const glintfx::platform::wayland_global *shm_global =
        adapter.globals().find_by_interface("wl_shm");
    if (shm_global == nullptr) {
        std::fprintf(stderr, "window_smoke: wl_shm absent from the registry\n");
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    glintfx::gltfx_rslt<void *> shm_proxy = adapter.bind(*shm_global, wl_shm_interface, 1);
    if (shm_proxy.has_error()) {
        std::fprintf(stderr, "window_smoke: bind(wl_shm) failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(shm_proxy.error().code())).c_str());
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    auto *shm = static_cast<wl_shm *>(shm_proxy.value());

    glintfx::platform::wayland_window_adapter window;
    glintfx::platform::wayland_window_desc desc{
        .logical_width = static_cast<std::uint32_t>(kWidth),
        .logical_height = static_cast<std::uint32_t>(kHeight),
        .title = "window_smoke",
        .application_id = "org.glintfx.window_smoke",
    };
    glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
    if (window_opened.has_error()) {
        std::fprintf(
            stderr, "window_smoke: window.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.error().code())).c_str(),
            std::string(window_opened.error().rejected_value()).c_str());
        wl_shm_destroy(shm);
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "window_smoke: window configured - logical_size=%ux%u pixel_size=%ux%u\n",
                 window.state().logical_size().width, window.state().logical_size().height,
                 window.state().pixel_size().width, window.state().pixel_size().height);
    // MEASURED-COLLECTOR: this fixture talks to the INTERNAL Wayland
    // adapter directly (platform::wayland_window_adapter), not the
    // public gltfx_window window_parity_test.cpp already collects the
    // same four facts for - kept as its own keys, never merged, since
    // the two never claim to measure the same call path.
    std::fprintf(stdout, "MEASURED window_smoke.logical_width=%u\n",
                 window.state().logical_size().width);
    std::fprintf(stdout, "MEASURED window_smoke.logical_height=%u\n",
                 window.state().logical_size().height);
    std::fprintf(stdout, "MEASURED window_smoke.pixel_width=%u\n",
                 window.state().pixel_size().width);
    std::fprintf(stdout, "MEASURED window_smoke.pixel_height=%u\n",
                 window.state().pixel_size().height);

    // D-W5-9: the ONE wl_shm buffer this whole fatia ever attaches -
    // never wayland_window_adapter's own job (see this file's own
    // header comment).
    int shm_fd = make_zeroed_shm_fd(kPoolSize);
    if (shm_fd < 0) {
        std::fprintf(stderr, "window_smoke: could not create the shm-backed fd\n");
        window.close();
        wl_shm_destroy(shm);
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    wl_shm_pool *pool = wl_shm_create_pool(shm, shm_fd, kPoolSize);
    wl_buffer *buffer =
        wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(shm_fd);

    wl_surface_attach(window.surface(), buffer, 0, 0);
    wl_surface_damage_buffer(window.surface(), 0, 0, kWidth, kHeight);
    wl_surface_commit(window.surface());

    // THE MANIFESTATION (this file's own header comment): a REMOVED
    // ack_configure() inside wayland_window_adapter::wait_first_
    // configure() surfaces HERE, as a protocol error on this
    // roundtrip() - never earlier, because nothing was ever attached
    // for the compositor to reject until the two lines right above ran.
    glintfx::gltfx_rslt<void> roundtripped = adapter.roundtrip();
    if (roundtripped.has_error()) {
        std::fprintf(
            stderr,
            "window_smoke: roundtrip() after buffer commit failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(roundtripped.error().code())).c_str(),
            std::string(roundtripped.error().rejected_value()).c_str());
        wl_buffer_destroy(buffer);
        window.close();
        wl_shm_destroy(shm);
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    std::fprintf(stdout,
                 "window_smoke: buffer committed and acknowledged without a protocol error\n");

    wl_buffer_destroy(buffer);

    window.close();
    if (window.is_open()) {
        std::fprintf(stderr, "window_smoke: window.close() ran but is_open() is still true\n");
        wl_shm_destroy(shm);
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    wl_shm_destroy(shm);
    shell.close();
    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "window_smoke: adapter.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "window_smoke: closed cleanly\n");

    return EXIT_SUCCESS;
}
