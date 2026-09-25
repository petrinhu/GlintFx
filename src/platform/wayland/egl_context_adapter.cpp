// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_context_adapter.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "platform/nul_terminated_name.hpp"

// WL_EGL_PLATFORM before <EGL/egl.h> (tests/container/egl_probe_
// smoke.cpp's own header comment, this fatia's own briefing "leia-a
// inteira"): without it, <EGL/eglplatform.h>'s own #elif chain types
// EGLNativeWindowType as a bare integer instead of `struct wl_egl_
// window *`, forcing an integer-cast this file never needs.
#define WL_EGL_PLATFORM
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <poll.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gl_memory_facts.hpp"
#include "platform/gl/gl_surface_size_policy.hpp"
#include "platform/gl/gl_version_policy.hpp"
#include "platform/gl/gpu_kind_report.hpp"
#include "platform/gl/gpu_kind_seam.hpp"
#include "platform/gl/memory_separation_kind.hpp"
#include "platform/wayland/bounded_output_wait.hpp"
#include "platform/wayland/connection_failure.hpp"
#include "platform/wayland/drm_device_facts.hpp"
#include "platform/wayland/drm_gpu_kind.hpp"
#include "platform/wayland/egl_device_enumeration.hpp"
#include "platform/wayland/egl_incoming_poll_step.hpp"
#include "platform/wayland/incoming_poll_outcome.hpp"
#include "platform/wayland/incoming_poll_reaction.hpp"
#include "platform/wayland/window_adapter.hpp"
#include "platform/window/window_state.hpp"

