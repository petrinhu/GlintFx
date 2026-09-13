// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/loop/loop_context_mark.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// loop_parity_test.cpp - LOOP-RUN fatia 8, sub-fatia S4 (docs/plano-
// loop-callbacks.md / /var/tmp/glintfx-plan/loop-fix.md sec. S4.4-
// S4.6, GODS_LAWS.md L-04): ONE file, ONLY the public API
// (glintfx::gltfx_display/gltfx_window/gltfx_gl_context/gltfx_loop),
// NO #if of any kind, no harness - the SAME shape window_parity_test.
// cpp and gl_context_parity_test.cpp (this same directory) already
// use, and the SAME P-0 mechanism (tests/tools/check_test_parity.py):
// the Windows side links this file into a plain add_executable()/
// add_test() target (tests/CMakeLists.txt, if(WIN32)), the Linux side
// stages and runs it as a container fixture of this EXACT name
// (tests/container/prepare_arch_ports_fixture.sh, Containerfile, the
// wayland-container job of .github/workflows/ci.yml) - ctest itself is
// the only thing that differs, never the source.
//
// WHY THIS FILE EXISTS AT ALL: every promise of include/glintfx/
// platform/loop/loop.hpp used to be proven ONLY against the pure atoms
// underneath it (frame_tick_state, frame_cap_schedule, loop_callbacks_
// validation) or against the engine template with fake ports. Nothing
// in the whole suite ever opened a real display, a real window and a
// real graphics context and drove gltfx_loop THROUGH THE PACKAGED
// LIBRARY BOUNDARY (.so/.dll) - so the WIRING between facade and atom
// was, measurably, untested. That is the entire subject of this file.
//
// THE CRITERION IS FIXED HERE, BEFORE THE DATA EXISTS (sec. S4.4,
// GODS_LAWS.md L-43) - three kinds of line, never confused:
//   - ASSERTED EQUAL on both systems: the eighteen check() call sites
//     below, each printing its own "camada 2" verdict line (sec.
//     S4.5's own two-layer report: raw numbers first, then one line
//     per assertion with the criterion spelled out, so a reviewer
//     reads the verdict in the log without recomputing anything).
//   - MEASURED, PRINTED, NEVER ASSERTED EQUAL (sec. S4.5, each with
//     its own line in tests/measured_exceptions.txt, added in this
//     same commit): mean_step_ns, max_step_ns, rendered_frames,
//     cap30_wall_ms, swap_failures_tolerated.
//   - PLAIN DIAGNOSTIC TEXT: everything else, read by a human.
//
// WHAT THIS FILE DELIBERATELY DOES NOT PROVE (sec. S4.3, declared
// here so nobody infers coverage from an absence):
//   - A real graphics board, and vsync locked to a real monitor: both
//     executors render in software (llvmpipe on Linux, the Mesa
//     opengl32.dll placed beside the test binaries on Windows). The
//     beat P7 warns about when a frame cap equals the refresh rate is
//     NOT measured anywhere.
//   - Focus loss and occlusion by ANOTHER window: neither executor
//     provokes either one. Declared absence, never "proven identical".
//   - open() refusing by the names "display" and "window" (P10): NOT
//     exercised live, here or anywhere - covered ONLY by reading the
//     three ordered is_open() checks of src/platform/loop/loop_facade.
//     cpp:114-121, and declared as such in docs/gl-loop-portability-
//     matrix.md. THE REASON, self-contained so nobody has to find the
//     P10 block below to learn it: the only technique that produces a
//     genuinely closed handle in this tree is to MOVE a real, open one
//     into a second handle and use the husk left behind (no seam
//     fabricates a closed handle without a real open() first -
//     measured 13/09/2026, D-091305). Applying that to the display or
//     to the window would consume the very display/window every
//     remaining assertion of this file depends on, and open() checks
//     them BEFORE the context, so the run would end there. Only
//     "context" can be spent that way and replaced, and that is the
//     one this file proves.
//   - A moved-from gltfx_loop handed to set_callbacks()/step(): that
//     is an assert() in Debug and undefined behaviour in Release,
//     provable only from a subprocess fixture. Item LOOP-MOVED-FROM-
//     LIVE, not this file.
//   - P3's internal ORDER inside one step(): there is no public getter
//     for a window title, so "set it on tick k, read it back on tick
//     k+1" is not expressible through the public API. The set_title()
//     call below therefore asserts only that the window stays usable
//     mid-loop; the ORDER itself stays proven by the engine's own
//     tests, and stays declared in the portability matrix.
//
// EVERY TEXT LITERAL IN THIS FILE IS PLAIN ASCII, ON PURPOSE: this
// same source is compiled by MSVC on the Windows side, this project
// passes no /utf-8 flag (cmake/), and a non-ASCII byte in a narrow
// literal would raise C4819 - escalated to an error by /WX (cmake/
// GlintfxCompileOptions.cmake). The Portuguese words below are
// therefore unaccented, exactly as every other fixture of this family
// already writes them.

