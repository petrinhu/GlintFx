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
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/window_impl.hpp"

// tests/loop_hidden_test.cpp - LOOP-RUN fatia 8, sub-fatia S4
// (loop-fix.md sec. S4.4/S4.5/S4.6): the WINDOWS half of a pair that
// shares ONE ctest name and NOTHING else. The Linux half lives in
// tests/container/loop_hidden_test.cpp, is a plain int main() staged
// as a container fixture, and hides the window with xdg_toplevel_set_
// minimized(); this half is a harness case and hides it with
// ShowWindow(SW_MINIMIZE), reading IsIconic() back.
//
// TWO FILES, ONE NAME, NO #if OF MECHANISM ANYWHERE (the whole-file
// _WIN32 guard above is the house shape every win32 fixture already
// carries, not a branch of behaviour) - the molde tests/two_displays_
// test.cpp + tests/container/two_displays_test.cpp already established.
// tests/tools/check_test_parity.py pairs the two by this shared name,
// with no line in tests/parity_exceptions.txt and no alias.
//
// THE SAME SIXTEEN ASSERTIONS, IN THE SAME ORDER, AS THE LINUX HALF
// (eight per vsync mode, two modes) - see that file's own header
// comment for the full criterion list. THREE of the sixteen read
// differently here, and each difference is a decision fixed before any
// data existed (sec. S4.3/S4.5):
//   - `suspended_while_minimized` is asserted UNCONDITIONALLY here.
//     Win32 has no protocol version to negotiate: WM_SIZE/SIZE_
//     MINIMIZED is always delivered, so the bit must be on. The Linux
//     half conditions the same assertion on xdg_wm_base >= 6.
//   - `restored` is likewise unconditional: SW_RESTORE always
//     restores. xdg-shell has NO unset_minimized request at all, so
//     the Linux half conditions its recovery assertion on what the
//     compositor actually did.
//   - the ENVIRONMENT is a runner with no graphics board: an
//     opengl32.dll from Mesa sits beside these binaries (tools/ci/
//     install-mesa-opengl32.ps1) and gpu().kind resolves to `software`,
//     which is exactly the third factor of the swap tolerance below.
//
// WHY THIS FIXTURE ALSO RECOMPILES THE FACADES, ON TOP OF LINKING THE
// DLL (MEASURED 15/09/2026, reauditoria W7 - this target DOES link
// glintfx::glintfx, same as every case glintfx_add_test() creates; the
// gêmeo of the comment this one corrects lives in tests/CMakeLists.txt
// right above where this target is registered):
// it needs BOTH the public factories, recompiled standalone (not
// imported from the DLL), AND window_internal_access::get() -
// declared in the public header, DEFINED only in window_facade.cpp,
// never placed in glintfx.dll's own export table (GODS_LAWS.md L-19).
// The same reasoning, and the same GLINTFX_LIBRARY_STATIC_DEFINE, that
// win32_facade_pin_test.cpp already carries (win32_facade_pin_test
// does NOT link the DLL; this fixture does, on top of the recompiled
// sources - the two are not the same
// shape, measured). The passkey is used for exactly ONE thing: the
// HWND to minimize. Everything asserted goes through gltfx_loop/
// gltfx_window's public surface.
//
// This project has no Windows toolchain on the machine that wrote this
// file: it is proven ONLY by the server (sec. S4-F6 - the local
// Windows image is never a source of proof for this fatia).
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
// ===================================================================
// TROCA DE INSTRUMENTO DE CPU, 15/09/2026 (plano /var/tmp/glintfx-plan/
// espera-win32.md sec. 1/4/5.1, execucao 34959739763): std::clock() no
// CRT da Microsoft mede tempo de PAREDE, nao tempo de processador -
// documentacao oficial, verbatim: "The clock function tells how much
// wall-clock time has passed since the CRT initialization during
// process start... To obtain CPU times, use the Win32 GetProcessTimes
// function" (learn.microsoft.com/en-us/cpp/c-runtime-library/reference/
// clock). A ASSINATURA NUMERICA que provou isso naquela execucao:
// cpu_ns sempre multiplo exato de 1_000_000 e a MENOS de 1 ms de
// wall_ns - a marca de CLOCKS_PER_SEC==1000 lendo o MESMO relogio de
// parede. O substituto e' o que a propria Microsoft nomeia:
// GetProcessTimes() (kernel+user, FILETIME em unidades de 100 ns) -
// process_cpu_ns() abaixo. A metade Linux (tests/container/loop_
// hidden_test.cpp) NAO muda: std::clock() la' e' tempo de CPU de
// verdade (POSIX), e continua sendo o instrumento correto daquele lado.
//
// CELULA DE CALIBRACAO DO INSTRUMENTO, nas DUAS metades (GODS_LAWS.md
// L-36 aplicada ao INSTRUMENTO, nao so ao portao; L-17 do projeto, "o
// gemeo exato"): a janela girando tem de medir razao ALTA (>= 800 por
// mil); a mesma janela dormindo tem de medir razao BAIXA (<= 250 por
// mil). A JANELA (k_calibration_window_ms, abaixo) NAO e' mais a mesma
// nas duas metades desde 15/09/2026 - ver o bloco "JANELA DE
// CALIBRACAO ALARGADA" logo a seguir para o porque, e o gemeo desta
// nota em tests/container/loop_hidden_test.cpp para o porque do outro
// lado ficar em 60 ms. Calibracao reprovada NUNCA reprova o produto -
// o gate de cpu_ratio_permille por modo, abaixo, so conta quando a
// calibracao passou.
//
// PROVA DE QUE A CELULA PEGA A FAMILIA EXATA DO DEFEITO (L-27: cópia
// fora da arvore, nunca in-place): /var/tmp/builds/claude-1000/glintfx-
// calib-proof/ (calib_check.cpp/calib_check_sabotaged.cpp), compilados
// e executados em container (glintfx-devbuild:latest, GODS_LAWS.md
// L-09), 15/09/2026 - a MESMA logica de medicao, com a UNICA linha que
// le CPU trocada por relogio de parede. Real: instrumento_giro_
// permille=997 (>=800 OK), instrumento_sono_permille=0 (<=250 OK),
// exit=0. Sabotado: instrumento_giro_permille=1000 (>=800 OK, porque
// girar tambem teria razao alta num relogio de parede - o giro NAO
// distingue os dois instrumentos), instrumento_sono_permille=1000
// (<=250 FALHOU - exatamente a assinatura ~1000 da execucao real),
// exit=1. A celula de sono e' a que morde; a de giro fica como o
// segundo controle (positivo) que a L-40 exige. (Esta prova rodou com
// janela de 60 ms, em Linux - o instrumento provado aqui e' a LOGICA da
// celula, nao o numero da janela de nenhuma metade especifica.)
// ===================================================================
//
// ===================================================================
// JANELA DE CALIBRACAO ALARGADA, 15/09/2026 (DECISOES_AUTONOMAS.md
// D-091508, D15.1/D15.2 - GODS_LAWS.md global L-43, "conserta-se o
// FURO DO CRITERIO antes da fase seguinte, nunca o criterio"): a
// execucao de servidor 34969449304 (ramo prova-teto-e-regua) reprovou
// esta celula, so nesta metade:
//   instrumento_giro_permille=777 e 781  (criterio >= 800)  FALHOU
//   instrumento_sono_permille=259        (criterio <= 250)  FALHOU
// A CAUSA, CALCULADA: GetProcessTimes() no Windows conta tempo de
// processador em quanta de relogio de sistema de ~15,6 ms (o tique de
// 64 Hz padrao do HAL, sem timeBeginPeriod(1) - a mesma granularidade
// que a documentacao da Microsoft cita para este relogio). Os numeros
// denunciam exatamente isso: sono mediu 259/1000 de 60 ms = 15,54 ms =
// 1,00 quantum quase exato; giro faltou 13,4 ms dos 60 = 0,86 quantum.
// Uma janela de 60 ms nao resolve melhor que +/-26% neste relogio - os
// limiares 800/250 foram fixados na maquina do autor, onde o grao e'
// fino, e nao sobrevivem no executor do servidor.
//
// O CONSERTO E' A JANELA, NUNCA O LIMIAR (D15.1): alargar 800/250 ate
// caber calaria justamente ESTE executor - o de grao grosso -, e
// deixaria a regua incapaz de distinguir girar de dormir em qualquer
// maquina parecida, que e' o defeito que a calibracao inteira existe
// para impedir. k_calibration_window_ms sobe de 60 para 500 ms. Erro
// relativo do quantum nessa janela: 15,6 / 500 ~= 3,1% (contra 26% em
// 60 ms) - giro passa a medir por volta de 969/1000 (bem acima do piso
// 800) e sono por volta de 31/1000 (bem abaixo do teto 250), as duas
// com folga de mais de 8x em relacao ao limiar mais proximo. D15.2:
// esta janela se dimensiona pelo grao do relogio do PIOR executor
// conhecido, nunca pelo da maquina de quem escreve o teste - e' a
// mesma regra que toda celula de calibracao futura deste projeto
// segue a partir de hoje.
//
// CUSTO: duas chamadas de measure_cpu_ratio_permille_for() por
// execucao deste arquivo (giro + sono, uma vez cada, nunca por modo -
// ver k_calibration_checks abaixo), 500 ms cada uma no lugar de 60 ms -
// cerca de 880 ms a mais por execucao da suite neste arquivo. O gemeo
// Linux (tests/container/loop_hidden_test.cpp) NAO muda: o mesmo
// instrumento, na mesma janela de 60 ms, ja mede 997/0 por mil neste
// mesmo dia (bloco de prova acima) - std::clock() no POSIX tem grao
// muito mais fino que o relogio do Windows, e alargar sem uma reprovacao
// real so pagaria custo sem comprar seguranca. As duas metades ficarem
// com janelas diferentes e' a divergencia documentada que a L-17 do
// projeto exige nomear, nao um descuido.
// ===================================================================
//
// ASCII ONLY IN EVERY LITERAL: no /utf-8 flag is passed anywhere in
// cmake/, and /WX would turn C4819 into an error.