// egl_context_adapter.cpp - W-EGL (docs/plano-w6b-placa-e-laco.md
// fatia 3, D-W6b-1/4/5/6/7/17/18, GODS_LAWS.md L-04/L-07/L-17/L-19/
// L-31): the real EGL/libwayland-egl calls egl_context_adapter.hpp's
// own header comment names, one atom per named step - the mechanics
// this file exercises were MEASURED, not guessed: G-1's own sonda
// (tests/container/egl_probe_smoke.cpp, fatia 1) already proved this
// exact sequence (eglGetPlatformDisplay -> eglInitialize -> eglBindAPI
// -> eglChooseConfig(RGBA8+stencil8) -> wl_egl_window_create -> egl
// CreateWindowSurface -> eglCreateContext(3.3 core)) runs to completion
// against the SAME `kwin_wayland --virtual` compositor this project's
// CI uses (run 34028525955's own container job).
//
// GL FUNCTIONS DECLARED BY HAND, <GL/gl.h> NOT LINKED (GODS_LAWS.md
// L-07, the SAME technique egl_probe_smoke.cpp/tests/win32_runner_
// probe_test.cpp already use, sourced against the SAME vendored
// registry src/render/gl_abi.hpp is measured against, third_party/
// khronos/gl.xml lines 176/1072-1074/1980-1981/6124): this adapter
// only ever needs glGetString/glGetIntegerv, resolved through
// eglGetProcAddress() the same way the public proc_address() below
// resolves anything else - never a second, hidden linkage path against
// -lGL.
namespace glintfx::platform {

namespace {

using gl_enum = unsigned int;
using gl_ubyte = unsigned char;
using gl_int = int;

constexpr gl_enum k_gl_renderer = 0x1F01;
constexpr gl_enum k_gl_major_version = 0x821B;
constexpr gl_enum k_gl_minor_version = 0x821C;
constexpr gl_enum k_gl_context_profile_mask = 0x9126;

using gl_get_string_fn = const gl_ubyte *(*)(gl_enum);
using gl_get_integerv_fn = void (*)(gl_enum, gl_int *);

// egl_context_adapter.hpp's own class comment names the callback
// listener's own address-taking requirement - the SAME shape wayland_
// window_adapter's own three listeners already document one file over.
constexpr wl_callback_listener k_frame_callback_listener{
    .done = &wayland_egl_context_adapter::frame_callback_done,
};

} // namespace

// CONT-WARMUP C-6, EMENDA (ordem do team-lead sobre o residual
// declarado da primeira rodada de C-6): poll_and_dispatch_with_budget()
// SAIU do namespace anônimo acima - antes disto, TU-local, sem linkage
// externo, um arquivo de teste em OUTRA translation unit não tinha
// como chamá-la, então só o átomo que ela consome
// (is_incoming_poll_connection_fatal(), incoming_poll_reaction.hpp)
// podia ser testado diretamente, nunca a fiação real em volta dele
// (o `switch` inteiro). Agora vive direto em `namespace glintfx::
// platform` (linkage externo comum, exatamente como qualquer método de
// classe deste arquivo), declarada em `src/platform/wayland/egl_
// incoming_poll_step.hpp` - um cabeçalho INTERNO (`src/`, nunca
// `include/glintfx/`, GODS_LAWS.md L-19 intacto) que só
// tests/incoming_poll_wiring_test.cpp e este `.cpp` incluem. Nada mais
// deste arquivo referenciava esta função por fora do namespace
// anônimo (só o call site dentro de swap_buffers(), mais abaixo, na
// MESMA translation unit - continua resolvendo por nome comum, sem
// precisar do cabeçalho novo).
//
// Local BUDGETED variant of wayland_display_adapter's own manpage-
// blessed prepare_read/flush/poll/read_events sequence (display_
// adapter.cpp, one directory over) - duplicated here rather than
// shared for the SEQUENCE as a whole, because this caller only ever
// has a raw wl_display* (wl_proxy_get_display() from the window's own
// wl_surface, egl_context_adapter.hpp's own class comment: this
// adapter never needs a separate wayland_display_adapter& reference,
// since gl_context_facade.cpp's own open() call only ever hands it the
// window). The mandatory prepare_read/cancel_read pairing (manpage
// ARMADILHA 2) is identical; the poll() TIMEOUT differs - budgeted
// here, always 0 in display_adapter.cpp's own non-blocking pump.
//
// The WRITE-WAIT ITSELF (POLLOUT while wl_display_flush() reports
// EAGAIN) is NOT duplicated, though: it is bounded_output_wait.hpp's
// own wait_for_writable_until(), the SAME atom display_adapter.cpp's
// own flush_with_retry() now calls one directory over (INBOX, drenagem
// 06/09/2026, GODS_LAWS.md L-17 - the two copies of this exact wait
// diverging is what let this file's OWN infinite wait, fixed once
// already by 72754af with an inline write_deadline, and its sibling in
// display_adapter.cpp go unnoticed for hours the first time around).
// context.hpp's own class comment promises swap_buffers() "NEVER
// blocks the process indefinitely" - the reason this wait carries a
// real budget at all.
//
// Returns false only when the wl_display connection itself is now
// unusable (a real protocol/socket failure) - "nothing arrived within
// budget" is NOT a failure, it is reported through frame_callback_
// sequence.hpp's own decide_after_wait() instead, read by the caller
// after this function returns.
//
// CONT-WARMUP C-5 (revisao adversarial C-4, /var/tmp/glintfx-plan/
// revisao-cont-warmup-C4.md, achado CRITICO-1/IMPORTANTE-1,
// GODS_LAWS.md L-17 "gemeo"): the read-side wait below used to decide
// with its OWN inline `poll_result <= 0 || (incoming.revents &
// POLLIN) == 0` - the exact composite condition display_adapter.cpp's
// own wait_for_incoming_data() had BEFORE 99b5138, and this file was
// never touched by that fix. Two bugs, both closed now: (1) POLLHUP/
// POLLERR without POLLIN, and POLLNVAL, were absorbed as "nothing to
// read, success" instead of the fatal connection failure poll(2)'s own
// contract calls for; (2) `poll_result <= 0` folded a REAL poll()
// error (-1, any errno, EINTR included) into the SAME branch as
// "budget merely exhausted" - a caller here would read that as
// skipped_hidden (an ordinary, silent degrade), never the connection
// failure it actually is. Both now route through classify_incoming_
// poll() (platform/wayland/incoming_poll_outcome.hpp), the SAME atom
// display_adapter.cpp's own wait_for_incoming_data() already uses.
// EINTR retries WITH WHATEVER TIME IS LEFT of `budget_ms` (its own
// `read_deadline`, computed the same way `write_deadline` above is) -
// unlike wait_for_incoming_data(), which simply reports "nothing yet"
// and lets the NEXT caller-driven pump retry: THIS function's own
// caller (swap_buffers()) is mid-frame-wait, budgeted for one specific
// frame, and a bare EINTR must not silently cost the whole remaining
// budget the way a single non-retried "nothing yet" would - the same
// "keep trying with whatever time is left" shape wait_for_writable_
// until() above already uses for its own EINTR (poll(2)'s own manpage:
// a signal arriving mid-wait is never a reason to declare the
// connection unusable).
//
// CONT-WARMUP C-6 (revisao adversarial C-5, /var/tmp/glintfx-plan/
// revisao-cont-warmup-c5.md, achado CRITICO-2): `case fatal:`/`case
// poll_call_failed:` below used to decide "is this a real connection
// failure?" with their OWN hand-written `return`s - nothing tested that
// decision directly, and the review's OWN mutant (m-fatal-swallowed,
// swapping those two returns to `return true`) survived every test that
// existed: the `ready_to_read` branch (a real `wl_display_read_events()`/
// `wl_display_dispatch_pending()` failure AFTER a genuine `POLLIN`) is
// what a real Wayland protocol error routes through - never via
// `fatal`/`poll_call_failed`, the SAME "cenario real vs. seam sintetico"
// split incoming_poll_outcome.hpp's own header comment already
// documents for display_adapter.cpp's side (measured four times: this
// kernel never delivers POLLHUP/POLLNVAL/a real poll() error without
// POLLIN alongside it).
//
// EGL-DEAD-DISPLAY-GUARD S4 (docs/plano-egl-dead-display-guard.md sec.
// 5, 24/09/2026): until S4, this ramo's ONLY proof against a real
// protocol error was `egl_protocol_error_smoke` (tests/container/),
// contingent on the compositor's error arriving inside a fixed 2-swap/
// 100ms budget - the M-1 measurement found that budget missed the real
// error 12 of 20 rounds through wire_relay (never this ramo being
// wrong: all 12 read no error at all in every attempt, a timing gap,
// not a routing bug). The DETERMINISTIC proof now lives in
// tests/incoming_poll_wiring_test.cpp's own poll_and_dispatch_with_
// budget_ready_to_read_with_real_protocol_error_returns_false - a real
// wl_display.error event (object 1, code 3) written to the socket
// BEFORE the call, so the real `::poll()` always finds genuine POLLIN,
// never dependent on a compositor or a clock. `egl_protocol_error_
// smoke` still exercises the SAME ramo end-to-end against a real
// compositor (now synchronized by `wl_display_roundtrip()`, S4,
// instead of racing a fixed budget), it just is no longer the ONLY
// place this ramo's real-error behavior is proven.
//
// PROVA POR SEAM, NAO POR CENARIO REAL, exactly like that header
// already says for the read-side atom itself: the ramos `fatal`/
// `poll_call_failed` below are proven by
// tests/incoming_poll_reaction_test.cpp's own unit + real-fabricated-fd
// cases, never by this project's own container fixtures. Both cases now
// share ONE call to is_incoming_poll_connection_fatal() (platform/
// wayland/incoming_poll_reaction.hpp) - the SAME atom display_adapter.
// cpp's own wait_for_incoming_data() (this same fatia) now calls too -
// so the only way to reintroduce m-fatal-swallowed is to edit THAT atom
// (caught by its own direct test) or to invert the call below (a
// residual, DECLARED limitation - see that test file's own header
// comment for the honest accounting of what real-kernel coverage
// exists and what does not).
[[nodiscard]] bool poll_and_dispatch_with_budget(wl_display *display, std::uint32_t budget_ms,
                                                 incoming_poll_syscall_fn poll_impl,
                                                 incoming_poll_reaction_fn reaction_impl) noexcept {
    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) == -1) {
            return false;
        }
    }

    const auto write_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
    while (wl_display_flush(display) == -1) {
        if (errno != EAGAIN) {
            wl_display_cancel_read(display);
            return false;
        }
        if (wait_for_writable_until(wl_display_get_fd(display), write_deadline) !=
            bounded_wait_outcome::ready) {
            // Budget exhausted (or the socket itself failed) and the
            // kernel send buffer is still not writable - the
            // compositor is not draining. Treated exactly like any
            // other now-unusable connection (the caller already
            // reports this through build_connection_failure()), never
            // an unbounded wait.
            wl_display_cancel_read(display);
            return false;
        }
    }

    const auto read_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
    for (;;) {
        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      read_deadline - std::chrono::steady_clock::now())
                                      .count();
        const int wait_ms = remaining_ms > 0 ? static_cast<int>(remaining_ms) : 0;
        pollfd incoming{.fd = wl_display_get_fd(display), .events = POLLIN, .revents = 0};
        const int poll_result = poll_impl(&incoming, 1, wait_ms);
        const incoming_poll_outcome outcome = classify_incoming_poll(poll_result, incoming.revents);
        switch (outcome) {
        case incoming_poll_outcome::fatal:
        case incoming_poll_outcome::poll_call_failed: {
            const bool errno_is_eintr =
                outcome == incoming_poll_outcome::poll_call_failed && errno == EINTR;
            if (errno_is_eintr && remaining_ms > 0) {
                continue; // retry with whatever time is left of the budget
            }
            wl_display_cancel_read(display);
            // CONT-WARMUP C-7 (revisao-cont-warmup-c6.md, achado
            // m-eintr-budget-bypass): este `case` costumava ter um
            // TERCEIRO caminho aqui - "EINTR com o orcamento ja
            // esgotado" respondia com um `return true;` HARDCODED,
            // nunca passando pelo atomo de decisao. Removido: os TRES
            // desfechos deste `case` (EINTR-exhausted, outro errno,
            // `fatal`/POLLNVAL) agora passam pela MESMA e UNICA chamada
            // abaixo.
            //
            // CONT-WARMUP C-8 (revisao-cont-warmup-c7.md S2.4/S2.5): a
            // frase que costumava estar aqui - "nao ha mais nenhum jeito
            // de reintroduzir o bug original" - era FALSA, medida: o
            // atalho hardcoded e o atomo de hoje concordam POR
            // COINCIDENCIA para o par (poll_call_failed,
            // errno_is_eintr==true) (`is_incoming_poll_connection_fatal`
            // devolve `!true == false`, `!false == true`, o MESMO valor
            // do atalho), entao reinserir o atalho e' um mutante
            // EQUIVALENTE ao atomo real - nenhum teste de caixa-preta
            // contra o atomo alcanca a diferenca. O que este arquivo
            // prova agora e' mais estreito e honesto: `reaction_impl` (o
            // atomo por tras de uma costura, egl_incoming_poll_step.hpp,
            // padrao `&is_incoming_poll_connection_fatal`) e' de fato
            // CONSULTADO aqui, nunca decidido por conta propria - prova
            // por tests/incoming_poll_wiring_test.cpp injetando um atomo
            // DIVERGENTE (que discorda do real de proposito) e exigindo
            // que o resultado siga o injetado. Residual DECLARADO que
            // continua aberto: um atalho hardcoded inserido DEPOIS desta
            // chamada, ou que IGNORE o valor de retorno de
            // `reaction_impl`, nao e' pego por nenhum teste existente.
            return !reaction_impl(outcome, errno_is_eintr);
        }
        case incoming_poll_outcome::nothing_yet:
            // Budget exhausted with nothing to read - the mandatory
            // other half of ARMADILHA 2's pairing, never a bare poll()
            // left hanging without its matching cancel_read().
            wl_display_cancel_read(display);
            return true;
        case incoming_poll_outcome::ready_to_read:
            if (wl_display_read_events(display) == -1) {
                return false;
            }
            return wl_display_dispatch_pending(display) != -1;
        }
        // Unreachable (the switch above is exhaustive over incoming_
        // poll_outcome's four enumerators) - GODS_LAWS.md L-22 style
        // safety net only, never meant to be hit; treated as "nothing
        // to read" rather than silently falling through with no
        // return.
        wl_display_cancel_read(display);
        return true;
    }
}