namespace {

// ---------------------------------------------------------------
// GL entry points declared by hand, <GL/gl.h> never linked (GODS_
// LAWS.md L-07) - resolved through gltfx_gl_context::proc_address()
// itself, the SAME technique gl_context_parity_test.cpp (this same
// directory) already documents at length.
// ---------------------------------------------------------------
using gl_enum = unsigned int;
using gl_float = float;

constexpr gl_enum k_gl_color_buffer_bit = 0x00004000;

using gl_clear_color_fn = void (*)(gl_float, gl_float, gl_float, gl_float);
using gl_clear_fn = void (*)(gl_enum);

gl_clear_color_fn g_clear_color = nullptr;
gl_clear_fn g_clear = nullptr;

// ---------------------------------------------------------------
// The verdict ledger (sec. S4.4's own last row, GODS_LAWS.md L-40's
// own non-empty-sweep floor). k_planned_assertions is a CONSTANT, not
// a running total: a run that aborts early prints a number SMALLER
// than it, and that mismatch is itself a reproval - "18 de 18
// avaliadas" on one system and "4 de 18" on the other is exactly the
// silent coverage loss this floor exists to make loud.
// ---------------------------------------------------------------
constexpr int k_planned_assertions = 18;

int g_assertions_evaluated = 0;
int g_assertions_failed = 0;

// One assertion, one printed verdict line, always - the "camada 2" of
// sec. S4.5's own two-layer report. NEVER returns early out of main():
// every one of the eighteen has to be evaluated on BOTH systems for
// the counts to be comparable, so a failure is recorded and execution
// continues wherever continuing is physically possible.
void check(const char *key, bool condition, const char *criterion, const std::string &observed) {
    ++g_assertions_evaluated;
    if (!condition) {
        ++g_assertions_failed;
    }
    std::fprintf(stdout, "loop_parity_test: %s=%s (criterio %s) %s\n", key, observed.c_str(),
                 criterion, condition ? "OK" : "FALHOU");
}

// The scope line MUST survive every exit path, including the early
// `return EXIT_FAILURE` sites below that fire when the environment
// itself refuses to hand over a display/window/context (there is no
// assertion to record in those cases - there is nothing to assert
// ABOUT). A destructor runs on ANY scope exit, so binding the print to
// one guarantees it fires exactly once, whatever path got there - the
// same technique gl_context_parity_test.cpp's own swap_tolerated_
// downgrades_reporter already uses one layer down, for the same
// measured reason (a print sitting at the bottom of main() never ran
// on the paths that reproved).
struct scope_reporter {
    ~scope_reporter() noexcept {
        std::fprintf(stdout, "loop_parity_test: assercoes %d de %d avaliadas\n",
                     g_assertions_evaluated, k_planned_assertions);
    }
};

[[nodiscard]] std::string to_text(long long value) { return std::to_string(value); }

[[nodiscard]] std::string err_text(const glintfx::gltfx_err &err) {
    return std::string(glintfx::gltfx_err_code_name(err.code())) + "/" +
           std::string(err.rejected_value());
}

// ---------------------------------------------------------------
// GL-CI-SOFTWARE TOLERANCE (decisao do lider, 07/09/2026, por
// AskUserQuestion: "tolerar, mas so sob prova") - the SAME three
// factors in conjunction gl_context_parity_test.cpp:130-148 already
// fixes, plus gpu().kind == software, plus "some other swap of THIS
// SAME execution already succeeded". Any other combination still
// reproves exactly as before. Copied in SHAPE, never in effect: sec.
// S4.4's own row requires this file to apply the identical rule so a
// downgrade means the same thing in both logs.
// ---------------------------------------------------------------
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
    // NEVER by renderer-name text (include/glintfx/platform/gl/gpu.hpp
    // forbids it) - only the CLOSED `kind` type.
    if (gpu_kind != glintfx::gltfx_gpu_kind::software) {
        return false;
    }
    return any_other_swap_succeeded;
}

// ---------------------------------------------------------------
// The consumer object the loop calls back into. Inherits gltfx_loop_
// context_mark (LOOP-CONTEXT-MARK, S1d): costs exactly zero bytes and
// zero instructions in Release, and in the `debug`/`windows-debug`
// jobs makes the .so/.dll's own bind thunks check, before every single
// call, that this object is still alive. It gets NO assertion of its
// own here on purpose - a dead object at this call site would be a
// defect of THIS TEST, never of the library.
// ---------------------------------------------------------------
int g_app_destructions = 0;
int g_last_destroyed_id = 0;
int g_ticks_of_last_app = 0;
int g_destructions_seen_inside_callback = -1;

struct app : glintfx::gltfx_loop_context_mark {
    int id = 0;
    int ticks = 0;
    int stop_every = 5;
    int renders = 0;

    ~app() {
        ++g_app_destructions;
        g_last_destroyed_id = id;
    }