namespace {

using gl_enum = unsigned int;
using gl_float = float;

constexpr gl_enum k_gl_color_buffer_bit = 0x00004000;

using gl_clear_color_fn = void (*)(gl_float, gl_float, gl_float, gl_float);
using gl_clear_fn = void (*)(gl_enum);

gl_clear_color_fn g_clear_color = nullptr;
gl_clear_fn g_clear = nullptr;

constexpr int k_checks_per_mode = 8;
// TROCA DE INSTRUMENTO DE CPU (15/09/2026, este arquivo's own header
// comment): duas assercoes novas de calibracao, avaliadas UMA vez
// (nunca por modo) antes de qualquer numero de produto.
constexpr int k_calibration_checks = 2;
constexpr int k_planned_assertions = 2 * k_checks_per_mode + k_calibration_checks;
// JANELA DE CALIBRACAO (este arquivo's own header comment, "JANELA DE
// CALIBRACAO ALARGADA"): 500 ms, nao 60 - dimensionada pelo quantum de
// ~15,6 ms do relogio de processador do Windows (GetProcessTimes()),
// para que o erro relativo do quantum (~3,1% nesta janela) fique bem
// abaixo da folga dos limiares 800/250 abaixo. So esta metade muda; o
// gemeo Linux fica em 60 ms, com a razao documentada no mesmo bloco.
constexpr std::int64_t k_calibration_window_ms = 500;

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

// process_cpu_ns() - GetProcessTimes(), o substituto que a propria
// Microsoft nomeia para std::clock() (este arquivo's own header
// comment, "TROCA DE INSTRUMENTO DE CPU"). Soma kernel+user, FILETIME
// em unidades de 100 ns convertidas para nanosegundos. Falha de
// GetProcessTimes() (nunca observada, mas GetProcessTimes() e' capaz de
// falhar por contrato) devolve 0 - a mesma degradacao "nao-fatal, valor
// nao confiavel" que a celula de calibracao abaixo existe para pegar,
// nunca um crash.
[[nodiscard]] std::int64_t process_cpu_ns() noexcept {
    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (::GetProcessTimes(::GetCurrentProcess(), &creation_time, &exit_time, &kernel_time,
                          &user_time) == 0) {
        return 0;
    }
    const auto to_ns = [](const FILETIME &ft) noexcept -> std::int64_t {
        const std::uint64_t ticks_100ns =
            (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return static_cast<std::int64_t>(ticks_100ns) * 100;
    };
    return to_ns(kernel_time) + to_ns(user_time);
}

// A CELULA DE CALIBRACAO (este arquivo's own header comment): mede a
// razao de CPU por mil que process_cpu_ns() reporta para `target_ms`
// girando (`spin == true`, consumindo CPU de verdade via gltfx_now())
// ou dormindo (`spin == false`, via Sleep()). MESMA formula da medicao
// de producao abaixo (wall_ns > 0 ? (cpu_ns * 1000) / wall_ns : 1000),
// deliberadamente - a calibracao tem de exercitar o MESMO caminho que
// ela prova, nunca um atalho que poderia divergir dele.
[[nodiscard]] std::int64_t measure_cpu_ratio_permille_for(std::int64_t target_ms,
                                                          bool spin) noexcept {
    const std::int64_t cpu_start_ns = process_cpu_ns();
    const glintfx::gltfx_time_point wall_start = glintfx::gltfx_now();
    if (spin) {
        glintfx::gltfx_time_point now = wall_start;
        while (glintfx::gltfx_duration_between(wall_start, now).nanoseconds <
               target_ms * 1000000LL) {
            now = glintfx::gltfx_now();
        }
    } else {
        ::Sleep(static_cast<DWORD>(target_ms));
    }
    const glintfx::gltfx_time_point wall_end = glintfx::gltfx_now();
    const std::int64_t cpu_end_ns = process_cpu_ns();
    const std::int64_t wall_ns = glintfx::gltfx_duration_between(wall_start, wall_end).nanoseconds;
    const std::int64_t cpu_ns = cpu_end_ns - cpu_start_ns;
    return wall_ns > 0 ? (cpu_ns * 1000) / wall_ns : 1000;
}

// The scope line has to survive a GLINTFX_CHECK reproval too: the
// harness throws case_check_failed to unwind the current case the
// instant a check fails (harness/check.hpp's own "CASE-FATAL"), and a
// print sitting at the bottom of the case body would never run - the
// exact gap win32_iconic_present_test.cpp's own swap_tolerated_
// downgrades_reporter was written for, measured on a real run.
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

struct mode_result {
    bool ran = false;
    bool hidden_detected = false;
    bool suspended_seen = false;
    bool restored = false;
    int rendered_while_hidden = 0;
    long long cpu_ratio_permille = 0;
    long long visible_wall_ms = 0;
    // ACHADO 6 (14/09/2026): true so quando as 5 apresentacoes da fase
    // visivel tiveram sucesso - ver o comentario junto de
    // hidden_detected_all, mais abaixo, para o porque.
    bool visible_clean = false;
};

} // namespace

GLINTFX_TEST(loop_hidden_vsync_on_and_off) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const scope_reporter reporter;

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    GLINTFX_CHECK(!display_opened.has_error());
    glintfx::gltfx_display display = std::move(display_opened.value());

