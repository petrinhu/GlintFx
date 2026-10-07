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

// SONDA DESCARTAVEL (ramo sonda-swap2, nunca entra na onda): mede o segundo swap_buffers.
// Variantes 1 a 4: controle (sem nenhum comando GL entre os swaps). 5 a 9: decisao-swap2.md
// secao 2. Mede, nunca reprova: cada variante imprime uma linha MEASURED.
namespace {

using fn_clear = void (*)(unsigned);
using fn_clear_color = void (*)(float, float, float, float);
using fn_flush = void (*)();
using fn_get_string = const unsigned char *(*)(unsigned);

constexpr unsigned k_gl_color_buffer_bit = 0x00004000;
constexpr unsigned k_gl_renderer = 0x1F01;
constexpr unsigned k_gl_version = 0x1F02;

struct gl_calls {
    fn_clear clear = nullptr;
    fn_clear_color clear_color = nullptr;
    fn_flush flush = nullptr;
    fn_get_string get_string = nullptr;
};

gl_calls resolve(const glintfx::platform::win32_gl_context_adapter &context) {
    gl_calls g;
    g.clear = reinterpret_cast<fn_clear>(context.proc_address("glClear"));
    g.clear_color = reinterpret_cast<fn_clear_color>(context.proc_address("glClearColor"));
    g.flush = reinterpret_cast<fn_flush>(context.proc_address("glFlush"));
    g.get_string = reinterpret_cast<fn_get_string>(context.proc_address("glGetString"));
    return g;
}

std::string gl_text(const gl_calls &g, unsigned name) {
    if (g.get_string == nullptr) {
        return "sem_glGetString";
    }
    const unsigned char *s = g.get_string(name);
    return s == nullptr ? "nulo" : std::string(reinterpret_cast<const char *>(s));
}

void draw(const gl_calls &g) {
    if (g.clear_color != nullptr && g.clear != nullptr) {
        g.clear_color(0.25F, 0.5F, 0.75F, 1.0F);
        g.clear(k_gl_color_buffer_bit);
    }
}

struct recorder {
    std::string swaps;
    std::string errors;
    void add(const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &r) {
        if (!swaps.empty()) {
            swaps += ",";
            errors += ",";
        }
        if (r.has_error()) {
            swaps += std::string("erro:") +
                     std::string(glintfx::gltfx_err_code_name(r.err().code())) + ":" +
                     std::string(r.err().rejected_value());
            errors += std::to_string(static_cast<std::int64_t>(r.err().os_error_code()));
        } else {
            swaps +=
                r.value() == glintfx::gltfx_present_outcome::presented ? "ok" : "skipped_hidden";
            errors += "0";
        }
    }
};

void variant(int n) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 320;
    desc.logical_height = 240;
    desc.title = "glintfx swap2 probe";
    if (display.open().has_error() || window.open(display, desc).has_error()) {
        std::println("MEASURED variante={} swaps=setup_falhou os_errors=0 visivel_antes_ultimo=0 "
                     "swap_calls=0 gl_renderer=? gl_version=?",
                     n);
        return;
    }
    const HWND hwnd = window.native_handle();
    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    if (context.open(window, options).has_error() || context.make_current().has_error()) {
        std::println(
            "MEASURED variante={} swaps=contexto_falhou os_errors=0 visivel_antes_ultimo=0 "
            "swap_calls=0 gl_renderer=? gl_version=?",
            n);
        return;
    }
    const gl_calls g = resolve(context);
    const std::string renderer = gl_text(g, k_gl_renderer);
    const std::string version = gl_text(g, k_gl_version);
    recorder rec;
    int visible_before_last = 0;
    auto swap = [&] {
        visible_before_last = ::IsWindowVisible(hwnd) != 0 ? 1 : 0;
        rec.add(context.swap_buffers());
    };
    switch (n) {
    case 1:
        swap();
        swap();
        break;
    case 2:
        swap();
        (void)display.pump_events();
        swap();
        break;
    case 3:
        swap();
        ::ShowWindow(hwnd, SW_HIDE);
        (void)display.pump_events();
        swap();
        break;
    case 4:
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE); // mostrada por fora, show_window_now nao age
        (void)display.pump_events();
        swap();
        swap();
        break;
    case 5: // desenho entre os swaps
        swap();
        draw(g);
        swap();
        break;
    case 6: // swap vazio falha; recupera no quadro seguinte com desenho
        swap();
        swap();
        draw(g);
        swap();
        break;
    case 7: // dez quadros, desenho antes de cada um
        for (int i = 0; i < 10; ++i) {
            draw(g);
            swap();
        }
        break;
    case 8: // contexto reaberto, sem desenho
        swap();
        context.close();
        if (context.open(window, options).has_error() || context.make_current().has_error()) {
            rec.swaps += ",reabertura_falhou";
            rec.errors += ",0";
            break;
        }
        swap();
        break;
    case 9: // glFlush nao desenha
        swap();
        if (g.flush != nullptr) {
            g.flush();
        }
        swap();
        break;
    default:
        break;
    }
    std::println("MEASURED variante={} swaps={} os_errors={} visivel_antes_ultimo={} swap_calls={} "
                 "gl_renderer={} gl_version={}",
                 n, rec.swaps, rec.errors, visible_before_last, context.swap_calls_issued(),
                 renderer, version);
}

} // namespace

GLINTFX_TEST(win32_swap2_probe) {
    for (int n = 1; n <= 9; ++n) {
        variant(n);
    }
}

#endif