    app() noexcept = default;
    app(const app &) = delete;
    app &operator=(const app &) = delete;
    app(app &&) = delete;
    app &operator=(app &&) = delete;

    // P11/P12's own "the destructor runs AFTER the last callback"
    // half is measured HERE, from inside the callback itself: a
    // library that destroyed the context before or during this call
    // would leave a non-zero count behind in the global below.
    [[nodiscard]] bool frame(const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
        ++ticks;
        g_ticks_of_last_app = ticks;
        g_destructions_seen_inside_callback = g_app_destructions;
        return (ticks % stop_every) != 0;
    }

    void render(const glintfx::gltfx_frame_tick & /*tick*/) noexcept { ++renders; }
};

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture of this family (window_parity_test.cpp's own
    // header comment on this exact line: `_IOLBF` with size 0 crashed
    // the Windows CI job with 0xC0000409, `_IONBF` is the one mode
    // MSVC's own docs confirm actually disables buffering on Win32).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const scope_reporter reporter;

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(stderr, "loop_parity_test: gltfx_display::open() failed: %s\n",
                     err_text(display_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "janela de paridade do laco principal",
        .application_id = "org.glintfx.loop_parity_test",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "loop_parity_test: gltfx_window::open() failed: %s\n",
                     err_text(window_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    // ===============================================================
    // P10, LIVE (D-091305): open() refuses BY NAME a graphics context
    // that is not open.
    //
    // THE TECHNIQUE, AND WHY IT IS THE ONLY ONE AVAILABLE (measured
    // refutation, 13/09/2026, registered in DECISOES_AUTONOMAS.md):
    // there is no seam anywhere in this tree that fabricates a
    // "closed" handle without a REAL open() against the backend first
    // - all three types are move-only, have no default constructor,
    // and their private constructor's only friend merely READS the
    // inner pointer of an already existing instance. What DOES work,
    // and is used here: open a real context, MOVE it into a second
    // handle, and hand the moved-from husk to gltfx_loop::open(). The
    // moved-from object's own m_impl is nulled by the move constructor
    // (src/platform/gl/gl_context_facade.cpp), so is_open() is false
    // while the real backend context is still perfectly alive inside
    // the destination handle - no backend teardown needed, and none of
    // the surrounding state disturbed.
    //
    // WHY THIS RUNS FIRST, BEFORE THE LOOP'S OWN CONTEXT EXISTS: the
    // pair below opens and closes a context of its own on this window;
    // doing that AFTER the loop's context is current would hand
    // `make_current` to a second context behind the loop's back. The
    // reopen is legitimate (gl_context_parity_test.cpp proves reopening
    // a window with the SAME - empty - option list is accepted, since
    // an empty list resolves to exactly the defaults the first open
    // already fixed).
    //
    // NEITHER "display" NOR "window" IS REACHABLE THE SAME WAY: a
    // moved-from gltfx_display or gltfx_window would be refused
    // before the context is ever looked at, which would prove the
    // ORDER of the three checks but would leave this window/display
    // unusable for everything below. They stay declared, never
    // asserted (this file's own header comment).
    // ===============================================================
    {
        const glintfx::gltfx_gl_context_desc probe_desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> probe_opened =
            glintfx::gltfx_gl_context::open(window, probe_desc);
        if (probe_opened.has_error()) {
            std::fprintf(stderr, "loop_parity_test: contexto da sonda P10 falhou ao abrir: %s\n",
                         err_text(probe_opened.err()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_gl_context probe_context = std::move(probe_opened.value());
        glintfx::gltfx_gl_context alive_context = std::move(probe_context);

        glintfx::gltfx_rslt<glintfx::gltfx_loop> refused =
            glintfx::gltfx_loop::open(display, window, probe_context);
        const bool refused_by_name =
            refused.has_error() &&
            refused.err().code() == glintfx::gltfx_err_code::invalid_argument &&
            refused.err().rejected_value() == std::string_view{"context"};
        check("recusa_open_contexto_fechado", refused_by_name, "invalid_argument/context",
              refused.has_error() ? err_text(refused.err()) : std::string("aceito (sem erro)"));
    }

    const glintfx::gltfx_gl_context_desc empty_desc{};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
        glintfx::gltfx_gl_context::open(window, empty_desc);
    if (context_opened.has_error()) {
        std::fprintf(stderr, "loop_parity_test: gltfx_gl_context::open() failed: %s\n",
                     err_text(context_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_gl_context context = std::move(context_opened.value());
    if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
        std::fprintf(stderr, "loop_parity_test: make_current() failed: %s\n",
                     err_text(current.err()).c_str());
        return EXIT_FAILURE;
    }

    void *clear_color_addr = context.proc_address("glClearColor");
    void *clear_addr = context.proc_address("glClear");
    if (clear_color_addr == nullptr || clear_addr == nullptr) {
        std::fprintf(stderr, "loop_parity_test: proc_address falhou - glClearColor=%d glClear=%d\n",
                     clear_color_addr != nullptr, clear_addr != nullptr);
        return EXIT_FAILURE;
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal dlsym-style
    // function-to-object-pointer cast every GL loader already relies on (gl_context_parity_test.cpp
    // carries the same pair of markers, for the same two lines).
    g_clear_color = reinterpret_cast<gl_clear_color_fn>(clear_color_addr);
    g_clear = reinterpret_cast<gl_clear_fn>(clear_addr);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above

    const glintfx::gltfx_gpu_info gpu = context.gpu();

    glintfx::gltfx_rslt<glintfx::gltfx_loop> loop_opened =
        glintfx::gltfx_loop::open(display, window, context);
    if (loop_opened.has_error()) {
        std::fprintf(stderr, "loop_parity_test: gltfx_loop::open() failed: %s\n",
                     err_text(loop_opened.err()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_loop loop = std::move(loop_opened.value());

    // ===============================================================
    // Thirty manual step()/present() cycles: P2, P5, the monotonic
    // clock, and the two implications of P4 (called P-a and P-b in
    // sec. S4.4), all accumulated into booleans and asserted ONCE
    // after the loop - never one check() per tick, which would make
    // the printed assertion count depend on how many ticks ran.
    // ===============================================================
    constexpr int k_manual_ticks = 30;
    bool frame_index_contiguous = true;
    bool elapsed_within_contract = true;
    bool first_elapsed_zero = false;
    bool now_never_went_back = true;
    bool p_a_holds = true;
    bool p_b_holds = true;
    bool untolerated_swap_failure = false;
    int rendered_frames = 0;
    int swap_failures_tolerated = 0;
    bool any_swap_succeeded = false;
    std::int64_t total_step_ns = 0;
    std::int64_t max_step_ns = 0;
    std::uint64_t expected_index = 1;
    glintfx::gltfx_time_point previous_now{};
    bool have_previous_now = false;
    bool set_title_ok = true;
    // DIAGNOSTICO, nunca assercao: quantos dos 30 tiques o executor
    // recusou desenhar, e quantas trocas ele respondeu `skipped_hidden`.
    // Sem estes dois numeros, um `rendered_frames` baixo nao distingue
    // "o laco nao desenha" de "este executor nunca mostra a janela".
    int ticks_without_render = 0;
    int skipped_hidden_outcomes = 0;

    for (int i = 0; i < k_manual_ticks; ++i) {
        const auto step_start = std::chrono::steady_clock::now();
        glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
        const auto step_end = std::chrono::steady_clock::now();
        if (ticked.has_error()) {
            std::fprintf(stderr, "loop_parity_test: step() #%d falhou: %s\n", i + 1,
                         err_text(ticked.err()).c_str());
            return EXIT_FAILURE;
        }
        const glintfx::gltfx_frame_tick tick = ticked.value();

        const std::int64_t step_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(step_end - step_start).count();
        total_step_ns += step_ns;
        if (step_ns > max_step_ns) {
            max_step_ns = step_ns;
        }

        if (tick.frame_index != expected_index) {
            frame_index_contiguous = false;
        }
        ++expected_index;

        if (tick.elapsed.nanoseconds < 0 ||
            tick.elapsed.nanoseconds > glintfx::k_gltfx_max_frame_elapsed.nanoseconds) {
            elapsed_within_contract = false;
        }
        if (i == 0) {
            first_elapsed_zero = tick.elapsed.nanoseconds == 0;
        }

        if (have_previous_now && tick.now.ticks < previous_now.ticks) {
            now_never_went_back = false;
        }
        previous_now = tick.now;
        have_previous_now = true;

        // P-b: a tick that refuses to render can only do so because the
        // last present() skipped a hidden window.
        if (!tick.should_render &&
            tick.last_present != glintfx::gltfx_present_outcome::skipped_hidden) {
            p_b_holds = false;
        }

        // P3's observable half, as far as the public API allows: the
        // window stays usable in the middle of a running loop. See this
        // file's own header comment for why this is NOT an assertion
        // about the ORDER inside step().
        if (i == k_manual_ticks / 2) {
            if (glintfx::gltfx_rslt<void> titled = window.set_title("laco em execucao");
                titled.has_error()) {
                set_title_ok = false;
            }
        }

        if (!tick.should_render) {
            ++ticks_without_render;
        }
        if (tick.should_render) {
            g_clear_color(0.1F, 0.2F, 0.3F, 1.0F);
            g_clear(k_gl_color_buffer_bit);
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = loop.present();
            if (presented.has_error()) {
                if (should_tolerate_swap_failure(presented.err(), gpu.kind, any_swap_succeeded)) {
                    ++swap_failures_tolerated;
                    std::fprintf(stdout,
                                 "DOWNGRADE: loop_parity_test.manual_present tolerada - %s, "
                                 "os_error_code=0, gpu().kind=software, com outra troca desta "
                                 "MESMA execucao ja bem-sucedida antes desta.\n",
                                 err_text(presented.err()).c_str());
                    continue;
                }
                untolerated_swap_failure = true;
                std::fprintf(stderr,
                             "loop_parity_test: present() #%d falhou fora da tolerancia: %s "
                             "(os_error_code=%lld, gpu_kind=%d, outra_troca_ok=%d)\n",
                             i + 1, err_text(presented.err()).c_str(),
                             static_cast<long long>(presented.err().os_error_code()),
                             static_cast<int>(gpu.kind), any_swap_succeeded ? 1 : 0);
                continue;
            }
            if (presented.value() == glintfx::gltfx_present_outcome::presented) {
                ++rendered_frames;
                any_swap_succeeded = true;
            } else if (presented.value() == glintfx::gltfx_present_outcome::skipped_hidden) {
                ++skipped_hidden_outcomes;
            }
        }

        // P-a: whatever the present() above produced, a tick that DID
        // present has to have said should_render first. Evaluated
        // against the NEXT tick's own last_present in the next
        // iteration; here it is the direct reading of this tick.
        if (tick.last_present == glintfx::gltfx_present_outcome::presented && !tick.should_render) {
            p_a_holds = false;
        }
    }

    std::fprintf(stdout,
                 "loop_parity_test: diagnostico: tiques_sem_desenho=%d de %d, "
                 "trocas_respondidas_skipped_hidden=%d\n",
                 ticks_without_render, k_manual_ticks, skipped_hidden_outcomes);

    const std::int64_t mean_step_ns = total_step_ns / k_manual_ticks;
    std::fprintf(stdout, "MEASURED loop_parity_test.mean_step_ns=%lld\n",
                 static_cast<long long>(mean_step_ns));
    std::fprintf(stdout, "MEASURED loop_parity_test.max_step_ns=%lld\n",
                 static_cast<long long>(max_step_ns));
    std::fprintf(stdout, "MEASURED loop_parity_test.rendered_frames=%d\n", rendered_frames);

    check("frame_index_contiguo", frame_index_contiguous, "1..30 sem lacuna",
          frame_index_contiguous ? std::string("contiguo")
                                 : std::string("lacuna detectada (ver stderr)"));
    check("elapsed_dentro_do_contrato", elapsed_within_contract, "0 <= elapsed <= 250 ms",
          elapsed_within_contract ? std::string("dentro") : std::string("fora"));
    check("elapsed_zero_no_primeiro_tique", first_elapsed_zero, "== 0",
          first_elapsed_zero ? std::string("0") : std::string("diferente de 0"));
    check("now_nao_decrescente", now_never_went_back, "monotonico entre tiques",
          now_never_went_back ? std::string("monotonico") : std::string("andou para tras"));
    check("P_a_presented_implica_should_render", p_a_holds, "presented => should_render",
          p_a_holds ? std::string("vale") : std::string("violada"));
    check("P_b_sem_render_implica_skipped_hidden", p_b_holds,
          "!should_render => last_present == skipped_hidden",
          p_b_holds ? std::string("vale") : std::string("violada"));
    check("rendered_frames", rendered_frames >= 1, ">= 1 em 30 tiques visiveis",
          to_text(rendered_frames));
    check("set_title_no_meio_do_laco", set_title_ok, "ok()",
          set_title_ok ? std::string("ok") : std::string("erro"));

    // ===============================================================
    // P9, refusal half, through the FACADE (not through the pure atom
    // - the eight cells of validate_loop_callbacks() are already
    // proven on all five systems by loop_callbacks_validation_test;
    // repeating them here would count the same proof twice). ONE cell:
    // an empty on_frame, which proves the facade really does call the
    // atom before touching any port.
    // ===============================================================
    {
        const glintfx::gltfx_loop_callbacks empty_callbacks{};
        glintfx::gltfx_rslt<void> refused = loop.run(empty_callbacks);
        const bool refused_by_name =
            refused.has_error() &&
            refused.err().code() == glintfx::gltfx_err_code::invalid_argument &&
            refused.err().rejected_value() == std::string_view{"on_frame"};
        check("recusa_run_on_frame_nulo", refused_by_name, "invalid_argument/on_frame",
              refused.has_error() ? err_text(refused.err()) : std::string("aceito (sem erro)"));
    }

    // ===============================================================
    // P9, termination half, through LAYER 3 (gltfx_bind_loop_callbacks,
    // S1c) crossing the REAL binary boundary: a member function pair
    // bound at compile time, an object that lives on this stack, and
    // on_frame returning false on the fifth tick.
    // ===============================================================
    {
        app bound_app;
        bound_app.id = 1;
        bound_app.stop_every = 5;
        glintfx::gltfx_rslt<void> ran =
            loop.run(glintfx::gltfx_bind_loop_callbacks<&app::frame, &app::render>(bound_app));
        const bool five_ticks = !ran.has_error() && bound_app.ticks == 5;
        check("run_bind_encerra_em_5_tiques", five_ticks, "ok() com exatamente 5 tiques",
              ran.has_error() ? err_text(ran.err()) : to_text(bound_app.ticks) + " tique(s)");
    }

    // ===============================================================
    // P7, live: a frame-rate cap of 30 Hz with vsync OFF. The LOWER
    // bound is the one that proves the cap bites at all - without it,
    // thirty vsync-off frames cost under 700 ms (measured in fatia 5).
    // The UPPER bound only tolerates the 15.6 ms timer granularity a
    // Windows executor without a high-resolution timer carries.
    //
    // SAFETY VALVE [REV-3], fixed BEFORE this run and deliberately NOT
    // exercised here: should cap30_wall_ms exceed 1500 ON WINDOWS,
    // with Linux inside the band AND high_resolution_wait == 0 in the
    // same log, the UPPER bound rises ONCE, to the measured value plus
    // ten percent rounded up to a multiple of 100 ms, citing that run
    // in the commit that raises it. The LOWER bound never moves. No
    // number here has been raised: the band below is the one written
    // before any data existed.
    // ===============================================================
    {
        if (glintfx::gltfx_rslt<void> off =
                context.set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
            off.has_error()) {
            std::fprintf(stderr, "loop_parity_test: set_option(vsync=off) falhou: %s\n",
                         err_text(off.err()).c_str());
            return EXIT_FAILURE;
        }
        if (glintfx::gltfx_rslt<void> capped =
                context.set_option({.id = glintfx::gltfx_gfx_option::frame_rate_cap, .value = 30});
            capped.has_error()) {
            std::fprintf(stderr, "loop_parity_test: set_option(frame_rate_cap=30) falhou: %s\n",
                         err_text(capped.err()).c_str());
            return EXIT_FAILURE;
        }

        // UM numero so para este trecho, lido pelo laco E pelo portao
        // logo abaixo: se alguem mudar a quantidade de ciclos e o
        // portao continuar comparando com um literal antigo, a
        // comparacao passa a ser sempre falsa em silencio - a familia
        // exata de defeito que esta fatia existe para cacar.
        constexpr int k_cap_ticks = 30;
        int cap_ticks_without_render = 0;
        const auto cap_start = std::chrono::steady_clock::now();
        for (int i = 0; i < k_cap_ticks; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_frame_tick> ticked = loop.step();
            if (ticked.has_error()) {
                std::fprintf(stderr, "loop_parity_test: step() com teto #%d falhou: %s\n", i + 1,
                             err_text(ticked.err()).c_str());
                return EXIT_FAILURE;
            }
            if (!ticked.value().should_render) {
                ++cap_ticks_without_render;
                continue;
            }
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented = loop.present();
            if (presented.has_error()) {
                if (should_tolerate_swap_failure(presented.err(), gpu.kind, any_swap_succeeded)) {
                    ++swap_failures_tolerated;
                    std::fprintf(stdout,
                                 "DOWNGRADE: loop_parity_test.cap30_present tolerada - %s, "
                                 "os_error_code=0, gpu().kind=software.\n",
                                 err_text(presented.err()).c_str());
                    continue;
                }
                untolerated_swap_failure = true;
                std::fprintf(stderr,
                             "loop_parity_test: present() com teto #%d falhou fora da "
                             "tolerancia: %s\n",
                             i + 1, err_text(presented.err()).c_str());
                continue;
            }
            if (presented.value() == glintfx::gltfx_present_outcome::presented) {
                any_swap_succeeded = true;
            }
        }
        const auto cap_end = std::chrono::steady_clock::now();
        const std::int64_t cap30_wall_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(cap_end - cap_start).count();
        std::fprintf(stdout, "MEASURED loop_parity_test.cap30_wall_ms=%lld\n",
                     static_cast<long long>(cap30_wall_ms));
        // DIAGNOSTICO, nunca assercao: um tique que recusa desenhar paga
        // a espera de janela escondida (100 ms) EM VEZ da espera do teto
        // de quadros, entao este numero e' o que separa "o teto nao
        // morde" de "este executor reporta a janela como escondida".
        std::fprintf(stdout, "loop_parity_test: diagnostico: cap30_tiques_sem_desenho=%d de %d\n",
                     cap_ticks_without_render, k_cap_ticks);
        // CONDICIONADA, no mesmo molde de hidden_detected/restored do
        // fixture irmao (loop_hidden_test): a chave sai impressa SEMPRE,
        // e a faixa so deixa de reprovar quando o diagnostico irmao
        // prova que o executor nao deixou o laco desenhar em NENHUM dos
        // 30 tiques. Nesse estado cada tique paga a espera de janela
        // oculta (100 ms) em vez da espera do teto, e o numero medido
        // nao fala mais sobre o teto - fato de ambiente medido em
        // 13/09/2026 (kwin_wayland --virtual nunca entrega o retorno de
        // quadro do EGL), declarado e CONTADO em tests/measured_
        // exceptions.txt sob o item LOOP-CAP30-KWIN-VIRTUAL, com a
        // prova da biblioteca no lugar dele (frame_cap_schedule_test,
        // sem sistema operacional, seis celulas que mordem por mutacao).
        // Num executor que mostra a janela - o Windows de hoje, e um
        // compositor de sessao real amanha - a faixa volta a reprovar.
        //
        // TODOS os tiques, nunca "ao menos um" (achado da re-verificacao
        // por arquivo:linha, 13/09/2026): a primeira versao desta linha
        // desligava a faixa assim que UM tique de 30 recusasse desenhar,
        // e o comentario acima ja dizia "nenhum dos 30" - o texto estava
        // certo e o codigo nao o alcancava. O estrago seria calado e
        // grande: cada tique sem desenho troca a espera do teto pela
        // espera de janela oculta (100 ms), entao UM tique perdido nao
        // tira a rodada da faixa - so o trecho INTEIRO tira. Com a
        // versao antiga, um executor que pulasse um unico quadro faria
        // a verificacao do teto desaparecer sem que nada no log
        // dissesse isso.
        const bool cap_every_tick_was_hidden = cap_ticks_without_render == k_cap_ticks;
        check("cap30_wall_ms",
              cap_every_tick_was_hidden || (cap30_wall_ms >= 900 && cap30_wall_ms <= 1500),
              "900..1500 ms, desligado SO quando os 30 tiques recusaram desenhar "
              "(LOOP-CAP30-KWIN-VIRTUAL)",
              to_text(cap30_wall_ms) + " ms, com " + to_text(cap_ticks_without_render) + " de " +
                  to_text(k_cap_ticks) + " tiques sem desenho");

        // The cap is left OFF for everything below: the ownership cells
        // that follow count destructors, never milliseconds, and a
        // 30 Hz cap would make each of them cost a real second.
        if (glintfx::gltfx_rslt<void> uncapped =
                context.set_option({.id = glintfx::gltfx_gfx_option::frame_rate_cap, .value = 0});
            uncapped.has_error()) {
            std::fprintf(stderr, "loop_parity_test: set_option(frame_rate_cap=0) falhou: %s\n",
                         err_text(uncapped.err()).c_str());
            return EXIT_FAILURE;
        }
    }

    std::fprintf(stdout, "MEASURED loop_parity_test.swap_failures_tolerated=%d\n",
                 swap_failures_tolerated);
    check("swap_buffers_fora_da_tolerancia", !untolerated_swap_failure,
          "nenhuma falha de troca de quadro fora dos tres fatores",
          untolerated_swap_failure ? std::string("houve") : std::string("nenhuma"));

    // ===============================================================
    // P11 - FORM 1 of context ownership: destroy_context set, handed
    // to run(callbacks). Destroyed EXACTLY ONCE, AFTER the last
    // callback, on every return path. Two cells (sec. S4.4).
    // ===============================================================
    {
        const int before = g_app_destructions;
        g_destructions_seen_inside_callback = -1;
        g_ticks_of_last_app = 0;
        app *owned = new app;
        owned->id = 11;
        owned->stop_every = 3;
        glintfx::gltfx_rslt<void> ran =
            loop.run(glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(owned));
        const int destroyed = g_app_destructions - before;
        const bool cell_ok = destroyed == 1 && g_destructions_seen_inside_callback == before &&
                             (!ran.has_error() && g_ticks_of_last_app == 3);
        check("posse_forma1_encerramento_no_3o_tique", cell_ok,
              "destrutor 1x, depois da ultima chamada de volta, com 3 tiques",
              to_text(destroyed) + " destruicao(oes), " + to_text(g_ticks_of_last_app) +
                  " tique(s), vistas dentro da chamada=" +
                  to_text(g_destructions_seen_inside_callback - before) +
                  (ran.has_error() ? (", run=" + err_text(ran.err())) : std::string(", run=ok")));
    }
    {
        const int before = g_app_destructions;
        g_ticks_of_last_app = 0;
        app *owned = new app;
        owned->id = 12;
        owned->stop_every = 1;
        glintfx::gltfx_rslt<void> ran =
            loop.run(glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(owned));
        const int destroyed = g_app_destructions - before;
        const bool cell_ok = destroyed == 1 && !ran.has_error() && g_ticks_of_last_app == 1;
        check("posse_forma1_caminho_curto_1_tique", cell_ok, "destrutor 1x no caminho curto",
              to_text(destroyed) + " destruicao(oes), " + to_text(g_ticks_of_last_app) +
                  " tique(s)");
    }

    // ===============================================================
    // P12 - FORM 2: a context handed to set_callbacks() survives any
    // number of run() calls and is destroyed exactly once, when
    // replaced or when the loop dies. Three cells (sec. S4.4).
    // ===============================================================
    {
        // (i) the SAME object across two run() calls: ten ticks, zero
        // destructions in between.
        const int before = g_app_destructions;
        app *stored = new app;
        stored->id = 21;
        stored->stop_every = 5;
        glintfx::gltfx_rslt<void> set = loop.set_callbacks(
            glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(stored));
        glintfx::gltfx_rslt<void> first_run = set.has_error() ? set : loop.run();
        glintfx::gltfx_rslt<void> second_run = first_run.has_error() ? first_run : loop.run();
        const int destroyed = g_app_destructions - before;
        const bool cell_ok = !second_run.has_error() && destroyed == 0 && g_ticks_of_last_app == 10;
        check("posse_forma2_mesmo_objeto_em_dois_run", cell_ok,
              "10 tiques no MESMO objeto, destrutor 0x",
              to_text(g_ticks_of_last_app) + " tique(s), " + to_text(destroyed) +
                  " destruicao(oes)" +
                  (second_run.has_error() ? (", run=" + err_text(second_run.err()))
                                          : std::string(", run=ok")));
    }
    {
        // (ii) a loop that owns a stored context and simply goes out of
        // scope: the destruction is the facade's own `delete m_impl`.
        const int before = g_app_destructions;
        bool opened_ok = false;
        {
            glintfx::gltfx_rslt<glintfx::gltfx_loop> scoped_opened =
                glintfx::gltfx_loop::open(display, window, context);
            if (!scoped_opened.has_error()) {
                opened_ok = true;
                glintfx::gltfx_loop scoped = std::move(scoped_opened.value());
                app *stored = new app;
                stored->id = 22;
                stored->stop_every = 5;
                glintfx::gltfx_rslt<void> set = scoped.set_callbacks(
                    glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(stored));
                if (set.has_error()) {
                    opened_ok = false;
                }
            }
        }
        const int destroyed = g_app_destructions - before;
        const bool cell_ok = opened_ok && destroyed == 1 && g_last_destroyed_id == 22;
        check("posse_forma2_destruido_ao_sair_de_escopo", cell_ok, "destrutor 1x ao sair do escopo",
              to_text(destroyed) + " destruicao(oes), ultimo id=" + to_text(g_last_destroyed_id));
    }
    {
        // (iii) move assignment between two open loops, each owning its
        // own stored context: the destination destroys ITS OWN before
        // taking the source's, and the moved-from source destroys
        // nothing when it dies.
        const int before = g_app_destructions;
        int destroyed_at_move = -1;
        int id_at_move = 0;
        bool setup_ok = false;
        {
            glintfx::gltfx_rslt<glintfx::gltfx_loop> a_opened =
                glintfx::gltfx_loop::open(display, window, context);
            glintfx::gltfx_rslt<glintfx::gltfx_loop> b_opened =
                glintfx::gltfx_loop::open(display, window, context);
            if (!a_opened.has_error() && !b_opened.has_error()) {
                glintfx::gltfx_loop a = std::move(a_opened.value());
                glintfx::gltfx_loop b = std::move(b_opened.value());
                app *a_app = new app;
                a_app->id = 31;
                app *b_app = new app;
                b_app->id = 32;
                glintfx::gltfx_rslt<void> set_a = a.set_callbacks(
                    glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(a_app));
                glintfx::gltfx_rslt<void> set_b = b.set_callbacks(
                    glintfx::gltfx_adopt_loop_callbacks<&app::frame, &app::render>(b_app));
                setup_ok = !set_a.has_error() && !set_b.has_error();
                if (setup_ok) {
                    a = std::move(b);
                    destroyed_at_move = g_app_destructions - before;
                    id_at_move = g_last_destroyed_id;
                }
            }
        }
        const int destroyed_total = g_app_destructions - before;
        const bool cell_ok = setup_ok && destroyed_at_move == 1 && id_at_move == 31 &&
                             destroyed_total == 2 && g_last_destroyed_id == 32;
        check("posse_forma2_atribuicao_por_movimento", cell_ok,
              "1x no destino antes de receber, 0x no movido-de, 2x no total",
              to_text(destroyed_at_move) + " no movimento (id=" + to_text(id_at_move) + "), " +
                  to_text(destroyed_total) +
                  " no total (ultimo id=" + to_text(g_last_destroyed_id) + ")");
    }

    if (g_assertions_evaluated != k_planned_assertions) {
        std::fprintf(stderr,
                     "loop_parity_test: FAIL - %d assercoes avaliadas, %d planejadas (GODS_LAWS.md "
                     "L-40: piso de varredura nao-vazia)\n",
                     g_assertions_evaluated, k_planned_assertions);
        return EXIT_FAILURE;
    }
    if (g_assertions_failed > 0) {
        std::fprintf(stderr, "loop_parity_test: FAIL - %d de %d assercoes reprovaram\n",
                     g_assertions_failed, g_assertions_evaluated);
        return EXIT_FAILURE;
    }

    // No explicit close() anywhere: loop, context, window and display
    // are destroyed in reverse order of creation by ordinary scope
    // exit (GODS_LAWS.md L-22's own RAII shape, the same teardown
    // window_parity_test.cpp's own final comment already proves).
    return EXIT_SUCCESS;
}