namespace {

// classify_current_gpu() - GL-GPU-KIND (docs/plano-w6b-fatias-5.md
// sec. 4.1; docs/plano-w6b-fatias-5b-revisao.md sec. 1.5/3, D-W6b-30/
// 33/38): the ONE place this adapter walks kernel -> via 1 (exclusion)
// -> via 2 (memory separation) -> unknown - a call into gpu_kind_seam.
// hpp's own two pure functions since the CONSERTO of 07/09/2026
// (revisão adversarial da fatia 5b; ver o header daquele arquivo para
// o achado e o RED literal). `enumeration_index` is found by matching
// THIS display's own egl_device_facts (by render node, falling back to
// primary node) against the survivor list enumerate_egl_devices()
// already deduplicated - the SAME list via 1 walks for "some OTHER
// entry is `shared`".
[[nodiscard]] std::pair<glintfx::gltfx_gpu_kind, std::uint32_t>
classify_current_gpu(void *egl_display, gl_get_integerv_fn get_integerv) noexcept {
    using glintfx::gltfx_gpu_kind;
    using glintfx::k_gltfx_gpu_index_unknown;
    using glintfx::platform::classify_by_memory_separation;
    using glintfx::platform::classify_drm_gpu;
    using glintfx::platform::enumerate_egl_devices;
    using glintfx::platform::resolve_kernel_and_exclusion;
    using glintfx::platform::resolve_memory_separation;

    const egl_device_facts current = query_egl_display_device(egl_display);

    // O PORTÃO DE CERTEZA (CONSERTO 07/09/2026): `current.queried`
    // vem direto de query_egl_display_device() - só é `true` quando a
    // extensão realmente respondeu. `!current.queried` significa "o
    // sistema não disse nada", NUNCA "não é software" - antes deste
    // conserto, esse caso caía na via 2 do mesmo jeito que um `queried
    // && !software` real, e é exatamente o achado da revisão.
    const bool definitely_not_software = current.queried && !current.software;

    gltfx_gpu_kind kernel_kind = gltfx_gpu_kind::unknown;
    if (current.queried) {
        if (current.software) {
            kernel_kind = gltfx_gpu_kind::software;
        } else {
            const std::string &node =
                !current.render_node.empty() ? current.render_node : current.primary_node;
            if (!node.empty()) {
                kernel_kind = classify_drm_gpu(read_drm_device_facts(node));
            }
        }
    }

    std::uint32_t enumeration_index = k_gltfx_gpu_index_unknown;
    // ACHADO, NAO CONSERTADO (WIN-DEBUG-CTORALLOC, mesma familia do
    // gemeo em gfx_open_only_fixation.cpp): `kinds`, aqui, precisa
    // ficar visivel ATE `resolve_kernel_and_exclusion()` mais abaixo,
    // fora do try{} que guarda reserve()/push_back() logo a seguir -
    // diferente dos outros achados desta varredura, mover a
    // declaracao para dentro do try nao fecha o risco por completo
    // aqui (o fallback do proprio catch tambem precisaria construir um
    // vetor vazio). Residual teorico, nao medido nesta maquina (sem
    // toolchain Windows) e de porte pequeno (a mesma classe de risco
    // que gpu_kind_exclusion.cpp's own header comment ja aceita para
    // um vetor de contagem de placas de video, nunca de tamanho vindo
    // de rede ou de compositor hostil).
    std::vector<gltfx_gpu_kind> kinds;
    const glintfx::gltfx_rslt<std::vector<egl_device_facts>> devices = enumerate_egl_devices();
    if (devices.has_value()) {
        const std::vector<egl_device_facts> &survivors = devices.value();

        // GODS_LAWS.md L-22: reserve()/push_back() abaixo podem lancar
        // std::bad_alloc dentro de uma funcao noexcept - a mesma
        // guarda que gfx_open_only_fixation.cpp's own resolve_gfx_
        // open_only_fixation() ja aplica. Esta funcao devolve um
        // std::pair simples, sem canal de erro (D-W6b-37's own shape,
        // igual ao gemeo Windows classify_current_gpu() de wgl_
        // context_adapter.cpp) - o desfecho honesto e degradar para o
        // MESMO estado que este bloco inteiro ja produz quando devices.
        // has_value() e falso (enumeracao indisponivel): `kinds` vazio,
        // `enumeration_index` continua k_gltfx_gpu_index_unknown, e a
        // via 1 (resolve_kernel_and_exclusion) segue seu proprio
        // caminho ja testado para essa forma.
        try {
            kinds.reserve(survivors.size());

            for (std::size_t i = 0; i < survivors.size(); ++i) {
                const egl_device_facts &survivor = survivors[i];
                const bool is_current =
                    current.queried &&
                    ((!current.render_node.empty() &&
                      current.render_node == survivor.render_node) ||
                     (current.render_node.empty() && !current.primary_node.empty() &&
                      current.primary_node == survivor.primary_node) ||
                     (current.render_node.empty() && current.primary_node.empty() &&
                      current.software == survivor.software && survivor.render_node.empty() &&
                      survivor.primary_node.empty()));
                if (is_current && enumeration_index == k_gltfx_gpu_index_unknown) {
                    enumeration_index = static_cast<std::uint32_t>(i);
                }

                gltfx_gpu_kind survivor_kind = gltfx_gpu_kind::unknown;
                if (survivor.software) {
                    survivor_kind = gltfx_gpu_kind::software;
                } else {
                    const std::string &node = !survivor.render_node.empty() ? survivor.render_node
                                                                            : survivor.primary_node;
                    if (!node.empty()) {
                        survivor_kind = classify_drm_gpu(read_drm_device_facts(node));
                    }
                }
                kinds.push_back(survivor_kind);
            }
        } catch (const std::bad_alloc &) {
            kinds.clear();
            enumeration_index = k_gltfx_gpu_index_unknown;
        }
    }

    const gltfx_gpu_kind after_via1 =
        resolve_kernel_and_exclusion(kernel_kind, enumeration_index, kinds);

    // A via 2 só é LIDA (uma chamada GL de verdade) quando ainda falta
    // resposta E há certeza de que não é software - nunca lida e
    // depois descartada (gpu_kind_seam.hpp's own header comment).
    std::optional<gltfx_gpu_kind> via2;
    if (after_via1 == gltfx_gpu_kind::unknown && definitely_not_software) {
        auto get_error = reinterpret_cast<unsigned int (*)()>(eglGetProcAddress("glGetError"));
        const gl_memory_facts memory_facts = read_gl_memory_facts(get_integerv, get_error);
        via2 = classify_by_memory_separation(memory_facts);
    }

    const gltfx_gpu_kind kind =
        resolve_memory_separation(after_via1, definitely_not_software, via2);

    // CORE-LOG CL-7 (TODO.md W5, GODS_LAWS.md L-04): the one real
    // emission this project's structured-log recebedor carries -
    // WAYLAND SIDE. The Win32 mirror (wgl_context_adapter.cpp's own
    // classify_current_gpu()) calls the SAME two functions with its
    // own already-computed inputs, right before its own equivalent
    // return - paridade by the compiler sharing one definition
    // (gpu_kind_report.cpp), never by two files read side by side.
    glintfx::platform::report_gpu_kind_resolved(glintfx::platform::resolve_gpu_kind_report(
        kernel_kind, enumeration_index, kinds, definitely_not_software, via2));

    return {kind, enumeration_index};
}

// steady_now_ns() - LOOP-RUN fatia 7 (docs/plano-w6b-fatias-6-8.md,
// D-W6b-46's own table row: "now vem de std::chrono::steady_clock,
// camada de plataforma, permitido"): present_would_skip() below and
// swap_buffers()'s own vsync=off branch both need a fresh clock
// reading to hand frame_callback_sequence::arm_pending()/pending_
// older_than() (this same fatia) - that atom itself stays clock-
// agnostic (GODS_LAWS.md L-17, "sem SO", frame_callback_sequence.hpp's
// own header comment), so THIS platform-layer file is where the one
// real read happens, in one place, rather than at each of the three
// call sites separately.
[[nodiscard]] std::int64_t steady_now_ns() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

wayland_egl_context_adapter::~wayland_egl_context_adapter() { close(); }

gltfx_rslt<void> wayland_egl_context_adapter::create_egl_display(wl_surface &surface) noexcept {
    // wl_surface IS a wl_proxy (every Wayland protocol object shares
    // that layout - the SAME cast every wayland-scanner-generated
    // request wrapper already performs internally); wl_proxy_get_
    // display() is how a caller that never opened the connection
    // itself (this adapter's own class comment) reaches the wl_display
    // it belongs to.
    wl_display *display = wl_proxy_get_display(reinterpret_cast<wl_proxy *>(&surface));
    if (display == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wl_display"));
    }

    void *egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, nullptr);
    if (egl_display == EGL_NO_DISPLAY) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_display"));
    }

    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    if (eglInitialize(egl_display, &egl_major, &egl_minor) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_display"));
    }

    // Per-THREAD state read by the NEXT eglCreateContext() call (this
    // fatia's own busca, docs/plano-w6b-placa-e-laco.md sec. 0) - has
    // to happen before create_context() below, never after.
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
        eglTerminate(egl_display);
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_bind_api"));
    }

    m_egl_display = egl_display;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void>
