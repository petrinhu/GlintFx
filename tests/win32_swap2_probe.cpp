// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <print>
#include <string>
#include <vector>

#include <glintfx/core/err_code.hpp>

#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/wgl_context_adapter.hpp"
#include "platform/win32/window_adapter.hpp"

// SONDA DESCARTAVEL (ramo sonda-swap2, nunca entra na onda): mede o segundo swap_buffers em quatro
// variantes. Mede, nunca reprova: cada variante imprime uma linha MEASURED.
namespace {

std::string describe(const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &r,
                     std::int64_t &os_error) {
    if (r.has_error()) {
        os_error = r.err().os_error_code();
        return std::string("erro:") + std::string(glintfx::gltfx_err_code_name(r.err().code())) +
               ":" + std::string(r.err().rejected_value());
    }
    return r.value() == glintfx::gltfx_present_outcome::presented ? "ok" : "skipped_hidden";
}

void variant(int n) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 320;
    desc.logical_height = 240;
    desc.title = "glintfx swap2 probe";
    if (display.open().has_error() || window.open(display, desc).has_error()) {
        std::println("MEASURED variante={} swap1=setup_falhou swap2=nao_rodou os_error=0 "
                     "visivel_antes_swap2=0",
                     n);
        return;
    }
    const HWND hwnd = window.native_handle();
    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    if (context.open(window, options).has_error() || context.make_current().has_error()) {
        std::println("MEASURED variante={} swap1=contexto_falhou swap2=nao_rodou os_error=0 "
                     "visivel_antes_swap2=0",
                     n);
        return;
    }
    if (n == 4) {
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE); // mostrada por fora, show_window_now nao age
        (void)display.pump_events();
    }
    std::int64_t os1 = 0;
    std::int64_t os2 = 0;
    const std::string swap1 = describe(context.swap_buffers(), os1);
    if (n == 2) {
        (void)display.pump_events();
    }
    if (n == 3) {
        ::ShowWindow(hwnd, SW_HIDE);
        (void)display.pump_events();
    }
    const int visible = ::IsWindowVisible(hwnd) != 0 ? 1 : 0;
    const std::string swap2 = describe(context.swap_buffers(), os2);
    std::println("MEASURED variante={} swap1={} swap2={} os_error={} visivel_antes_swap2={}", n,
                 swap1, swap2, os2, visible);
}

} // namespace

GLINTFX_TEST(win32_swap2_probe) {
    for (int n = 1; n <= 4; ++n) {
        variant(n);
    }
}

#endif
