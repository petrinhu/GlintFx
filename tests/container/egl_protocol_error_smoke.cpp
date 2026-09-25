// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>

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

// egl_protocol_error_smoke.cpp - GL-CONTEXT fatia 5, P-GL (docs/plano-
// w6b-fatias-5.md sec. 3.1/D-W6b-28): the PERMANENT proof that a real
// protocol violation on this SAME kwin_wayland --virtual compositor
// comes back through wayland_egl_context_adapter::swap_buffers() named
// by the REAL interface that reprovou (connection_failure.{hpp,cpp},
// this fatia's own shared atom) - not just something a reviewer's
// mutation happens to exercise once. window_smoke.cpp (one directory
// over, aposentado in THIS SAME commit, D-W6b-12) proved the identical
// shape one layer down, over wayland_window_adapter alone, before this
// project had any GL context to swap_buffers() through at all - this
// fixture is that same proof, moved up to the layer that actually
// SWAPS, over adaptadores internos (wayland_display_adapter/wayland_
// shell_adapter/wayland_window_adapter/wayland_egl_context_adapter
// directly, never the public gltfx_* facade - the same "one layer
// below any handle" convention egl_probe_smoke.cpp already documents).
//
// ARVORE DE DECISAO FIXADA ANTES DO DADO (D-W6b-29, GODS_LAWS.md L-43):
// the PRIMARY provocation is `wl_surface_set_buffer_scale(surface, 0)`
// - wl_surface's own protocol requires scale >= 1 (wayland.xml, wl_
// surface::error::invalid_scale), so this is a deliberate violation ON
// THE wl_surface INTERFACE, exactly the interface connection_failure.
// cpp's build_connection_failure() is meant to name back through
// rejected_value(). `MEASURED egl_protocol_error_smoke.provoked=0`
// means the compositor did NOT reject it (nobody had measured, before
// this fatia's own busca - docs/plano-w6b-fatias-5.md sec. 1 - that
// `kwin_wayland --virtual` enforces this particular rule) - the
// RESERVE provocation is then `xdg_toplevel_set_max_size(-1, -1)`
// (xdg-shell.xml requires a non-negative size, 0 meaning "no limit"),
// naming `xdg_toplevel` instead. If NEITHER is ever rejected, this
// fixture FAILS rather than pass in silence (GODS_LAWS.md L-42's own
// "nao ha medicao que confirma ausencia").
//
// wl_shm/wl_surface.attach ARE NOT NEEDED HERE (unlike window_smoke.
// cpp's own D-W5-9): eglSwapBuffers() already attaches and commits a
// REAL EGL-rendered buffer on every successful swap_buffers() call -
// the protocol violation this fixture provokes needs nothing more than
// a context that has already reached a presented frame once.

