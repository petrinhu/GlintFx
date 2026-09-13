// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// tests/container/loop_hidden_test.cpp - LOOP-RUN fatia 8, sub-fatia
// S4 (loop-fix.md sec. S4.4/S4.5/S4.6): the LINUX half of a pair that
// shares ONE ctest name and NOTHING else. The Windows half lives in
// tests/loop_hidden_test.cpp, is a harness case, and minimizes with
// ShowWindow(SW_MINIMIZE)/IsIconic(); this half is a plain int main()
// container fixture and minimizes with xdg_toplevel_set_minimized().
//
// TWO FILES, ONE NAME, NO #if ANYWHERE - the molde tests/two_displays_
// test.cpp + tests/container/two_displays_test.cpp already established
// (tests/CMakeLists.txt's own comment on that pair): the MECHANISM of
// "hide this window" has no common expression across the two systems,
// while the EFFECT the library must produce is identical, and that
// effect is what both files assert. The container carries no test
// harness at all (it stages src/, include/ and tests/parity/ only), so
// one file with two #if branches could not live here even if the
// mechanisms happened to line up.
//
// WHAT THIS FILE MEASURES, AND THE CRITERION, FIXED BEFORE THE FIRST
// RUN (GODS_LAWS.md L-43, sec. S4.4): for EACH vsync mode (on, then
// off), eight assertions -
//   1. the window drew at least once while visible (trivialidade veto:
//      a loop that never presents proves nothing about hiding);
//   2. ten hidden ticks burn at most a quarter of their own wall time
//      in CPU (cpu_ratio_permille <= 250) - a loop that polls instead
//      of sleeping measures ~1000;
//   3. those same ten ticks take at least 500 ms of wall time (the
//      library's own 100 ms hidden-wait budget, nine times over, with
//      generous slack);
//   4. nothing was drawn while hidden, WHENEVER the environment
//      reported hiding at all (hidden_detected, printed always, even
//      as zero - sec. S4.3 item 2: silence is not data);
//   5. P-a/P-b still hold on every hidden tick;
//   6. if this compositor speaks xdg_wm_base >= 6 it must set the
//      `suspended` state bit while minimized - unconditional, and the
//      single measurement D2 makes the whole fatia hinge on;
//   7. after restoring, drawing comes back within ten ticks - on THIS
//      system conditioned on `restored == 1`, because xdg-shell has no
//      unset_minimized request and restoring is the compositor's
//      decision, never ours (sec. S4.3 item 3);
//   8. no swap_buffers() failure outside the three-factor tolerance.
//
// WHAT IT DOES NOT MEASURE, DECLARED: occlusion by another window (no
// executor provokes it), focus loss, a real graphics board, and the
// beat of a frame cap against a real refresh rate. Those stay declared
// absences in docs/gl-loop-portability-matrix.md, never "proven".
//
//
// ===================================================================
// DUAS TROCAS DE INSTRUMENTO, 13/09/2026, com o ANTES e o DEPOIS
// (ordem do orquestrador: justificar o observavel novo pelo TEXTO da
// promessa, citando onde ela diz isso - nunca por "assim o teste
// passa"). As duas nasceram de uma execucao real em container que
// mediu, nao de leitura:
//
// (1) O OBSERVAVEL DE "ESTA OCULTA". ANTES: `hidden_detected` era
//     "present() devolveu skipped_hidden", e `rendered_while_hidden`
//     contava saidas `presented`. Medido: os dois sairam ZERO num
//     ambiente que escondia a janela de verdade (`suspended_while_
//     minimized` saiu 1 no mesmo log) - porque com o bit ligado a
//     sonda decide ANTES, `should_render` ja vem falso, e present()
//     nunca chega a ser chamado. O instrumento nao conseguia enxergar
//     o acerto. DEPOIS: os dois passam a ler `gltfx_frame_tick::
//     should_render`.
//     POR QUE ESSE E O OBSERVAVEL CERTO, pelo texto da promessa:
//       - include/glintfx/platform/loop/loop.hpp:112-116 define o
//         proprio campo como "Whether THIS tick should call on_
//         render()/present()" - e' ele, nao o retorno de present(),
//         que diz se o laco mandou desenhar;
//       - loop.hpp:356-359 (P8) promete, para uma janela oculta, que
//         "on_render is not called" - nao que present() foi pulado;
//       - loop.hpp:329-330 (P4) fecha a frase: "on_frame keeps
//         running every tick regardless; only on_render stops".
//     `rendered_while_hidden` conta, entao, os tiques com should_
//     render VERDADEIRO que acontecem DEPOIS do primeiro tique falso.
//     O recorte nao e conveniencia: pela formula de P4 (loop.hpp:325-
//     330) o primeiro tique do trecho oculto tem should_render
//     verdadeiro POR CONSTRUCAO, porque a ultima apresentacao ainda
//     foi bem-sucedida - o laco precisa de uma apresentacao para
//     aprender. O que P8 promete e que, depois de aprender, ele nao
//     volte a mandar desenhar enquanto a janela seguir escondida, e e'
//     exatamente isso que este numero mede.
//
// (2) A SEQUENCIA DOS DOIS MODOS DE SINCRONIA. ANTES: os dois modos
//     compartilhavam UMA janela, e o segundo comecava com ela ainda
//     minimizada, porque restaurar no Wayland e decisao do compositor
//     e este nao restaurou (`restored` saiu 0). Medido: o segundo modo
//     abria com `suspended` ja ligado e nao apresentava um quadro
//     sequer. DEPOIS: cada modo abre a PROPRIA janela, o proprio
//     contexto grafico e o proprio laco, e os destroi ao terminar -
//     nenhum modo herda o estado do anterior. So a conexao de tela e
//     compartilhada.
// ===================================================================
//
// ASCII ONLY IN EVERY LITERAL, same reason as the sibling parity
// fixture: the name this file shares is compiled by MSVC on the other
// side, and this project passes no /utf-8 flag.

