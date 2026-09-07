// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#include <EGL/egl.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/gl/gl_memory_facts.hpp"
#include "platform/wayland/egl_device_query.hpp"

// egl_device_reader_probe.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5b-
// revisao.md sec. 4.6): "the leitor ran against the real system and
// this is what it saw" - the Linux half of the pair dxcore_reader_
// probe_test.cpp (tests/, Windows) is the Windows half of (paired in
// tests/parity_aliases.txt: dxcore_reader_probe_test|egl_device_
// reader_probe - the SAME coverage, different mechanism, under two
// names). gl_context_parity_test.cpp is PUBLIC-API-ONLY and cannot see
// `egl_device_queried`/`egl_device_software`/`egl_device_render_node_
// present`/`nvx_present` raw - this probe is the one place that reads
// query_egl_device_query()/read_gl_memory_facts() directly.
//
// OPENS THROUGH THE PUBLIC API (gltfx_display/gltfx_window/gltfx_gl_
// context), the SAME shape gl_context_parity_test.cpp/facade_pin_
// smoke.cpp already use one directory over - NO bespoke EGL bootstrap
// (egl_probe_smoke.cpp's own 500+ lines of wl_egl_window/eglChoose
// Config/eglCreateContext dance are a FATIA-1 question, "does the
// mechanism exist at all", already answered; this probe only needs a
// CURRENT context, and the public API already gives one). Once
// make_current() succeeds, eglGetCurrentDisplay() (core EGL 1.2+, no
// extension, no internal header needed) hands back the RAW EGLDisplay
// the current context is bound to - exactly what query_egl_display_
// device() (platform/wayland/egl_device_query.hpp, an internal, non-
// public header this test file is allowed to reach the same way
// egl_protocol_error_smoke.cpp reaches internal atoms) needs.
//
// ONLY `egl_device_queried == 1` IS ASSERTED (GODS_LAWS.md L-40's own
// non-empty floor) - every other value here is printed, never
// compared, the same "measure before asserting" shape gl_context_
// parity_test.cpp's own gpu_kind_raw already uses (sec. 4.6's own
// correction).

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "egl_device_reader_probe: gltfx_display::open() failed\n");
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "sonda de leitor de dispositivo EGL",
        .application_id = "org.glintfx.egl_device_reader_probe",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "egl_device_reader_probe: gltfx_window::open() failed\n");
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    const glintfx::gltfx_gl_context_desc empty_desc{};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
        glintfx::gltfx_gl_context::open(window, empty_desc);
    if (context_opened.has_error()) {
        std::fprintf(stderr, "egl_device_reader_probe: gltfx_gl_context::open() failed\n");
        return EXIT_FAILURE;
    }
    glintfx::gltfx_gl_context context = std::move(context_opened.value());

    if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "egl_device_reader_probe: make_current() failed\n");
        return EXIT_FAILURE;
    }

    // eglGetCurrentDisplay() - core EGL, no extension: the raw
    // EGLDisplay the context just made current is bound to.
    EGLDisplay egl_display = eglGetCurrentDisplay();
    const egl_device_facts device = query_egl_display_device(egl_display);

    std::fprintf(stdout, "MEASURED egl_device_reader_probe.egl_device_queried=%d\n",
                 device.queried ? 1 : 0);
    if (!device.queried) {
        std::fprintf(stderr,
                     "egl_device_reader_probe: query_egl_display_device() nao conseguiu resolver "
                     "eglQueryDisplayAttribEXT/eglQueryDeviceStringEXT - egl_device_queried=0, "
                     "esperado 1 (GODS_LAWS.md L-40)\n");
        return EXIT_FAILURE;
    }

    std::fprintf(stdout, "MEASURED egl_device_reader_probe.egl_device_software=%d\n",
                 device.software ? 1 : 0);
    std::fprintf(stdout, "MEASURED egl_device_reader_probe.egl_device_render_node_present=%d\n",
                 !device.render_node.empty() ? 1 : 0);

    // GL_NVX_gpu_memory_info, resolved the SAME way gl_context_parity_
    // test.cpp already resolves anything else - proc_address() is the
    // ONE mechanism a real consumer (and this probe) ever reaches GL
    // functions through.
    void *get_integerv_addr = context.proc_address("glGetIntegerv");
    void *get_error_addr = context.proc_address("glGetError");
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on.
    const auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(get_integerv_addr);
    const auto get_error = reinterpret_cast<gl_get_error_fn>(get_error_addr);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above
    const gl_memory_facts memory_facts = read_gl_memory_facts(get_integerv, get_error);
    std::fprintf(stdout, "MEASURED egl_device_reader_probe.nvx_present=%d\n",
                 memory_facts.nvx_present ? 1 : 0);

    return EXIT_SUCCESS;
}