namespace {

constexpr std::int32_t kWidth = 320;
constexpr std::int32_t kHeight = 240;

// EGL-DEAD-DISPLAY-GUARD S4 (docs/plano-egl-dead-display-guard.md sec.
// 5, D-S7): replaces the old fixed 2-attempt/100ms swap_buffers()
// budget (D-W6b-29) with wl_display_roundtrip() - the PROTOCOL's own
// barrier (wayland.xml, wl_display.sync: "a barrier to ensure all
// previous requests and the resulting events have been handled").
// Blocks exactly until the compositor either answers the sync (>= 0,
// the provocation was ACCEPTED) or never can, because the connection
// already died trying (-1, REJECTED). The M-1 measurement (24/09/2026,
// DECISOES_AUTONOMAS.md, 20 rounds through wire_relay, container novo
// por rodada) found the old fixed budget missed the real error 12 of
// 20 times - never because the S2/S3 guard was wrong (all 12 rounds
// read wl_display_get_error()==0 on every attempt, never the S2
// defect the guard closes), but because the error can legitimately
// arrive later than two 100ms windows allow; 6 of those 12 even
// misattributed the PRIMARY provocation's own late error to the
// RESERVE one. A protocol barrier has no such window to miss.
//
// The barrier only says ACCEPTED/REJECTED - the actual gltfx_err_code/
// rejected_value() this fixture asserts on still comes from
// swap_buffers() itself: the S2/S3 guard at swap_buffers()'s own TOP
// already read the exact same error the roundtrip just consumed
// (connection_failure_if_dead(), S1), so this ONE call never touches
// EGL again - it only turns the already-known error into the gltfx_err
// this fixture's assertions expect (proved by S3b: the guard answers
// identically, every time, once the connection is dead).
[[nodiscard]] bool provoke_and_read_refusal(glintfx::platform::wayland_egl_context_adapter &context,
                                            wl_display *display, glintfx::gltfx_err_code &out_code,
                                            std::string &out_rejected) {
    if (wl_display_roundtrip(display) != -1) {
        return false; // the compositor answered the sync - accepted, not rejected
    }
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
    if (!swapped.has_error()) {
        return false;
    }
    out_code = swapped.err().code();
    out_rejected = std::string(swapped.err().rejected_value());
    return true;
}

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (connect_smoke.cpp's own header
    // comment on this exact line; `_IONBF` never triggers MSVC's
    // narrower `_IOLBF`/size-range abort, though this fixture is
    // Linux-only).
    glintfx::container_fixture::checked_setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::platform::wayland_display_adapter adapter;
    const glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_protocol_error_smoke: display open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    const glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_protocol_error_smoke: shell.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(shell_opened.err().code())).c_str(),
            std::string(shell_opened.err().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_window_adapter window;
    glintfx::platform::wayland_window_desc const desc{
        .logical_width = static_cast<std::uint32_t>(kWidth),
        .logical_height = static_cast<std::uint32_t>(kHeight),
        .title = "egl_protocol_error_smoke",
        .application_id = "org.glintfx.egl_protocol_error_smoke",
    };
    const glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
    if (window_opened.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_protocol_error_smoke: window.open() failed: %s (rejected_value=%s)\n",
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
            stderr, "egl_protocol_error_smoke: context.open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(context_opened.err().code())).c_str(),
            std::string(context_opened.err().rejected_value()).c_str());
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // Reach ONE presented frame before provoking anything - proves the
    // protocol-error detection below is exercised against a context
    // that genuinely finished negotiating its first frame, never one
    // still waiting on it (the same "presented, then break the thing
    // that was working" shape D-W6b-29's own gl_context_parity_test
    // case uses).
    bool presented = false;
    for (int attempt = 0; attempt < 5 && !presented; ++attempt) {
        glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
        if (swapped.has_error()) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "egl_protocol_error_smoke: swap_buffers() before provoking anything "
                "failed: %s (rejected_value=%s)\n",
                std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str(),
                std::string(swapped.err().rejected_value()).c_str());
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        presented = swapped.value() == glintfx::gltfx_present_outcome::presented;
    }
    if (!presented) {
        glintfx::container_fixture::checked_fprintf(
            stderr, "egl_protocol_error_smoke: never reached a presented frame before "
                    "provoking anything\n");
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }

    // THE PRIMARY PROVOCATION (D-W6b-28/29): wl_surface.set_buffer_
    // scale requires scale >= 1 - a deliberate violation on wl_surface
    // itself.
    wl_surface_set_buffer_scale(window.surface(), 0);
    wl_display_flush(adapter.native_display());

    glintfx::gltfx_err_code primary_code{};
    std::string primary_rejected;
    const bool primary_provoked =
        provoke_and_read_refusal(context, adapter.native_display(), primary_code, primary_rejected);
    glintfx::container_fixture::checked_fprintf(
        stdout, "MEASURED egl_protocol_error_smoke.provoked=%d\n", primary_provoked ? 1 : 0);