wayland_egl_context_adapter::choose_config(std::span<const gltfx_gfx_option_entry> options,
                                           void *&out_config) noexcept {
    std::int64_t msaa_samples = 0;
    std::int64_t srgb_framebuffer = 0;
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::msaa_samples) {
            msaa_samples = entry.value;
        } else if (entry.id == gltfx_gfx_option::srgb_framebuffer) {
            srgb_framebuffer = entry.value;
        }
    }

    // RGBA8+stencil8 always (D-W6b-4); EGL_SAMPLES/EGL_GL_COLORSPACE_
    // KHR appended only when requested - a fixed-size array, never a
    // heap allocation, this fatia's own open()-time path can afford
    // (it runs once per context, never per frame).
    auto try_choose = [this](std::int64_t samples, bool srgb, void *&config_out) noexcept -> bool {
        EGLint attribs[16] = {
            EGL_SURFACE_TYPE,
            EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE,
            EGL_OPENGL_BIT,
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8,
            EGL_STENCIL_SIZE,
            8,
            EGL_NONE,
            EGL_NONE,
        };
        std::size_t next = 12;
        if (samples > 0) {
            attribs[next++] = EGL_SAMPLES;
            attribs[next++] = static_cast<EGLint>(samples);
        }
        if (srgb) {
            attribs[next++] = EGL_GL_COLORSPACE_KHR;
            attribs[next++] = EGL_GL_COLORSPACE_SRGB_KHR;
        }
        attribs[next] = EGL_NONE;

        EGLConfig config = nullptr;
        EGLint num_configs = 0;
        const bool ok =
            eglChooseConfig(m_egl_display, attribs, &config, 1, &num_configs) == EGL_TRUE &&
            num_configs > 0;
        if (ok) {
            config_out = config;
        }
        return ok;
    };

    void *full_config = nullptr;
    if (try_choose(msaa_samples, srgb_framebuffer != 0, full_config)) {
        out_config = full_config;
        m_msaa_supported = true;
        m_srgb_supported = true;
        return gltfx_rslt<void>::ok();
    }

    // D-W6b-17: never degrade in silence - isolate WHICH option this
    // driver could not honor before refusing, one attempt at a time,
    // rather than blaming the first one requested.
    void *without_msaa = nullptr;
    if (msaa_samples > 0 && try_choose(0, srgb_framebuffer != 0, without_msaa)) {
        m_msaa_supported = false;
        m_srgb_supported = true;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("msaa_samples"));
    }

    void *without_srgb = nullptr;
    if (srgb_framebuffer != 0 && try_choose(msaa_samples, false, without_srgb)) {
        m_msaa_supported = true;
        m_srgb_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("srgb_framebuffer"));
    }

    if (msaa_samples > 0) {
        m_msaa_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("msaa_samples"));
    }
    if (srgb_framebuffer != 0) {
        m_srgb_supported = false;
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("srgb_framebuffer"));
    }
    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_config"));
}