namespace {

using gl_enum = unsigned int;
using gl_float = float;

constexpr gl_enum k_gl_color_buffer_bit = 0x00004000;

using gl_clear_color_fn = void (*)(gl_float, gl_float, gl_float, gl_float);
using gl_clear_fn = void (*)(gl_enum);

gl_clear_color_fn g_clear_color = nullptr;
gl_clear_fn g_clear = nullptr;

// Eight assertions per vsync mode, two modes - the constant, never a
// running total (GODS_LAWS.md L-40: a run that stops early prints a
// smaller number and that mismatch is itself the reproval).
constexpr int k_checks_per_mode = 8;
constexpr int k_planned_assertions = 2 * k_checks_per_mode;

int g_assertions_evaluated = 0;
int g_assertions_failed = 0;

void check(const char *mode, const char *key, bool condition, const char *criterion,
           const std::string &observed) {
    ++g_assertions_evaluated;
    if (!condition) {
        ++g_assertions_failed;
    }
    std::fprintf(stdout, "loop_hidden_test: [vsync=%s] %s=%s (criterio %s) %s\n", mode, key,
                 observed.c_str(), criterion, condition ? "OK" : "FALHOU");
}

struct scope_reporter {
    ~scope_reporter() noexcept {
        std::fprintf(stdout, "loop_hidden_test: assercoes %d de %d avaliadas\n",
                     g_assertions_evaluated, k_planned_assertions);
    }
};

[[nodiscard]] std::string to_text(long long value) { return std::to_string(value); }

[[nodiscard]] std::string err_text(const glintfx::gltfx_err &err) {
    return std::string(glintfx::gltfx_err_code_name(err.code())) + "/" +
           std::string(err.rejected_value());
}

// The SAME three factors in conjunction gl_context_parity_test.cpp:130
// fixes, plus gpu().kind == software and "another swap of THIS SAME
// execution already succeeded" - decisao do lider, 07/09/2026,
// "tolerar, mas so sob prova".
[[nodiscard]] bool should_tolerate_swap_failure(const glintfx::gltfx_err &err,
                                                glintfx::gltfx_gpu_kind gpu_kind,
                                                bool any_other_swap_succeeded) noexcept {
    if (err.code() != glintfx::gltfx_err_code::platform_failure) {
        return false;
    }
    if (err.rejected_value() != std::string_view{"swap_buffers"}) {
        return false;
    }
    if (err.os_error_code() != 0) {
        return false;
    }
    if (gpu_kind != glintfx::gltfx_gpu_kind::software) {
        return false;
    }
    return any_other_swap_succeeded;
}

// Everything ONE vsync mode produced, so main() can fold the two modes
// into the aggregate MEASURED keys sec. S4.5 fixes by name.
struct mode_result {
    bool ran = false;
    bool hidden_detected = false;
    bool suspended_seen = false;
    bool restored = false;
    int rendered_while_hidden = 0;
    long long cpu_ratio_permille = 0;
    long long visible_wall_ms = 0;
};

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_smoke.cpp's own header
    // comment on this exact line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const scope_reporter reporter;

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "loop_hidden_test: gltfx_display::open() failed: %s\n",
                     err_text(display_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());
    glintfx::display_impl *d_impl = glintfx::display_internal_access::get(display);