    // EGL-DEAD-DISPLAY-GUARD S3 (docs/plano-egl-dead-display-guard.md
    // sec. 3): a interface que QUALQUER UMA das duas provocações
    // acabar nomeando é a que as três chamadas novas abaixo (depois de
    // ambos os ramos) têm de repetir - nunca um nome hardcoded, porque
    // qual provocador acusa primeiro varia por compositor/corrida.
    std::string established_rejected_value;

    if (primary_provoked) {
        if (primary_code != glintfx::gltfx_err_code::platform_failure ||
            primary_rejected != std::string_view{"wl_surface"}) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "egl_protocol_error_smoke: swap_buffers falhou: code=%s "
                "rejected_value=%s (esperado platform_failure/wl_surface)\n",
                std::string(glintfx::gltfx_err_code_name(primary_code)).c_str(),
                primary_rejected.c_str());
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        glintfx::container_fixture::checked_fprintf(
            stdout, "egl_protocol_error_smoke: swap_buffers falhou: code=platform_failure "
                    "rejected_value=wl_surface (como esperado)\n");
        established_rejected_value = primary_rejected;
    } else {
        // RESERVE PROVOCATION, fixed BEFORE the data existed (D-W6b-29,
        // GODS_LAWS.md L-42/L-43): the compositor did not reject scale
        // 0 - never a second attempt at the SAME request, the next
        // step is xdg_toplevel_set_max_size(-1, -1) (xdg-shell.xml: a
        // negative size is invalid), naming `xdg_toplevel` instead.
        glintfx::container_fixture::checked_fprintf(
            stdout, "egl_protocol_error_smoke: escala 0 nao foi acusada pelo compositor - "
                    "tentando o provocador de reserva (xdg_toplevel_set_max_size(-1,-1))\n");
        xdg_toplevel_set_max_size(window.toplevel(), -1, -1);
        wl_display_flush(adapter.native_display());

        glintfx::gltfx_err_code reserve_code{};
        std::string reserve_rejected;
        const bool reserve_provoked = provoke_and_read_refusal(context, adapter.native_display(),
                                                               reserve_code, reserve_rejected);
        if (!reserve_provoked) {
            glintfx::container_fixture::checked_fprintf(
                stderr, "egl_protocol_error_smoke: NENHUM dos dois provocadores foi acusado "
                        "pelo compositor - esta fixture nao pode provar nada (GODS_LAWS.md "
                        "L-42), reprovando em vez de passar em silencio\n");
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        if (reserve_code != glintfx::gltfx_err_code::platform_failure ||
            reserve_rejected != std::string_view{"xdg_toplevel"}) {
            glintfx::container_fixture::checked_fprintf(
                stderr,
                "egl_protocol_error_smoke: provocador de reserva falhou: code=%s "
                "rejected_value=%s (esperado platform_failure/xdg_toplevel)\n",
                std::string(glintfx::gltfx_err_code_name(reserve_code)).c_str(),
                reserve_rejected.c_str());
            context.close();
            window.close();
            shell.close();
            adapter.close();
            return EXIT_FAILURE;
        }
        glintfx::container_fixture::checked_fprintf(
            stdout, "egl_protocol_error_smoke: provocador de reserva acusado - code="
                    "platform_failure rejected_value=xdg_toplevel (como esperado)\n");
        established_rejected_value = reserve_rejected;
    }

    // EGL-DEAD-DISPLAY-GUARD S3 (docs/plano-egl-dead-display-guard.md
    // sec. 3, D-S3): depois que a conexão já está fatalmente errada,
    // as TRÊS chamadas que ainda falam com o fio (swap_buffers(),
    // make_current(), e um SEGUNDO open()) têm de recusar de imediato,
    // com a MESMA interface que o provocador real acabou de nomear -
    // nunca "funcionar" silenciosamente sobre uma conexão morta, e
    // nunca um nome genérico. close() fica de fora de propósito (D-S3:
    // só libera, nunca fala com o fio de novo).
    const std::uint32_t swap_calls_before_dead_swap = context.swap_calls_issued();
    const glintfx::gltfx_rslt<void> vsync_off_on_dead =
        context.apply_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
    if (vsync_off_on_dead.has_error()) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_protocol_error_smoke: apply_option(vsync=0) sobre conexao ja morta falhou "
            "de forma inesperada: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(vsync_off_on_dead.err().code())).c_str(),
            std::string(vsync_off_on_dead.err().rejected_value()).c_str());
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swap_on_dead = context.swap_buffers();
    if (!swap_on_dead.has_error() ||
        swap_on_dead.err().code() != glintfx::gltfx_err_code::platform_failure ||
        swap_on_dead.err().rejected_value() != std::string_view{established_rejected_value} ||
        context.swap_calls_issued() != swap_calls_before_dead_swap) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_protocol_error_smoke: swap_buffers() sobre conexao ja morta (S3) nao "
            "recusou como esperado - has_error=%d rejected_value=%s swap_calls_issued=%u "
            "(esperado %u, sem chamar o driver)\n",
            swap_on_dead.has_error() ? 1 : 0,
            swap_on_dead.has_error() ? std::string(swap_on_dead.err().rejected_value()).c_str()
                                     : "",
            context.swap_calls_issued(), swap_calls_before_dead_swap);
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout, "egl_protocol_error_smoke: swap_buffers() sobre conexao ja morta (S3) "
                "recusou sem chamar o driver - swap_calls_issued inalterado, como "
                "esperado\n");

    const glintfx::gltfx_rslt<void> make_current_on_dead = context.make_current();
    if (!make_current_on_dead.has_error() ||
        make_current_on_dead.err().code() != glintfx::gltfx_err_code::platform_failure ||
        make_current_on_dead.err().rejected_value() !=
            std::string_view{established_rejected_value}) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_protocol_error_smoke: make_current() sobre conexao ja morta (S3) nao "
            "recusou como esperado - has_error=%d rejected_value=%s\n",
            make_current_on_dead.has_error() ? 1 : 0,
            make_current_on_dead.has_error()
                ? std::string(make_current_on_dead.err().rejected_value()).c_str()
                : "");
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout, "egl_protocol_error_smoke: make_current() sobre conexao ja morta (S3) "
                "recusou, como esperado\n");

    glintfx::platform::wayland_egl_context_adapter second_context;
    const glintfx::gltfx_rslt<void> second_open =
        second_context.open(window, std::span<const glintfx::gltfx_gfx_option_entry>{});
    if (!second_open.has_error() ||
        second_open.err().code() != glintfx::gltfx_err_code::platform_failure ||
        second_open.err().rejected_value() != std::string_view{established_rejected_value}) {
        glintfx::container_fixture::checked_fprintf(
            stderr,
            "egl_protocol_error_smoke: um segundo open() sobre conexao ja morta (S3) nao "
            "recusou como esperado - has_error=%d rejected_value=%s\n",
            second_open.has_error() ? 1 : 0,
            second_open.has_error() ? std::string(second_open.err().rejected_value()).c_str() : "");
        context.close();
        window.close();
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    glintfx::container_fixture::checked_fprintf(
        stdout,
        "egl_protocol_error_smoke: um segundo open() sobre conexao ja morta (S3) "
        "recusou com a interface certa (%s), como esperado\n",
        established_rejected_value.c_str());

    // The connection is now fatally errored - close() on every adapter
    // still has to be safe to call, the same "never touch a fatally-
    // errored display again beyond reading its error" contract
    // display_adapter.hpp already documents (fatal_error_smoke.cpp, one
    // directory over, proves the identical shape one layer down).
    context.close();
    window.close();
    shell.close();
    adapter.close();

    glintfx::container_fixture::checked_fprintf(
        stdout, "egl_protocol_error_smoke: closed cleanly after a provoked protocol error\n");
    return EXIT_SUCCESS;
}