    // CALIBRACAO DO INSTRUMENTO, ANTES de qualquer numero de producao
    // (este arquivo's own header comment, "TROCA DE INSTRUMENTO DE
    // CPU"): se qualquer uma reprovar, o defeito e' no INSTRUMENTO -
    // nunca no laco - e por isso `calibration_ok` abaixo passa a gatear
    // a assercao de producao `cpu_ratio_permille` de cada modo, mais
    // longe neste arquivo.
    const std::int64_t calib_giro_permille =
        measure_cpu_ratio_permille_for(k_calibration_window_ms, true);
    const std::int64_t calib_sono_permille =
        measure_cpu_ratio_permille_for(k_calibration_window_ms, false);
    const bool calibration_ok = calib_giro_permille >= 800 && calib_sono_permille <= 250;
    check("calibracao", "instrumento_giro_permille", calib_giro_permille >= 800,
          ">= 800 (500ms girando)", to_text(calib_giro_permille));
    check("calibracao", "instrumento_sono_permille", calib_sono_permille <= 250,
          "<= 250 (500ms dormindo)", to_text(calib_sono_permille));

    bool any_swap_succeeded = false;
    mode_result results[2];
    const char *mode_names[2] = {"on", "off"};
    const std::int64_t mode_values[2] = {1, 0};