    // The ONE fact this side measures that the Windows side has no
    // equivalent for (tests/measured_exceptions.txt carries its line):
    // a compositor below xdg_wm_base 6 has no `suspended` state to
    // send at all, so assertion 6 below is written against THIS number,
    // never against an assumption about which compositor is running.
    std::uint32_t xdg_wm_base_version = 0;
    if (const glintfx::platform::wayland_global *wm_base =
            d_impl->connection.adapter().globals().find_by_interface("xdg_wm_base");
        wm_base != nullptr) {
        xdg_wm_base_version = wm_base->version;
    }
    std::fprintf(stdout, "MEASURED loop_hidden_test.xdg_wm_base_version=%u\n", xdg_wm_base_version);

    bool any_swap_succeeded = false;
    mode_result results[2];
    const char *mode_names[2] = {"on", "off"};
    const std::int64_t mode_values[2] = {1, 0};

    for (int mode_index = 0; mode_index < 2; ++mode_index) {
        const char *mode = mode_names[mode_index];
        mode_result &result = results[mode_index];
        bool untolerated_swap_failure = false;

        // ACHADO 4 (13/09/2026, medido): CADA modo abre a PROPRIA
        // janela, o proprio contexto grafico e o proprio laco. Antes os
        // dois modos compartilhavam uma janela so, e o segundo herdava
        // ela ainda minimizada - restaurar no Wayland e decisao do
        // compositor, e este nao restaura (`restored` sai 0). Tudo o
        // que nasce aqui morre no fim desta iteracao, por RAII; so a
        // conexao de tela (`display`) atravessa os dois modos.
        const glintfx::gltfx_window_desc desc{
            .title = "janela oculta do laco principal",
            .application_id = "org.glintfx.loop_hidden_test",
            .logical_size = {.width = 640, .height = 480},
        };
        glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
            glintfx::gltfx_window::open(display, desc);
        if (window_opened.has_error()) {
            std::fprintf(stderr, "loop_hidden_test: gltfx_window::open() failed: %s\n",
                         err_text(window_opened.err()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_window window = std::move(window_opened.value());
        glintfx::window_impl *w_impl = glintfx::window_internal_access::get(window);

        const glintfx::gltfx_gl_context_desc empty_desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
            glintfx::gltfx_gl_context::open(window, empty_desc);
        if (context_opened.has_error()) {
            std::fprintf(stderr, "loop_hidden_test: gltfx_gl_context::open() failed: %s\n",
                         err_text(context_opened.err()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_gl_context context = std::move(context_opened.value());
        if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
            std::fprintf(stderr, "loop_hidden_test: make_current() failed: %s\n",
                         err_text(current.err()).c_str());
            return EXIT_FAILURE;
        }

        void *clear_color_addr = context.proc_address("glClearColor");
        void *clear_addr = context.proc_address("glClear");
        if (clear_color_addr == nullptr || clear_addr == nullptr) {
            std::fprintf(stderr, "loop_hidden_test: proc_address falhou\n");
            return EXIT_FAILURE;
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
        // dlsym-style function-to-object-pointer cast every GL loader relies on
        // (gl_context_parity_test.cpp).
        g_clear_color = reinterpret_cast<gl_clear_color_fn>(clear_color_addr);
        g_clear = reinterpret_cast<gl_clear_fn>(clear_addr);
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above

        const glintfx::gltfx_gpu_info gpu = context.gpu();

        glintfx::gltfx_rslt<glintfx::gltfx_loop> loop_opened =
            glintfx::gltfx_loop::open(display, window, context);
        if (loop_opened.has_error()) {
            std::fprintf(stderr, "loop_hidden_test: gltfx_loop::open() failed: %s\n",
                         err_text(loop_opened.err()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_loop loop = std::move(loop_opened.value());

        xdg_toplevel *toplevel = w_impl->adapter.toplevel();
        wl_display *native_display = d_impl->connection.adapter().native_display();
        if (toplevel == nullptr || native_display == nullptr) {
            std::fprintf(stderr,
                         "loop_hidden_test: toplevel=%p native_display=%p - sem o par nao ha "
                         "como esconder a janela\n",
                         static_cast<void *>(toplevel), static_cast<void *>(native_display));
            return EXIT_FAILURE;
        }

        if (glintfx::gltfx_rslt<void> set_vsync = context.set_option(
                {.id = glintfx::gltfx_gfx_option::vsync, .value = mode_values[mode_index]});
            set_vsync.has_error()) {
            std::fprintf(stderr, "loop_hidden_test: set_option(vsync=%s) falhou: %s\n", mode,
                         err_text(set_vsync.err()).c_str());
            return EXIT_FAILURE;
        }

        // --- visible phase: five ticks, at least one real frame ------
        int visible_presented = 0;
        const glintfx::gltfx_time_point visible_start = glintfx::gltfx_now();
        for (int i = 0; i < 5; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            if (ticked.has_error()) {
                std::fprintf(stderr, "loop_hidden_test: [vsync=%s] step() visivel falhou: %s\n",
                             mode, err_text(ticked.err()).c_str());
                return EXIT_FAILURE;
            }
            if (!ticked.value().should_render) {
                continue;
            }
            g_clear_color(0.2F, 0.4F, 0.6F, 1.0F);
            g_clear(k_gl_color_buffer_bit);
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = loop.present();
            if (presented.has_error()) {
                if (should_tolerate_swap_failure(presented.err(), gpu.kind, any_swap_succeeded)) {
                    std::fprintf(stdout,
                                 "DOWNGRADE: loop_hidden_test visible present tolerada "
                                 "- %s, os_error_code=0, gpu().kind=software.\n",
                                 err_text(presented.err()).c_str());
                    continue;
                }
                untolerated_swap_failure = true;
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] present() visivel falhou fora da "
                             "tolerancia: %s\n",
                             mode, err_text(presented.err()).c_str());
                continue;
            }
            if (presented.value() == glintfx::gltfx_present_outcome::presented) {
                ++visible_presented;
                any_swap_succeeded = true;
            }
        }
        const glintfx::gltfx_time_point visible_end = glintfx::gltfx_now();
        result.visible_wall_ms =
            glintfx::gltfx_duration_between(visible_start, visible_end).nanoseconds / 1'000'000;

        check(mode, "visible_presented", visible_presented >= 1, ">= 1 em 5 tiques visiveis",
              to_text(visible_presented));

        // DIAGNOSTICO, nunca assercao nem chave MEASURED (sec. S4.4 nao
        // tem linha para ele): o bit `suspended` lido ENQUANTO a janela
        // ainda esta visivel. Sem esta linha, o valor lido depois de
        // minimizar nao distingue "ligou por causa do minimizar" de "ja
        // estava ligado" - e a decisao D2 da onda depende exatamente
        // dessa distincao.
        const bool suspended_before_hiding =
            window.state(glintfx::gltfx_window_state_bit::suspended);
        std::fprintf(stdout,
                     "loop_hidden_test: [vsync=%s] diagnostico: suspended_antes_de_esconder=%d, "
                     "presents_visiveis_bem_sucedidos=%d de 5\n",
                     mode, suspended_before_hiding ? 1 : 0, visible_presented);

        // --- hide it ------------------------------------------------
        // xdg-shell request, no commit needed: set_minimized is a
        // stateless hint to the compositor (xdg-shell.xml), unlike the
        // fullscreen/maximized pair whose effect arrives through a
        // configure the adapter itself acks. The flush is what puts the
        // request on the wire before the next step() blocks.
        xdg_toplevel_set_minimized(toplevel);
        wl_display_flush(native_display);
        // ACHADO 5 (13/09/2026, medido na execucao que os achados 3 e 4
        // destravaram): o assentamento depois de esconder e' por
        // CONDICAO com prazo, nunca por um numero fixo de tiques.
        // ANTES: cinco tiques secos. Medido: com vsync LIGADO os cinco
        // tiques custavam ~500 ms (a espera de janela oculta ja tinha
        // engatado) e o `configure` do compositor chegava a tempo; com
        // vsync DESLIGADO os mesmos cinco tiques voltavam quase
        // instantaneos, e o bit `suspended` era lido ANTES de o
        // compositor ter respondido - `suspended_while_minimized` saiu
        // 1 num modo e 0 no outro, pela pura velocidade do trecho, com
        // a janela igualmente escondida nos dois (`hidden_detected`
        // saiu 1 nos dois). DEPOIS: cicla ate o bit aparecer, com teto
        // de ciclos; sai na hora quando ele aparece (o caso do Win32,
        // onde WM_SIZE/SIZE_MINIMIZED chega junto com a chamada) e
        // pagina sozinho quando nao aparece (cada ciclo que apresenta
        // engata a espera de 100 ms do proprio laco). Nenhum `sleep`
        // arbitrario, e o criterio asseverado nao mudou uma virgula.
        int settle_cycles = 0;
        for (int i = 0; i < 20; ++i) {
            if (window.state(glintfx::gltfx_window_state_bit::suspended)) {
                break;
            }
            ++settle_cycles;
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> settled = loop.step();
            if (settled.has_error()) {
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] step() pos-minimizar falhou: %s\n", mode,
                             err_text(settled.err()).c_str());
                return EXIT_FAILURE;
            }
            if (!settled.value().should_render) {
                continue;
            }
            g_clear_color(0.4F, 0.4F, 0.4F, 1.0F);
            g_clear(k_gl_color_buffer_bit);
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> settle_present = loop.present();
            if (settle_present.has_error()) {
                if (!should_tolerate_swap_failure(settle_present.err(), gpu.kind,
                                                  any_swap_succeeded)) {
                    untolerated_swap_failure = true;
                    std::fprintf(stderr,
                                 "loop_hidden_test: [vsync=%s] present() pos-minimizar falhou fora "
                                 "da tolerancia: %s\n",
                                 mode, err_text(settle_present.err()).c_str());
                }
                continue;
            }
            if (settle_present.value() == glintfx::gltfx_present_outcome::presented) {
                any_swap_succeeded = true;
            }
        }
        result.suspended_seen = window.state(glintfx::gltfx_window_state_bit::suspended);
        std::fprintf(stdout,
                     "loop_hidden_test: [vsync=%s] diagnostico: ciclos_ate_assentar=%d de 20\n",
                     mode, settle_cycles);

        // --- ten hidden ticks, measured ------------------------------
        bool p_a_holds = true;
        bool p_b_holds = true;
        // "O laco recusou desenhar ao menos uma vez neste trecho" - ver
        // o bloco ACHADO 3 mais abaixo para por que este, e nao o
        // retorno de present(), e' o observavel que P8 descreve.
        bool hidden_detected_in_stretch = false;
        const std::clock_t cpu_start = std::clock();
        const glintfx::gltfx_time_point hidden_start = glintfx::gltfx_now();
        for (int i = 0; i < 10; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            if (ticked.has_error()) {
                std::fprintf(stderr, "loop_hidden_test: [vsync=%s] step() oculto falhou: %s\n",
                             mode, err_text(ticked.err()).c_str());
                return EXIT_FAILURE;
            }
            const glintfx::gltfx_frame_tick tick = ticked.value();
            if (tick.last_present == glintfx::gltfx_present_outcome::presented &&
                !tick.should_render) {
                p_a_holds = false;
            }
            if (!tick.should_render &&
                tick.last_present != glintfx::gltfx_present_outcome::skipped_hidden) {
                p_b_holds = false;
            }
            // ACHADO 3 (13/09/2026, medido): O OBSERVAVEL DE "O LACO
            // RECUSOU DESENHAR" e' should_render, nunca o retorno de
            // present(). loop.hpp:112-116 define o campo como "Whether
            // THIS tick should call on_render()/present()"; P8
            // (loop.hpp:356-359) promete que numa janela oculta "on_
            // render is not called"; P4 (loop.hpp:329-330) fecha:
            // "on_frame keeps running every tick regardless; only on_
            // render stops". Ler present() nao enxerga nada disso -
            // quando a sonda decide primeiro, present() nem chega a ser
            // chamado.
            if (!tick.should_render) {
                hidden_detected_in_stretch = true;
                continue;
            }
            // Depois de o laco ter percebido uma vez, mandar desenhar
            // de novo com a janela ainda escondida e' o que P8 proibe.
            // O primeiro tique do trecho NAO conta: pela formula de P4
            // (loop.hpp:325-330) ele vem verdadeiro por construcao,
            // porque a ultima apresentacao ainda foi bem-sucedida - o
            // laco precisa de uma apresentacao para aprender.
            if (hidden_detected_in_stretch) {
                ++result.rendered_while_hidden;
            }
            g_clear_color(0.6F, 0.2F, 0.2F, 1.0F);
            g_clear(k_gl_color_buffer_bit);
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = loop.present();
            if (presented.has_error()) {
                if (should_tolerate_swap_failure(presented.err(), gpu.kind, any_swap_succeeded)) {
                    std::fprintf(stdout,
                                 "DOWNGRADE: loop_hidden_test hidden present tolerada - "
                                 "%s, os_error_code=0, gpu().kind=software.\n",
                                 err_text(presented.err()).c_str());
                    continue;
                }
                untolerated_swap_failure = true;
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] present() oculto falhou fora da "
                             "tolerancia: %s\n",
                             mode, err_text(presented.err()).c_str());
                continue;
            }
            if (presented.value() == glintfx::gltfx_present_outcome::presented) {
                any_swap_succeeded = true;
            }
        }
        result.hidden_detected = hidden_detected_in_stretch;
        const glintfx::gltfx_time_point hidden_end = glintfx::gltfx_now();
        const std::clock_t cpu_end = std::clock();
        const std::int64_t wall_ns =
            glintfx::gltfx_duration_between(hidden_start, hidden_end).nanoseconds;
        const std::int64_t cpu_ns = static_cast<std::int64_t>(
            (static_cast<double>(cpu_end - cpu_start) / static_cast<double>(CLOCKS_PER_SEC)) *
            1'000'000'000.0);
        result.cpu_ratio_permille = wall_ns > 0 ? (cpu_ns * 1000) / wall_ns : 1000;

        std::fprintf(stdout, "loop_hidden_test: [vsync=%s] wall_ns=%lld cpu_ns=%lld\n", mode,
                     static_cast<long long>(wall_ns), static_cast<long long>(cpu_ns));

        check(mode, "cpu_ratio_permille", result.cpu_ratio_permille <= 250, "<= 250 (25% de CPU)",
              to_text(result.cpu_ratio_permille));
        check(mode, "wall_ns", wall_ns >= 500'000'000, ">= 500 ms em 10 tiques ocultos",
              to_text(wall_ns / 1'000'000) + " ms");
        check(mode, "rendered_while_hidden",
              !result.hidden_detected || result.rendered_while_hidden == 0,
              "== 0 quando hidden_detected == 1",
              to_text(result.rendered_while_hidden) +
                  " (hidden_detected=" + to_text(result.hidden_detected ? 1 : 0) + ")");
        check(mode, "P_a_e_P_b_no_trecho_oculto", p_a_holds && p_b_holds,
              "presented => should_render, e !should_render => skipped_hidden",
              std::string(p_a_holds ? "P_a ok" : "P_a violada") + ", " +
                  (p_b_holds ? "P_b ok" : "P_b violada"));
        // D2, the single measurement the whole fatia hinges on: with a
        // wm_base that HAS the state, the library must be seeing it.
        check(mode, "suspended_while_minimized", xdg_wm_base_version < 6 || result.suspended_seen,
              "xdg_wm_base >= 6 obriga o bit suspended ligado",
              to_text(result.suspended_seen ? 1 : 0) +
                  " (xdg_wm_base_version=" + to_text(xdg_wm_base_version) + ")");

        // --- restore it ----------------------------------------------
        // xdg-shell has NO unset_minimized request (xdg-shell.xml):
        // restoring is the compositor's decision. The fixed fallback
        // (sec. S4.3 item 3, chosen before any data existed) is to ask
        // for fullscreen, which forces a remap on any compositor that
        // honours it, then drop it again. `restored` records what
        // actually happened, and conditions assertion 7 below - never
        // hides it.
        xdg_toplevel_set_fullscreen(toplevel, nullptr);
        wl_surface_commit(w_impl->adapter.surface());
        wl_display_flush(native_display);
        for (int i = 0; i < 10; ++i) {
            if (glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
                ticked.has_error()) {
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] step() pos-restaurar falhou: "
                             "%s\n",
                             mode, err_text(ticked.err()).c_str());
                return EXIT_FAILURE;
            }
            if (!window.state(glintfx::gltfx_window_state_bit::suspended)) {
                result.restored = true;
                break;
            }
        }

        int recovery_ticks = 0;
        bool recovered = false;
        for (int i = 0; i < 10 && !recovered; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            if (ticked.has_error()) {
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] step() de recuperacao falhou: "
                             "%s\n",
                             mode, err_text(ticked.err()).c_str());
                return EXIT_FAILURE;
            }
            ++recovery_ticks;
            if (!ticked.value().should_render) {
                continue;
            }
            g_clear_color(0.2F, 0.6F, 0.2F, 1.0F);
            g_clear(k_gl_color_buffer_bit);
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = loop.present();
            if (presented.has_error()) {
                if (should_tolerate_swap_failure(presented.err(), gpu.kind, any_swap_succeeded)) {
                    std::fprintf(stdout,
                                 "DOWNGRADE: loop_hidden_test recovery present tolerada "
                                 "- %s, os_error_code=0, gpu().kind=software.\n",
                                 err_text(presented.err()).c_str());
                    continue;
                }
                untolerated_swap_failure = true;
                std::fprintf(stderr,
                             "loop_hidden_test: [vsync=%s] present() de recuperacao falhou fora "
                             "da tolerancia: %s\n",
                             mode, err_text(presented.err()).c_str());
                continue;
            }
            if (presented.value() == glintfx::gltfx_present_outcome::presented) {
                recovered = true;
                any_swap_succeeded = true;
            }
        }
        check(mode, "recuperacao_em_ate_10_tiques", !result.restored || recovered,
              "presented em <= 10 tiques (condicionada a restored == 1 neste sistema)",
              std::string(recovered ? "recuperou" : "nao recuperou") + " em " +
                  to_text(recovery_ticks) +
                  " tique(s), restored=" + to_text(result.restored ? 1 : 0));
        check(mode, "swap_buffers_fora_da_tolerancia", !untolerated_swap_failure,
              "nenhuma falha de troca de quadro fora dos tres fatores",
              untolerated_swap_failure ? std::string("houve") : std::string("nenhuma"));

        xdg_toplevel_unset_fullscreen(toplevel);
        wl_surface_commit(w_impl->adapter.surface());
        wl_display_flush(native_display);
        result.ran = true;
    }