gltfx_rslt<void>
wayland_egl_context_adapter::create_egl_window(wl_surface &surface, void *config,
                                               std::uint32_t pixel_width,
                                               std::uint32_t pixel_height) noexcept {
    // In PIXELS, never logical_size() (D-W6a-17's own distinction,
    // this file's own header comment) - the framebuffer a consumer
    // paints into is the window's own pixel_size(), read by open()
    // before this atom is ever called.
    wl_egl_window *egl_window = wl_egl_window_create(&surface, static_cast<int>(pixel_width),
                                                     static_cast<int>(pixel_height));
    if (egl_window == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wl_egl_window"));
    }
    m_egl_window = egl_window;
    m_buffer_width = pixel_width;
    m_buffer_height = pixel_height;

    // The EGLSurface a wl_egl_window is FOR is created right alongside
    // it, never as a separate top-level step (egl_probe_smoke.cpp's
    // own header comment: the two are one indivisible mechanical step,
    // both needing this SAME `config`).
    EGLSurface egl_surface = eglCreateWindowSurface(m_egl_display, config, m_egl_window, nullptr);
    if (egl_surface == EGL_NO_SURFACE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_surface"));
    }
    m_egl_surface = egl_surface;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_egl_context_adapter::create_context(void *config) noexcept {
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION,
        3,
        EGL_CONTEXT_MINOR_VERSION,
        3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        static_cast<EGLint>(k_gl_context_core_profile_bit),
        EGL_NONE,
    };
    EGLContext egl_context =
        eglCreateContext(m_egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (egl_context == EGL_NO_CONTEXT) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_context"));
    }
    m_egl_context = egl_context;

    if (eglMakeCurrent(m_egl_display, m_egl_surface, m_egl_surface, m_egl_context) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_make_current"));
    }

    // D-W6b-27 (docs/plano-w6b-fatias-5.md, F1 of that plan's own §0):
    // the Mesa Wayland platform starts a NEW EGL context at swap
    // interval 1 - in that mode, eglSwapBuffers() installs its own
    // wl_surface.frame callback and BLOCKS waiting for it (emersion.fr/
    // blog/2018/wayland-rendering-loop, Neil Roberts's own patch), a
    // wait with NO budget at all, sitting UNDERNEATH frame_callback_
    // sequence's own 100ms-budgeted wait above swap_buffers() below.
    // With a hidden window, this used to hang the whole process before
    // this adapter's own budget ever got a chance to say "skipped_
    // hidden" - the exact regression this fatia's own §3.2 case T
    // proves fixed by timing 60 swaps with `vsync=off`. Interval 0
    // here, ONCE, right after the first make_current(): from now on
    // the ONLY pacer on this Wayland side is frame_callback_sequence,
    // driven explicitly by swap_buffers() below - `vsync=off` becomes
    // what it always promised to be (D-W6b-18: "o mais rapido que o
    // driver deixa"), never a disguised interval-1 wait. A driver that
    // refuses this call (EGL_FALSE) is not a failure worth reporting:
    // the EGL 1.5 spec (sec. 3.10.3) allows an implementation to ignore
    // eglSwapInterval() outright, and this adapter's own budgeted wait
    // is the only pacer either way once vsync=on asks for one.
    //
    // INBOX (drenagem 06/09/2026): the return used to be discarded
    // outright. That left BOTH eglSwapBuffers() call sites in swap_
    // buffers() below (vsync=off's own early return, and the branch
    // after the budgeted frame-callback wait) a CONDITIONED wait,
    // never counted as such: if this exact call is the one a driver
    // ignores, eglSwapBuffers() falls back to whatever interval the
    // Mesa Wayland platform starts a context at (interval 1, two
    // paragraphs up) - which blocks on the SAME wl_surface.frame
    // callback this adapter itself already manages, but with NO budget
    // at all, reachable even on the vsync=off path this project
    // promises never waits on that callback (D-W6b-18). m_swap_
    // interval_honored below is the measured fact a caller (and tests/
    // wait_points.txt, which classifies both eglSwapBuffers sites as
    // `sem-teto-declarado` rather than silently `teto-nosso`) now has
    // to tell the two cases apart. Closing this gap for real needs a
    // driver-independent pacer that never leans on eglSwapInterval() at
    // all - out of this fatia's own scope (docs/plano-w6b-fatias-6-8.md
    // sec. 2, D-W6b-58's own "o que a fatia 7 mede antes de fechar");
    // measuring and declaring the fact, rather than assuming it away
    // in silence, is what is in scope here.
    m_swap_interval_honored = eglSwapInterval(m_egl_display, 0) == EGL_TRUE;

    // Resolved the SAME way the public proc_address() below resolves
    // anything else - this atom is a caller of that exact mechanism,
    // one layer below any public handle (egl_probe_smoke.cpp's own
    // header comment names this same technique).
    auto get_string = reinterpret_cast<gl_get_string_fn>(eglGetProcAddress("glGetString"));
    auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(eglGetProcAddress("glGetIntegerv"));
    if (get_string == nullptr || get_integerv == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("gl_proc_address"));
    }

    gl_int major = 0;
    gl_int minor = 0;
    gl_int profile_mask = 0;
    get_integerv(k_gl_major_version, &major);
    get_integerv(k_gl_minor_version, &minor);
    get_integerv(k_gl_context_profile_mask, &profile_mask);

    if (const gltfx_rslt<void> version_ok = validate_gl_context_version(
            static_cast<std::uint32_t>(major), static_cast<std::uint32_t>(minor),
            static_cast<std::uint32_t>(profile_mask));
        version_ok.has_error()) {
        return version_ok;
    }

    const gl_ubyte *renderer = get_string(k_gl_renderer);
    const std::string_view renderer_name =
        renderer != nullptr ? reinterpret_cast<const char *>(renderer) : std::string_view{};

    const auto [gpu_kind, gpu_index] = classify_current_gpu(m_egl_display, get_integerv);
    m_gpu.learn(gpu_kind, renderer_name, gpu_index);
    return gltfx_rslt<void>::ok();
}

