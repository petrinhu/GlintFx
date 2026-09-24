// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>

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

// egl_error_read_inside_swap_smoke.cpp - EGL-DEAD-DISPLAY-GUARD S0
// (/var/tmp/glintfx-plan/plano-segv-swap-protocol-error.md sec. 3,
// D-S8, GODS_LAWS.md L-20/L-40): reproduces the DEFECT this whole
// item exists to close, INSIDE the tree, without wire_relay and
// without the window_adapter.cpp:294 mutation the earlier report used
// (D-S8's own "sem rele e sem variante"). egl_protocol_error_smoke.cpp
// (one directory over) already proves a real protocol violation on
// this SAME kwin_wayland --virtual compositor comes back through
// wayland_egl_context_adapter::swap_buffers() correctly NAMED - it
// exercises the vsync=on path (D-W6b-29's own "2nd swap"), which
// already checks wl_display_get_error() whenever eglSwapBuffers()
// itself fails. THIS fixture exercises the vsync=off path instead
// (plano sec. 2, row "eglSwapBuffers no vsync=off"): under the
// llvmpipe/swrast Mesa backend this container runs (no /dev/dri,
// GODS_LAWS.md L-09 rule 4), dri2_wl_swrast_swap_buffers_with_damage()
// returns EGL_TRUE UNCONDITIONALLY (plano sec. 1.1) even once the
// compositor has already killed the connection - so
// swap_buffers()'s own vsync=off branch, which today only reads
// wl_display_get_error() when the EGL call itself failed, answers
// `presented` over a connection that is already dead. That is the
// exact invariant this fixture polices after every swap: `presented`
// while wl_display_get_error() != 0 is never allowed to happen.
//
// Contra o produto de hoje (antes de S2 nesta mesma onda), a previsao
// (plano sec. 2, INF) e' que a invariante reprova na primeira troca
// depois que o Mesa le' o erro por dentro do proprio eglSwapBuffers
// (o throttle() de dri2_wl_swrast_swap_buffers_with_damage, plano
// sec. 1.1). Depois de S2 (a guarda lida ANTES e DEPOIS de toda
// chamada EGL que fala com o fio), a mesma troca responde
// `platform_failure`/`rejected_value=="wl_surface"` em vez de
// `presented` - e' esse par vermelho-depois-verde que prova o
// conserto, nao um crash: nada aqui exige que o processo caia (essa
// prova mais funda, por mutacao numa copia fora da arvore, mora no
// relatorio da sub-fatia, GODS_LAWS.md L-27).

namespace {

constexpr std::int32_t kWidth = 320;
constexpr std::int32_t kHeight = 240;
constexpr int kMaxSwapsAfterProvocation = 10;

} // namespace

