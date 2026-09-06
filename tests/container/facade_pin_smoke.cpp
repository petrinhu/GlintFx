// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>

#include <wayland-client.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/wayland/egl_context_adapter.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// facade_pin_smoke.cpp - FACADE-PIN, T0 (docs/plano-conserto-fachadas-
// uaf.md sec. 8): the FIRST thing this fatia runs, before any conserto
// touches a single adapter - the measurement the whole plan hinges on.
// Opens display and window through the PUBLIC API (glintfx::gltfx_
// display/gltfx_window, the exact path a real consumer takes), reaches
// the opaque impl through the two passkeys the API already grants
// (display_internal_access::get(), window_internal_access::get() -
// include/glintfx/platform/window/display.hpp/window.hpp), and prints,
// side by side, &impl->adapter (the address the object ACTUALLY lives
// at right now) against wl_proxy_get_user_data() (the address the
// system was TOLD to call back into) for four proxies: wl_registry
// (display side), wl_surface/xdg_surface/xdg_toplevel (window side).
//
// THE FIFTH PAIR IS DELIBERATELY NOT REACHED THROUGH gltfx_gl_context:
// that public handle (include/glintfx/platform/gl/context.hpp) has no
// passkey into gl_context_impl - and this plan's own sec. 7.5 promises
// zero changes anywhere under include/glintfx/, so none is added here
// either. Instead, this fixture reproduces the EXACT shape gl_context_
// facade.cpp's own open() uses AFTER this fatia's own conserto (sec.
// 4/7.4): a wayland_egl_context_adapter allocated directly at its final
// address (`new`), opened THERE, never moved (wayland_egl_context_
// adapter is pinned - its move is deleted, the same fix this fatia
// applies to every adapter under src/platform/**). swap_buffers() runs
// on that same heap-owned object, the same way gltfx_gl_context::
// swap_buffers() (gl_context_facade.cpp) only ever reaches the adapter
// through m_impl->adapter.
//
// WHY THIS IS THE FIRST STEP, NOT THE LAST (docs/plano-conserto-
// fachadas-uaf.md sec. 13, item 1): T0 ran FIRST against the tree
// BEFORE this fatia's conserto touched a single adapter - back then,
// gl_context_facade.cpp still built the adapter on the stack and moved
// it into the heap afterward, and THIS SAME fixture (with std::move in
// place of `new` + open()) measured the mechanism this plan's own sec.
// 13 required: if it had printed five EQUAL pairs against that
// pre-conserto tree, the plan's whole account of the mechanism would
// have been wrong, and no conserto should have been trusted - GODS_
// LAWS.md L-44, a refuted hypothesis is corrected, never patched
// around. Vermelho MEDIDO nesse primeiro passo (eb61382, antes de
// qualquer edicao desta fatia): FOUR pairs different (registry,
// wl_surface, xdg_surface, xdg_toplevel - all four adapters this plan
// makes immobile), ONE pair equal (the EGL frame callback, safe today
// only by call order, sec. 3 #7 of the plan).

namespace {

int g_pares = 0;
int g_iguais = 0;
int g_diferentes = 0;

// Piso de varredura nao-vazia (GODS_LAWS.md L-36/L-40): esta funcao e
// chamada exatamente cinco vezes por main() abaixo, nunca zero - main()
// imprime pares/iguais/diferentes ao final, sempre, mesmo que um par
// intermediario aborte o programa antes (o `printf` de cada par sai
// ANTES de qualquer decisao de sair).
void check_pair(const char *name, const void *impl_address, const void *proxy_user_data) {
    ++g_pares;
    const bool same = impl_address == proxy_user_data;
    if (same) {
        ++g_iguais;
    } else {
        ++g_diferentes;
    }
    std::fprintf(stdout, "facade_pin_smoke: par %s: impl=%p proxy_user_data=%p (%s)\n", name,
                 impl_address, proxy_user_data, same ? "igual" : "diferente");
}

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_smoke.cpp's own header
    // comment on this exact line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "facade_pin_smoke: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    glintfx::display_impl *d_impl = glintfx::display_internal_access::get(display);