    for (int mode_index = 0; mode_index < 2; ++mode_index) {
        const char *mode = mode_names[mode_index];
        mode_result &result = results[mode_index];
        bool untolerated_swap_failure = false;

        // ACHADO 4 (13/09/2026, medido no irmao Linux): CADA modo abre
        // a PROPRIA janela, o proprio contexto grafico e o proprio
        // laco, e os destroi ao terminar. Antes os dois modos
        // compartilhavam uma janela so; no Wayland o segundo modo
        // herdava ela ainda minimizada, e os dois lados passam a ter a
        // MESMA sequencia por construcao, nunca uma cada.
        const glintfx::gltfx_window_desc desc{
            .title = "janela oculta do laco principal",
            .application_id = "org.glintfx.loop_hidden_test",
            .logical_size = {.width = 640, .height = 480},
        };
        glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
            glintfx::gltfx_window::open(display, desc);
        GLINTFX_CHECK(!window_opened.has_error());
        glintfx::gltfx_window window = std::move(window_opened.value());
        glintfx::window_impl *w_impl = glintfx::window_internal_access::get(window);
        GLINTFX_CHECK(w_impl != nullptr);
        const HWND hwnd = w_impl->adapter.native_handle();
        GLINTFX_CHECK(hwnd != nullptr);

        const glintfx::gltfx_gl_context_desc empty_desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
            glintfx::gltfx_gl_context::open(window, empty_desc);
        GLINTFX_CHECK(!context_opened.has_error());
        glintfx::gltfx_gl_context context = std::move(context_opened.value());
        GLINTFX_CHECK(!context.make_current().has_error());

        void *clear_color_addr = context.proc_address("glClearColor");
        void *clear_addr = context.proc_address("glClear");
        GLINTFX_CHECK(clear_color_addr != nullptr);
        GLINTFX_CHECK(clear_addr != nullptr);
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
        // dlsym-style function-to-object-pointer cast every GL loader relies on.
        g_clear_color = reinterpret_cast<gl_clear_color_fn>(clear_color_addr);
        g_clear = reinterpret_cast<gl_clear_fn>(clear_addr);
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above

        const glintfx::gltfx_gpu_info gpu = context.gpu();

        glintfx::gltfx_rslt<glintfx::gltfx_loop> loop_opened =
            glintfx::gltfx_loop::open(display, window, context);
        GLINTFX_CHECK(!loop_opened.has_error());
        glintfx::gltfx_loop loop = std::move(loop_opened.value());

        // The window is created WITHOUT WS_VISIBLE (D-W5-8/D-W5-13,
        // win32/window_adapter.hpp's own header comment) - mapped here
        // only because this case needs a real, visible top-level window to
        // minimize, never because open() itself ever calls ShowWindow.
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);