int main() {
    glintfx::container_fixture::checked_setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;
    const glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_error_read_inside_swap_smoke: display open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    const glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: shell.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(shell_opened.err().code())).c_str(),
            std::string(shell_opened.err().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_window_adapter window;
    glintfx::platform::wayland_window_desc const desc{
        .logical_width = static_cast<std::uint32_t>(kWidth),
        .logical_height = static_cast<std::uint32_t>(kHeight),
        .title = "egl_error_read_inside_swap_smoke",
        .application_id = "org.glintfx.egl_error_read_inside_swap_smoke",
    };
    const glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
    if (window_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: window.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
            std::string(window_opened.err().rejected_value()).c_str());
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_egl_context_adapter context;
    const glintfx::gltfx_rslt<void> context_opened =
        context.open(window, std::span<const glintfx::gltfx_gfx_option_entry>{});
    if (context_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: context.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(context_opened.err().code())).c_str(),
            std::string(context_opened.err().rejected_value()).c_str());
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // Reach ONE presented frame with the default (vsync=on) options
    // before touching anything - same shape egl_protocol_error_smoke.
    // cpp already uses, so the provocation below always lands on a
    // context that genuinely finished negotiating its first frame.
    bool presented_once = false;
    for (int attempt = 0; attempt < 5 && !presented_once; ++attempt) {
        const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
        if (swapped.has_error()) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "egl_error_read_inside_swap_smoke: swap_buffers() before provoking anything "
                "failed: %s (rejected_value=%s)\n",
                std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str(),
                std::string(swapped.err().rejected_value()).c_str());
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        presented_once = swapped.value() == glintfx::gltfx_present_outcome::presented;
    }
    if (!presented_once) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_error_read_inside_swap_smoke: never reached a presented frame before "
                    "provoking anything\n");
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // vsync=off is the branch this fixture exists to exercise (plano
    // sec. 2): the vsync=on path already checks wl_display_get_error()
    // whenever eglSwapBuffers() itself reports failure, and llvmpipe's
    // own swrast swap NEVER reports failure (plano sec. 1.1) - so only
    // vsync=off can observe the defect this item closes.
    const glintfx::gltfx_rslt<void> vsync_off =
        context.apply_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
    if (vsync_off.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: apply_option(vsync=0) failed: %s "
            "(rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(vsync_off.err().code())).c_str(),
            std::string(vsync_off.err().rejected_value()).c_str());
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // THE PROVOCATION (same shape egl_protocol_error_smoke.cpp already
    // proved rejected by this SAME compositor): wl_surface.set_buffer_
    // scale requires scale >= 1 (wayland.xml, wl_surface::error::
    // invalid_scale) - a deliberate violation on wl_surface itself.
    wl_surface_set_buffer_scale(window.surface(), 0);
    wl_display_flush(adapter.native_display());

    bool reported_error = false;
    glintfx::gltfx_err_code reported_code{};
    std::string reported_rejected;
    int swaps_issued = 0;
    bool invariant_broken = false;
    int invariant_broken_at_swap = -1;

    for (int swap_index = 1; swap_index <= kMaxSwapsAfterProvocation && !reported_error;
         ++swap_index) {
        const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
        ++swaps_issued;

        if (swapped.has_error()) {
            reported_error = true;
            reported_code = swapped.err().code();
            reported_rejected = std::string(swapped.err().rejected_value());
            break;
        }

        if (swapped.value() == glintfx::gltfx_present_outcome::presented &&
            wl_display_get_error(adapter.native_display()) != 0) {
            invariant_broken = true;
            invariant_broken_at_swap = swap_index;
            break;
        }
    }

    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED egl_error_read_inside_swap_smoke.swaps_after_provocation=%d\n",
        swaps_issued);

    if (invariant_broken) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: swap %d respondeu presented sobre uma "
            "conexao ja morta (wl_display_get_error()!=0) - EGL-DEAD-DISPLAY-GUARD S0, "
            "defeito reproduzido\n",
            invariant_broken_at_swap);
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    if (!reported_error) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: %d trocas depois da provocacao, "
            "swap_buffers() nunca reportou erro nem violou a invariante - a fixture nao "
            "pode provar nada (GODS_LAWS.md L-42/L-40), reprovando em vez de passar em "
            "silencio\n",
            kMaxSwapsAfterProvocation);
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    if (reported_code != glintfx::gltfx_err_code::platform_failure ||
        reported_rejected != std::string_view{"wl_surface"}) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_error_read_inside_swap_smoke: swap_buffers falhou com forma inesperada: "
            "code=%s rejected_value=%s (esperado platform_failure/wl_surface)\n",
            std::string(glintfx::gltfx_err_code_name(reported_code)).c_str(),
            reported_rejected.c_str());
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    context.close();
    window.close();
    shell.close();
    adapter.close();

    glintfx::container_fixture::checked_fprintf(
        stdout,
        "egl_error_read_inside_swap_smoke: swap_buffers respondeu platform_failure/"
        "wl_surface antes de violar a invariante - guarda EGL-DEAD-DISPLAY-GUARD provada\n");
    return EXIT_SUCCESS;
}