    const glintfx::gltfx_window_desc desc{
        .title = "facade_pin_smoke",
        .application_id = "org.glintfx.facade_pin_smoke",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(
            stderr, "facade_pin_smoke: gltfx_window::open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.error().code())).c_str(),
            std::string(window_opened.error().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    glintfx::window_impl *w_impl = glintfx::window_internal_access::get(window);

    // Par 1: wl_registry (display, #1 da varredura do plano).
    check_pair(
        "wl_registry", static_cast<const void *>(&d_impl->connection.adapter()),
        wl_proxy_get_user_data(
            reinterpret_cast<wl_proxy *>(d_impl->connection.adapter().registry())));

    // Pares 2-4: wl_surface, xdg_surface, xdg_toplevel (janela, #4-6
    // da varredura do plano).
    check_pair(
        "wl_surface", static_cast<const void *>(&w_impl->adapter),
        wl_proxy_get_user_data(reinterpret_cast<wl_proxy *>(w_impl->adapter.surface())));
    check_pair(
        "xdg_surface", static_cast<const void *>(&w_impl->adapter),
        wl_proxy_get_user_data(reinterpret_cast<wl_proxy *>(w_impl->adapter.xdg_surface_proxy())));
    check_pair(
        "xdg_toplevel", static_cast<const void *>(&w_impl->adapter),
        wl_proxy_get_user_data(reinterpret_cast<wl_proxy *>(w_impl->adapter.toplevel())));

    // Par 5: o wl_callback pendente do adaptador EGL (#7 da varredura
    // do plano) - reproduzindo, aqui dentro deste fixture, a MESMA
    // sequencia "aloca o lar, abre no lugar" que gl_context_facade.cpp
    // usa de verdade depois do conserto desta fatia (nunca mais
    // "pilha, open(), move para o heap" - wayland_egl_context_adapter e
    // pinned agora, o move nem compila). Ver este arquivo's own header
    // comment acima para por que nao se passa por gltfx_gl_context.
    auto *egl_ptr = new glintfx::platform::wayland_egl_context_adapter();
    const std::span<const glintfx::gltfx_gfx_option_entry> no_options{};
    if (glintfx::gltfx_rslt<void> egl_opened = egl_ptr->open(w_impl->adapter, no_options);
        egl_opened.has_error()) {
        std::fprintf(stderr, "facade_pin_smoke: egl adapter open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(egl_opened.error().code())).c_str(),
                     std::string(egl_opened.error().rejected_value()).c_str());
        delete egl_ptr;
        return EXIT_FAILURE;
    }

    if (glintfx::gltfx_rslt<void> current = egl_ptr->make_current(); current.has_error()) {
        std::fprintf(stderr, "facade_pin_smoke: egl adapter make_current() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(current.error().code())).c_str());
        delete egl_ptr;
        return EXIT_FAILURE;
    }
    if (glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = egl_ptr->swap_buffers();
        presented.has_error()) {
        std::fprintf(stderr, "facade_pin_smoke: egl adapter swap_buffers() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(presented.error().code())).c_str());
        delete egl_ptr;
        return EXIT_FAILURE;
    }

    check_pair("wl_callback_pendente", static_cast<const void *>(egl_ptr),
               wl_proxy_get_user_data(
                   reinterpret_cast<wl_proxy *>(egl_ptr->pending_frame_callback())));

    delete egl_ptr;

    std::fprintf(stdout, "facade_pin_smoke: pares=%d iguais=%d diferentes=%d\n", g_pares, g_iguais,
                 g_diferentes);

    // window/display fecham por RAII ao sair de escopo (gltfx_window::
    // ~gltfx_window()/gltfx_display::~gltfx_display()) - nenhum close()
    // manual necessario, mesmo padrao de window_parity_test.cpp.
    return EXIT_SUCCESS;
}