        GLINTFX_CHECK(!context
                           .set_option({.id = glintfx::gltfx_gfx_option::vsync,
                                        .value = mode_values[mode_index]})
                           .has_error());

        // --- visible phase: five ticks, at least one real frame ------
        int visible_presented = 0;
        const glintfx::gltfx_time_point visible_start = glintfx::gltfx_now();
        for (int i = 0; i < 5; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            GLINTFX_CHECK(!ticked.has_error());
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
            glintfx::gltfx_duration_between(visible_start, visible_end).nanoseconds / 1000000;

        check(mode, "visible_presented", visible_presented >= 1, ">= 1 em 5 tiques visiveis",
              to_text(visible_presented));

        // ACHADO 6 (14/09/2026): ver o comentario junto de
        // hidden_detected_all, mais abaixo, para o porque deste campo.
        result.visible_clean = (visible_presented == 5);

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
                     "presents_visiveis_bem_sucedidos=%d de 5, fase_visivel_limpa=%d\n",
                     mode, suspended_before_hiding ? 1 : 0, visible_presented,
                     result.visible_clean ? 1 : 0);

        // --- hide it ------------------------------------------------
        // pump_events() after EVERY ShowWindow (win32_iconic_present_
        // test.cpp's own comment on this exact pair of lines): Windows
        // POSTS messages around an iconic transition that only reach
        // this window's own queue through a pump, and step() is where
        // this loop pumps.
        ::ShowWindow(hwnd, SW_MINIMIZE);
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
            GLINTFX_CHECK(!settled.has_error());
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
        std::fprintf(stdout, "loop_hidden_test: [vsync=%s] IsIconic=%d\n", mode,
                     ::IsIconic(hwnd) != 0 ? 1 : 0);