void wayland_egl_context_adapter::attach_frame_listener() noexcept {
    if (m_pending_frame_callback != nullptr) {
        wl_callback_destroy(m_pending_frame_callback);
        m_pending_frame_callback = nullptr;
    }
    // wl_surface_frame() failing (allocation failure inside libwayland-
    // client) is not treated as a fatal open()/swap_buffers() error:
    // frame_callback_sequence's own arm_pending() still marks the next
    // swap as "waiting", and it will degrade to skipped_hidden once the
    // budget runs out on a callback that was never actually requested -
    // an honest degrade, never a crash, for a condition this project
    // has never measured happening.
    wl_callback *callback = wl_surface_frame(m_surface);
    if (callback != nullptr) {
        wl_callback_add_listener(callback, &k_frame_callback_listener, this);
        m_pending_frame_callback = callback;
    }
}

void wayland_egl_context_adapter::resize_surface_if_due() noexcept {
    const window_size pixel_size = m_window->state().pixel_size();
    const gltfx_rslt<gl_surface_size_decision> decision = resolve_gl_surface_size(
        pixel_size.width, pixel_size.height, m_buffer_width, m_buffer_height);
    if (decision.has_error() || !decision.value().should_resize) {
        return;
    }
    wl_egl_window_resize(m_egl_window, static_cast<int>(decision.value().width),
                         static_cast<int>(decision.value().height), 0, 0);
    m_buffer_width = decision.value().width;
    m_buffer_height = decision.value().height;
}

gltfx_rslt<void>
wayland_egl_context_adapter::open(wayland_window_adapter &window,
                                  std::span<const gltfx_gfx_option_entry> options) noexcept {
    if (!window.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }
    wl_surface *surface = window.surface();
    if (surface == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }

    // EGL-DEAD-DISPLAY-GUARD S3 (docs/plano-egl-dead-display-guard.md
    // sec. 3, D-S3): guarda ANTES de create_egl_display() abaixo, sobre
    // o wl_display obtido do wl_surface que acabou de ser resolvido -
    // sem isso, um segundo open() (ex.: consumidor tentando reabrir o
    // contexto depois de um close()) sobre uma conexão já morta chegava
    // a eglInitialize() e falhava com o nome genérico "egl_display" em
    // vez da interface que reprovou (plano sec. 2, linha "open() →
    // create_egl_display()"). Nada foi alocado por este adaptador
    // ainda neste ponto, então não há nada para close() liberar.
    if (const std::optional<gltfx_err> dead_on_entry =
            connection_failure_if_dead(wl_proxy_get_display(reinterpret_cast<wl_proxy *>(surface)));
        dead_on_entry.has_value()) {
        return gltfx_rslt<void>::err(*dead_on_entry);
    }

    // D-W6b-18: `adaptive` has no Wayland/EGL equivalent - refused BY
    // NAME here too, not only from a LATER set_option() call, since
    // `vsync` is `live` and this fatia's own resolved-options span may
    // already carry it if a consumer asked for `adaptive` straight in
    // the opening list (gl_context_desc_validation.cpp's own shape
    // check accepts any value in [0, 2] - refusing the THIRD one is
    // this adapter's own job, never silently downgraded to `on`).
    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::vsync && entry.value == 2) {
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
        }
    }

    if (const gltfx_rslt<void> display_ok = create_egl_display(*surface); display_ok.has_error()) {
        close();
        return display_ok;
    }

    void *config = nullptr;
    if (const gltfx_rslt<void> config_ok = choose_config(options, config); config_ok.has_error()) {
        close();
        return config_ok;
    }

    const window_size pixel_size = window.state().pixel_size();
    if (const gltfx_rslt<void> window_ok =
            create_egl_window(*surface, config, pixel_size.width, pixel_size.height);
        window_ok.has_error()) {
        close();
        return window_ok;
    }

    if (const gltfx_rslt<void> context_ok = create_context(config); context_ok.has_error()) {
        close();
        return context_ok;
    }

    for (const gltfx_gfx_option_entry &entry : options) {
        if (entry.id == gltfx_gfx_option::vsync) {
            m_vsync_on = entry.value != 0;
        }
    }

    m_surface = surface;
    m_window = &window;
    // MEDIDO (docs/plano-w6b-fatias-5.md, esta fatia, achado do
    // implementador contra kwin_wayland --virtual isolado, GODS_LAWS.md
    // L-44/L-50): open() NAO arma um wl_surface.frame aqui. Fazer isso
    // contradiz o proprio contrato que frame_callback_sequence.hpp ja
    // documenta (frame_wait_plan::present_immediately - "either this is
    // the very first frame this context has ever presented... the
    // adapter may call eglSwapBuffers() right now, no socket I/O needed
    // first") e o que frame_callback_sequence.cpp implementa (m_pending
    // nasce false). Pedir o callback e marcar m_pending=true AQUI, antes
    // de qualquer conteudo jamais commitado nesta superficie, e' pedir
    // um aviso que a superficie nunca ganha: medido via WAYLAND_DEBUG=1
    // que este compositor nunca da wl_callback.done para essa superficie
    // sem buffer - as cinco tentativas da primeira apresentacao
    // reprovavam com first_presented_attempt=0 antes deste conserto. O
    // rearme real (attach_frame_listener()+arm_pending()) que ja existe
    // no fim do ramo vsync=on de swap_buffers(), logo apos um eglSwap
    // Buffers() bem-sucedido, ja cobre o PROXIMO quadro - esta linha
    // aqui era redundante e, pela medicao, ativamente prejudicial.
    return gltfx_rslt<void>::ok();
}