    // --- the aggregate MEASURED keys (sec. S4.5) --------------------
    // CONSERVATIVE BY CONSTRUCTION: a key that claims "detected" or
    // "restored" does so only when EVERY mode saw it. Folding two
    // modes with OR would let one lucky mode hide the other's silence,
    // which is exactly what the closing criterion reads these keys to
    // decide.
    const bool hidden_detected_all = results[0].hidden_detected && results[1].hidden_detected;
    const bool suspended_all = results[0].suspended_seen && results[1].suspended_seen;
    const bool restored_all = results[0].restored && results[1].restored;
    const long long worst_cpu_ratio = results[0].cpu_ratio_permille > results[1].cpu_ratio_permille
                                          ? results[0].cpu_ratio_permille
                                          : results[1].cpu_ratio_permille;
    // P1's own declared exception (2), inferred from cadence and NEVER
    // read out of the driver: with vsync OFF, five real presents that
    // together cost under 200 ms mean the interval was honoured. A zero
    // here says THIS executor's driver ignores the request - the matrix
    // records that, and no assertion depends on it.
    const bool swap_interval_honored = results[1].visible_wall_ms < 200;

    std::fprintf(stdout, "MEASURED loop_hidden_test.hidden_detected=%d\n",
                 hidden_detected_all ? 1 : 0);
    std::fprintf(stdout, "MEASURED loop_hidden_test.suspended_while_minimized=%d\n",
                 suspended_all ? 1 : 0);
    std::fprintf(stdout, "MEASURED loop_hidden_test.rendered_while_hidden_vsync_off=%d\n",
                 results[1].rendered_while_hidden);
    std::fprintf(stdout, "MEASURED loop_hidden_test.restored=%d\n", restored_all ? 1 : 0);
    std::fprintf(stdout, "MEASURED loop_hidden_test.cpu_ratio_permille=%lld\n", worst_cpu_ratio);
    std::fprintf(stdout, "MEASURED loop_hidden_test.swap_interval_honored=%d\n",
                 swap_interval_honored ? 1 : 0);

    if (!results[0].ran || !results[1].ran) {
        std::fprintf(stderr, "loop_hidden_test: FAIL - um dos dois modos de vsync nao rodou\n");
        return EXIT_FAILURE;
    }
    if (g_assertions_evaluated != k_planned_assertions) {
        std::fprintf(stderr,
                     "loop_hidden_test: FAIL - %d assercoes avaliadas, %d planejadas "
                     "(GODS_LAWS.md L-40)\n",
                     g_assertions_evaluated, k_planned_assertions);
        return EXIT_FAILURE;
    }
    if (g_assertions_failed > 0) {
        std::fprintf(stderr, "loop_hidden_test: FAIL - %d de %d assercoes reprovaram\n",
                     g_assertions_failed, g_assertions_evaluated);
        return EXIT_FAILURE;
    }

    // RAII teardown in reverse order of creation - no explicit close()
    // anywhere (GODS_LAWS.md L-22).
    return EXIT_SUCCESS;
}