        // --- ten hidden ticks, measured ------------------------------
        bool p_a_holds = true;
        bool p_b_holds = true;
        // "O laco recusou desenhar ao menos uma vez neste trecho" - ver
        // o bloco ACHADO 3 mais abaixo para por que este, e nao o
        // retorno de present(), e' o observavel que P8 descreve.
        bool hidden_detected_in_stretch = false;
        const std::int64_t cpu_start_ns = process_cpu_ns();
        const glintfx::gltfx_time_point hidden_start = glintfx::gltfx_now();
        for (int i = 0; i < 10; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            GLINTFX_CHECK(!ticked.has_error());
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
        const std::int64_t cpu_end_ns = process_cpu_ns();
        const std::int64_t wall_ns =
            glintfx::gltfx_duration_between(hidden_start, hidden_end).nanoseconds;
        const std::int64_t cpu_ns = cpu_end_ns - cpu_start_ns;
        result.cpu_ratio_permille = wall_ns > 0 ? (cpu_ns * 1000) / wall_ns : 1000;

        std::fprintf(stdout, "loop_hidden_test: [vsync=%s] wall_ns=%lld cpu_ns=%lld\n", mode,
                     static_cast<long long>(wall_ns), static_cast<long long>(cpu_ns));

        // Gateado pela calibracao (este arquivo's own header comment,
        // "TROCA DE INSTRUMENTO DE CPU"): instrumento suspeito nunca
        // reprova o produto - a calibracao ja reprovou por conta
        // propria, mais acima, se foi o caso.
        check(mode, "cpu_ratio_permille", !calibration_ok || result.cpu_ratio_permille <= 250,
              "<= 250 (25% de CPU), OU instrumento suspeito (calibracao FALHOU)",
              to_text(result.cpu_ratio_permille));
        check(mode, "wall_ns", wall_ns >= 500000000, ">= 500 ms em 10 tiques ocultos",
              to_text(wall_ns / 1000000) + " ms");
        check(mode, "rendered_while_hidden",
              !result.hidden_detected || result.rendered_while_hidden == 0,
              "== 0 quando hidden_detected == 1",
              to_text(result.rendered_while_hidden) +
                  " (hidden_detected=" + to_text(result.hidden_detected ? 1 : 0) + ")");
        check(mode, "P_a_e_P_b_no_trecho_oculto", p_a_holds && p_b_holds,
              "presented => should_render, e !should_render => skipped_hidden",
              std::string(p_a_holds ? "P_a ok" : "P_a violada") + ", " +
                  (p_b_holds ? "P_b ok" : "P_b violada"));
        // UNCONDITIONAL on this system (this file's own header comment):
        // WM_SIZE/SIZE_MINIMIZED is always delivered, there is no
        // protocol version to negotiate.
        check(mode, "suspended_while_minimized", result.suspended_seen,
              "ligado apos SW_MINIMIZE, sem condicao", to_text(result.suspended_seen ? 1 : 0));

        // --- restore it ----------------------------------------------
        ::ShowWindow(hwnd, SW_RESTORE);
        for (int i = 0; i < 10; ++i) {
            GLINTFX_CHECK(!loop.step().has_error());
            if (!window.state(glintfx::gltfx_window_state_bit::suspended)) {
                result.restored = true;
                break;
            }
        }

        int recovery_ticks = 0;
        bool recovered = false;
        for (int i = 0; i < 10 && !recovered; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            GLINTFX_CHECK(!ticked.has_error());
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
        // UNCONDITIONAL on this system: SW_RESTORE always restores, so
        // `restored` is part of the assertion here, never a condition
        // on it (the Linux half is the one that has to condition).
        check(mode, "recuperacao_em_ate_10_tiques", result.restored && recovered,
              "presented em <= 10 tiques, com restored == 1",
              std::string(recovered ? "recuperou" : "nao recuperou") + " em " +
                  to_text(recovery_ticks) +
                  " tique(s), restored=" + to_text(result.restored ? 1 : 0));
        check(mode, "swap_buffers_fora_da_tolerancia", !untolerated_swap_failure,
              "nenhuma falha de troca de quadro fora dos tres fatores",
              untolerated_swap_failure ? std::string("houve") : std::string("nenhuma"));

        result.ran = true;
    }

    // --- the aggregate MEASURED keys (sec. S4.5) --------------------
    // Same conservative folding the Linux half uses, for the same
    // reason: a mode that saw nothing must not be hidden by one that
    // did. `xdg_wm_base_version` has no Windows counterpart and is the
    // one key of this pair that lives on one side only (tests/
    // measured_exceptions.txt carries its line).
    // ACHADO 6 (14/09/2026, achado I5 da auditoria independente, dossie
    // /var/tmp/glintfx-plan/auditoria-onda-w7.md): a chave agregada
    // `hidden_detected` cruzava os dois modos sem checar se a deteccao,
    // em CADA um, era de fato atribuivel ao minimizar. Medido pelo
    // auditor (logs/h3_loop_hidden.log, irmao Linux): no modo vsync=on,
    // so 1 de 5 apresentacoes da fase visivel teve sucesso ANTES de
    // minimizar - a sonda ja relatava "nao desenhar" numa janela ainda
    // mapeada e ativa, por um defeito de ambiente ja registrado
    // (TODO.md, INBOX, SONDA-OCULTA-PUNE-JANELA-VISIVEL, decisao de
    // produto do lider, fora de escopo aqui). Nesse modo, ver
    // hidden_detected=1 DEPOIS de minimizar nao prova que foi o
    // minimizar que causou a deteccao - pode ser a MESMA leitura falsa
    // que ja vinha de antes. So o modo vsync=off (5 de 5 na fase
    // visivel) prova isso de verdade. ANTES: `hidden_detected_all`
    // exigia so `hidden_detected` nos dois modos. DEPOIS: exige TAMBEM
    // `visible_clean` (fase visivel 100% bem-sucedida) em cada modo -
    // um modo com fase visivel suja nao pode assinar P8.
    // POR QUE ESSE E O OBSERVAVEL CERTO, pelo texto da promessa:
    // loop.hpp:365-368 (P8) promete o comportamento de UMA JANELA
    // OCULTA (minimizada); uma deteccao que pode ter comecado antes do
    // minimizar nao fala sobre o caso que P8 descreve, fala sobre outro
    // caso (janela mapeada mas com o retorno de quadro atrasado) que
    // este teste nao cobre. Nenhuma das oito assercoes por modo muda -
    // isso e so a chave agregada MEASURED ficando honesta sobre o que
    // ela cruza.
    const bool hidden_detected_all = results[0].hidden_detected && results[0].visible_clean &&
                                     results[1].hidden_detected && results[1].visible_clean;
    const bool suspended_all = results[0].suspended_seen && results[1].suspended_seen;
    const bool restored_all = results[0].restored && results[1].restored;
    const long long worst_cpu_ratio = results[0].cpu_ratio_permille > results[1].cpu_ratio_permille
                                          ? results[0].cpu_ratio_permille
                                          : results[1].cpu_ratio_permille;
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

    GLINTFX_CHECK(results[0].ran && results[1].ran);
    GLINTFX_CHECK(g_assertions_evaluated == k_planned_assertions);
    GLINTFX_CHECK(g_assertions_failed == 0);
}

#endif // defined(_WIN32)