void wayland_egl_context_adapter::close() noexcept {
    if (m_egl_context != nullptr) {
        eglMakeCurrent(m_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_egl_display, m_egl_context);
        m_egl_context = nullptr;
    }
    if (m_egl_surface != nullptr) {
        eglDestroySurface(m_egl_display, m_egl_surface);
        m_egl_surface = nullptr;
    }
    if (m_egl_display != nullptr) {
        eglTerminate(m_egl_display);
        m_egl_display = nullptr;
    }
    if (m_pending_frame_callback != nullptr) {
        wl_callback_destroy(m_pending_frame_callback);
        m_pending_frame_callback = nullptr;
    }
    if (m_egl_window != nullptr) {
        wl_egl_window_destroy(m_egl_window);
        m_egl_window = nullptr;
    }
    m_surface = nullptr;
    m_window = nullptr;
    m_frame_sequence = frame_callback_sequence{};
    m_gpu = gpu_kind_state{};
    m_buffer_width = 0;
    m_buffer_height = 0;
    m_msaa_supported = false;
    m_srgb_supported = false;
    m_swap_interval_honored = false;
    m_swap_calls_issued = 0;
}

gltfx_rslt<void> wayland_egl_context_adapter::make_current() noexcept {
    // EGL-DEAD-DISPLAY-GUARD S3 (D-S3): mesma guarda de swap_buffers()
    // (S2) e de open() (acima) - no COMEÇO, antes de qualquer chamada
    // EGL que fale com o fio.
    wl_display *display = wl_proxy_get_display(reinterpret_cast<wl_proxy *>(m_surface));
    if (const std::optional<gltfx_err> dead = connection_failure_if_dead(display);
        dead.has_value()) {
        return gltfx_rslt<void>::err(*dead);
    }

    if (eglMakeCurrent(m_egl_display, m_egl_surface, m_egl_surface, m_egl_context) != EGL_TRUE) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_make_current"));
    }
    return gltfx_rslt<void>::ok();
}

bool wayland_egl_context_adapter::present_would_skip() const noexcept {
    // The SYSTEM's own affirmative signal first (cheap, and arrives at
    // the compositor's `configure` before the alternative below ever
    // could - D-W6b-46's own reasoning, this method's own header
    // comment).
    if (m_window->state().state(window_state_bit::suspended)) {
        return true;
    }
    // The aging frame callback second - the only signal at all where
    // the compositor never affirms `suspended` (a version < 6 wm_base,
    // or one that never ties the bit to minimizing).
    return m_frame_sequence.pending_older_than(steady_now_ns(), k_frame_callback_budget_ms);
}

gltfx_rslt<gltfx_present_outcome> wayland_egl_context_adapter::swap_buffers() noexcept {
    // D-W6b-28: every failure path below that COULD be the wl_display
    // connection itself dying (a protocol error the compositor sent
    // under our feet, or the socket going away) resolves through this
    // ONE wl_surface's own display - the same wl_proxy_get_display()
    // trick create_egl_display() already uses one function up, hoisted
    // here so none of the call sites below has to recompute it.
    wl_display *display = wl_proxy_get_display(reinterpret_cast<wl_proxy *>(m_surface));

    // EGL-DEAD-DISPLAY-GUARD S2 (docs/plano-egl-dead-display-guard.md
    // sec. 3, D-S2): guarda no TOPO, antes de QUALQUER outra coisa -
    // inclusive antes de resize_surface_if_due(), que não fala com o
    // fio mas fica atrás desta checagem de todo jeito (plano sec. 2:
    // "a guarda vem antes dela"). Sem isto, uma troca anterior pode já
    // ter lido um erro de protocolo por dentro do próprio eglSwapBuffers
    // (llvmpipe/swrast, plano sec. 1.1) sem que ninguém tivesse
    // perguntado ainda - a resposta de hoje seria `presented` sobre uma
    // conexão já morta.
    if (const std::optional<gltfx_err> dead_on_entry = connection_failure_if_dead(display);
        dead_on_entry.has_value()) {
        return gltfx_rslt<gltfx_present_outcome>::err(*dead_on_entry);
    }

    resize_surface_if_due();

    if (!m_vsync_on) {
        // D-W6b-46: `vsync=off` used to present as fast as the driver
        // allows, UNCONDITIONALLY - the exact paridade defect (F3,
        // docs/plano-w6b-fatias-6-8.md sec. 0) this fatia closes: a
        // minimized window kept spinning the CPU here while the Win32
        // side already degraded via IsIconic(). The sonda this SAME
        // fatia gives both sides now guards this branch too, before
        // ever calling eglSwapBuffers() - never gated on the previous
        // frame's callback THE WAY vsync=on is below (D-W6b-18: this
        // branch still never WAITS for one), only on what the sonda
        // already knows without waiting for anything.
        if (present_would_skip()) {
            return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
        }
        return present_through_egl(display);
    }

    const frame_wait_plan plan = m_frame_sequence.plan_before_wait(k_frame_callback_budget_ms);
    if (plan == frame_wait_plan::give_up_without_polling) {
        return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
    }

    if (plan == frame_wait_plan::poll_then_decide) {
        if (!poll_and_dispatch_with_budget(display, k_frame_callback_budget_ms)) {
            // D-W6b-28: poll_and_dispatch_with_budget() only ever
            // returns false when the wl_display connection itself is
            // now unusable (this file's own comment on that function) -
            // always a real connection failure, never conditional on
            // wl_display_get_error() the way present_through_egl() is.
            return gltfx_rslt<gltfx_present_outcome>::err(build_connection_failure(display));
        }
        if (m_frame_sequence.decide_after_wait() == gltfx_present_outcome::skipped_hidden) {
            return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::skipped_hidden);
        }
    }

    return present_through_egl(display);
}

gltfx_rslt<gltfx_present_outcome>
wayland_egl_context_adapter::present_through_egl(wl_display *display) noexcept {
    const EGLBoolean swapped = eglSwapBuffers(m_egl_display, m_egl_surface);

    // EGL-DEAD-DISPLAY-GUARD S2 (D-S2): checado SEJA QUAL FOR o retorno
    // do EGL acima, nunca só quando ele falha - a correção do defeito
    // que motivou esta fatia inteira. O comentário antigo deste sítio
    // dizia que a mutação D-W6b-12 (ack_configure() removido) "aparece
    // aqui porque o EGL falha" - FALSO no llvmpipe/swrast (o backend
    // que este container e todo consumidor sem GPU dedicada usam,
    // plano sec. 1.1): dri2_wl_swrast_swap_buffers_with_damage()
    // devolve EGL_TRUE incondicionalmente, erro de protocolo ou não. A
    // única forma confiável de saber que a conexão morreu é perguntar
    // a ELA (connection_failure_if_dead(), S1), nunca ao valor de
    // retorno do EGL.
    if (const std::optional<gltfx_err> dead_after = connection_failure_if_dead(display);
        dead_after.has_value()) {
        return gltfx_rslt<gltfx_present_outcome>::err(*dead_after);
    }

    if (swapped != EGL_TRUE) {
        return gltfx_rslt<gltfx_present_outcome>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("egl_swap_buffers"));
    }

    ++m_swap_calls_issued;
    // D-W6b-46: the frame listener is armed after EVERY presentation,
    // in BOTH branches - the vsync=off branch never WAITS on it (its
    // own present_would_skip() already decided the frame without
    // touching the wire), but present_would_skip()'s own second
    // criterion needs a callback outstanding to age in the first
    // place, or a compositor that stops repainting this surface
    // without ever affirming `suspended` would never be detected at
    // all.
    attach_frame_listener();
    m_frame_sequence.arm_pending(steady_now_ns());
    return gltfx_rslt<gltfx_present_outcome>::ok(gltfx_present_outcome::presented);
}

void *wayland_egl_context_adapter::proc_address(std::string_view name) const noexcept {
    // eglGetProcAddress() needs a NUL-terminated name; `name` is a
    // string_view that may not own one. NOEXCEPT-ALLOC-B8 fatia F1
    // (/var/tmp/glintfx-plan/plano-conserto-noexcept.md, GODS_LAWS.md
    // L-04/L-20/L-22): a short-lived `const std::string owned(name)`
    // used to pay for that terminator here - an allocating, throw-
    // capable constructor inside a function that promises `noexcept`
    // (docs/api-conventions.md R3). tests/gl_proc_address_oom_test.cpp
    // proved that exact defect, by execution, against this SAME site
    // (std::terminate() under a forced allocation failure) before this
    // fix - the Windows gemeo of this same defect was already fixed in
    // babbd77 (src/platform/win32/wgl_proc_address.cpp), and this atom
    // is what now guarantees the two sides cannot silently diverge
    // again: both call the SAME copy_nul_terminated() template, over
    // the SAME k_max_proc_name_chars teto, so a change to one without
    // the other shows up as a straightforward, single-definition
    // change instead of two files a reader has to compare by hand
    // (the same reasoning src/platform/gl/gpu_kind_seam.hpp's own
    // header comment already gives itself for the identical shape).
    // A name too long to fit is treated as an ordinary lookup miss
    // (the same shape any other unresolvable name already gets), never
    // truncated - platform::nul_terminated_name.hpp's own header
    // comment has the full contract.
    std::array<char, k_max_proc_name_chars> name_buffer{};
    if (!copy_nul_terminated(name_buffer, name)) {
        return nullptr;
    }
    using egl_proc_fn = void (*)();
    // Function-pointer-to-object-pointer conversion: the SAME
    // universally-supported (if not strictly standard-blessed)
    // technique every GL loader (dlsym, GLAD, GLEW) already relies on -
    // this project's own public proc_address() contract (context.hpp)
    // exists to hand a consumer exactly this kind of address.
    egl_proc_fn function = eglGetProcAddress(name_buffer.data());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
    // dlsym-style function-to-object-pointer cast every GL loader already relies on.
    return reinterpret_cast<void *>(function);
}

std::pair<std::uint32_t, std::uint32_t>
wayland_egl_context_adapter::egl_surface_pixel_size() const noexcept {
    if (m_egl_surface == nullptr) {
        return {0, 0};
    }
    EGLint width = 0;
    EGLint height = 0;
    eglQuerySurface(m_egl_display, m_egl_surface, EGL_WIDTH, &width);
    eglQuerySurface(m_egl_display, m_egl_surface, EGL_HEIGHT, &height);
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

gltfx_rslt<void> wayland_egl_context_adapter::apply_option(gltfx_gfx_option_entry entry) noexcept {
    if (entry.id == gltfx_gfx_option::vsync) {
        if (entry.value == 2) { // adaptive - D-W6b-18, no Wayland/EGL equivalent
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported).with_rejected_value("vsync"));
        }
        m_vsync_on = entry.value != 0;
        return gltfx_rslt<void>::ok();
    }

    // Every other `live` id (frame_rate_cap, preset) is accepted here
    // with no adapter-side effect yet - see this class's own header
    // comment on apply_option() for why that is honest, not a gap.
    return gltfx_rslt<void>::ok();
}

gltfx_gfx_option_support
wayland_egl_context_adapter::option_support(gltfx_gfx_option id) const noexcept {
    switch (id) {
    case gltfx_gfx_option::vsync:
    case gltfx_gfx_option::frame_rate_cap:
    case gltfx_gfx_option::gpu_preference:
    case gltfx_gfx_option::preset:
        return gltfx_gfx_option_support::supported;
    case gltfx_gfx_option::msaa_samples:
        return m_msaa_supported ? gltfx_gfx_option_support::supported
                                : gltfx_gfx_option_support::unsupported_here;
    case gltfx_gfx_option::srgb_framebuffer:
        return m_srgb_supported ? gltfx_gfx_option_support::supported
                                : gltfx_gfx_option_support::unsupported_here;
    case gltfx_gfx_option::auto_choice_reason:
    case gltfx_gfx_option::power_source:
        return gltfx_gfx_option_support::read_only_here;
    }
    return gltfx_gfx_option_support::unsupported_here;
}

void wayland_egl_context_adapter::frame_callback_done(void *data, wl_callback *callback,
                                                      std::uint32_t /*callback_data*/) noexcept {
    auto *self = static_cast<wayland_egl_context_adapter *>(data);
    if (self->m_pending_frame_callback == callback) {
        self->m_pending_frame_callback = nullptr;
    }
    wl_callback_destroy(callback);
    self->m_frame_sequence.mark_frame_done();
}

} // namespace glintfx::platform
