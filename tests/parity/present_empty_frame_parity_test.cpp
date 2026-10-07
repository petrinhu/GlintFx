// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// present_empty_frame_parity_test.cpp - PRESENT-EMPTY-FRAME (W8, plan of 07/10/2026, D-W8-50 to
// D-W8-54; GODS_LAWS.md L-04): ONE file, ONLY the public API, NO #if of any
// kind - the same family as gl_context_parity_test.cpp and draw2d_parity_test.cpp. The Windows side
// links this file into an ordinary add_executable()/add_test() target (tests/CMakeLists.txt,
// if(WIN32)); the Linux side stages and runs it as a container fixture of this EXACT name.
//
// THE CONTRACT UNDER TEST (include/glintfx/platform/gl/context.hpp, item 2 of the one-way-door
// list, text added by the next slice of the same item): a frame presented with NOTHING drawn since
// the previous presentation has UNDEFINED content, and the system MAY refuse it. A refusal is an
// ordinary error (code platform_failure, rejected_value() a non-empty token, never a third present
// outcome and never `presented`), the context stays usable, and the next frame that draws presents
// normally. Whether a system refuses is the driver's decision: a Windows Mesa from 26.0 on does,
// the Linux llvmpipe does not.
//
// THE CRITERION IS FIXED HERE, BEFORE THE DATA EXISTS (GODS_LAWS.md L-43). Three kinds of line:
//   - ASSERTED, equal on both systems: the verdict function over its WHOLE closed space (P0, 12
//     cells), the first frame of every cell is `presented`, the frame that draws after an empty one
//     is `presented` (recovery), the loop keeps asking for frames after the empty one, and an empty
//     frame is never `skipped` and never any other kind of error.
//   - ASSERTED against the expectation of the APPARATUS (GLINTFX_EMPTY_FRAME_EXPECT): declared by
//     the preparation of each system (tools/ci/install-mesa-opengl32.ps1 on Windows, the container
//     step of .github/workflows/ci.yml on Linux), never by this file. Outside CI it is absent and
//     both `presented` and `refused` pass. Inside CI (GITHUB_ACTIONS defined) an absent
//     expectation REPROVES: the oracle that keeps a refusal from being masked must not be optional.
//   - MEASURED, printed, never a criterion: draw2d_empty. A 2D frame with clear_color = nullopt and
//     no piece issues no draw command, so it may be an empty frame too. The expectation of the
//     apparatus does NOT apply to it; the value is data for a later decision.
//
// WHY vsync IS TURNED OFF IN EVERY CONTEXT: the kwin --virtual of the container never returns the
// EGL frame callback (tests/measured_exceptions.txt, loop_parity_test.cap30_wall_ms), so with vsync
// on the second swap may become `skipped_hidden` and never reach the driver, and the cell would
// measure nothing.
//
// GL FUNCTIONS: glClear/glClearColor are declared by hand and resolved through proc_address() of
// the context of EACH cell (the table is per context on WGL), never linked (GODS_LAWS.md L-07).

namespace {

using gl_clear_color_fn = void (*)(float, float, float, float);
using gl_clear_fn = void (*)(unsigned int);
constexpr unsigned int k_gl_color_buffer_bit = 0x00004000;

enum class frame_class { presented, refused, skipped, other };
enum class expected_class { none, presented, refused };

int g_checks = 0;
int g_failures = 0;

[[nodiscard]] const char *class_name(frame_class observed) noexcept {
    switch (observed) {
    case frame_class::presented:
        return "presented";
    case frame_class::refused:
        return "refused";
    case frame_class::skipped:
        return "skipped";
    case frame_class::other:
        return "other";
    }
    return "other";
}

[[nodiscard]] const char *expected_name(expected_class expected) noexcept {
    switch (expected) {
    case expected_class::none:
        return "none";
    case expected_class::presented:
        return "presented";
    case expected_class::refused:
        return "refused";
    }
    return "none";
}

// Every assertion goes through here: counted, printed with its observed value, never silent.
// Returns the condition so a cell can fold it into its own verdict.
bool check(const char *name, bool condition, const std::string &observed) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
    }
    std::fprintf(stdout, "present_empty_frame_parity_test: %s=%s %s\n", name, observed.c_str(),
                 condition ? "OK" : "FALHOU");
    return condition;
}

[[nodiscard]] std::string
describe(const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &swap) {
    if (swap.has_value()) {
        return swap.value() == glintfx::gltfx_present_outcome::presented ? "presented"
                                                                         : "skipped_hidden";
    }
    const glintfx::gltfx_err &error = swap.err();
    return std::string(glintfx::gltfx_err_code_name(error.code())) + "/" +
           std::string(error.rejected_value()) +
           "/os_error_code=" + std::to_string(error.os_error_code());
}

// The classification of ONE swap. `refused` is exactly a platform_failure that names the system
// call that refused (docs/api-conventions.md R7: a token); any other error, and any outcome that is
// not one of the two known ones, is `other`.
[[nodiscard]] frame_class
classify(const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &swap) {
    if (swap.has_value()) {
        switch (swap.value()) {
        case glintfx::gltfx_present_outcome::presented:
            return frame_class::presented;
        case glintfx::gltfx_present_outcome::skipped_hidden:
            return frame_class::skipped;
        }
        return frame_class::other;
    }
    const glintfx::gltfx_err &error = swap.err();
    const bool refused = error.code() == glintfx::gltfx_err_code::platform_failure &&
                         !error.rejected_value().empty();
    return refused ? frame_class::refused : frame_class::other;
}

// The verdict on an EMPTY frame. Only `presented` and `refused` can ever pass; with an expectation
// declared, only the equal class does. `skipped` (the frame never reached the system) and `other`
// (a wrong kind of error) reprove under every expectation.
[[nodiscard]] bool empty_frame_verdict(frame_class observed, expected_class expected) noexcept {
    if (observed != frame_class::presented && observed != frame_class::refused) {
        return false;
    }
    switch (expected) {
    case expected_class::none:
        return true;
    case expected_class::presented:
        return observed == frame_class::presented;
    case expected_class::refused:
        return observed == frame_class::refused;
    }
    return false;
}

// P0 - the ruler (GODS_LAWS.md L-40, item 5: the space is small and closed, so it is ENUMERATED,
// not searched). The expected table is written out literally, row by row, 4 classes x 3
// expectations.
struct ruler_row {
    frame_class observed;
    expected_class expected;
    bool accepted;
};

constexpr std::size_t k_class_count = 4;
constexpr std::size_t k_expectation_count = 3;
constexpr std::size_t k_ruler_cells = k_class_count * k_expectation_count;

constexpr std::array<ruler_row, k_ruler_cells> k_ruler = {{
    {frame_class::presented, expected_class::none, true},
    {frame_class::presented, expected_class::presented, true},
    {frame_class::presented, expected_class::refused, false},
    {frame_class::refused, expected_class::none, true},
    {frame_class::refused, expected_class::presented, false},
    {frame_class::refused, expected_class::refused, true},
    {frame_class::skipped, expected_class::none, false},
    {frame_class::skipped, expected_class::presented, false},
    {frame_class::skipped, expected_class::refused, false},
    {frame_class::other, expected_class::none, false},
    {frame_class::other, expected_class::presented, false},
    {frame_class::other, expected_class::refused, false},
}};

[[nodiscard]] const ruler_row *find_ruler_row(frame_class observed,
                                              expected_class expected) noexcept {
    for (const ruler_row &row : k_ruler) {
        if (row.observed == observed && row.expected == expected) {
            return &row;
        }
    }
    return nullptr;
}

bool cell_ruler() {
    constexpr std::array<frame_class, k_class_count> classes = {
        frame_class::presented, frame_class::refused, frame_class::skipped, frame_class::other};
    constexpr std::array<expected_class, k_expectation_count> expectations = {
        expected_class::none, expected_class::presented, expected_class::refused};
    std::size_t enumerated = 0;
    std::size_t wrong = 0;
    for (const frame_class observed : classes) {
        for (const expected_class expected : expectations) {
            ++enumerated;
            const ruler_row *row = find_ruler_row(observed, expected);
            if (row == nullptr || empty_frame_verdict(observed, expected) != row->accepted) {
                ++wrong;
                std::fprintf(stderr, "present_empty_frame_parity_test: regua errada em (%s, %s)\n",
                             class_name(observed), expected_name(expected));
            }
        }
    }
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.regua_celulas=%zu\n",
                 enumerated);
    const std::string observed_text =
        std::to_string(enumerated) + " celulas, " + std::to_string(wrong) + " erradas";
    return check("regua_celulas", enumerated == k_ruler_cells && wrong == 0, observed_text);
}

// The expectation of the apparatus. False means the environment is wrong, and the reason is
// printed.
[[nodiscard]] bool resolve_expectation(expected_class &expected) {
    const char *declared = std::getenv("GLINTFX_EMPTY_FRAME_EXPECT");
    const char *in_ci = std::getenv("GITHUB_ACTIONS");
    const bool has_expectation = declared != nullptr && declared[0] != '\0';
    const bool is_ci = in_ci != nullptr && in_ci[0] != '\0';
    if (!has_expectation) {
        if (is_ci) {
            std::fprintf(stderr,
                         "present_empty_frame_parity_test: FALHOU aparato de CI sem expectativa "
                         "declarada (GLINTFX_EMPTY_FRAME_EXPECT ausente com GITHUB_ACTIONS "
                         "definida; GODS_LAWS.md L-40)\n");
            return false;
        }
        expected = expected_class::none;
        return true;
    }
    const std::string_view text{declared};
    if (text == "presented") {
        expected = expected_class::presented;
        return true;
    }
    if (text == "refused") {
        expected = expected_class::refused;
        return true;
    }
    std::fprintf(stderr,
                 "present_empty_frame_parity_test: FALHOU GLINTFX_EMPTY_FRAME_EXPECT=%s, esperado "
                 "`presented` ou `refused`\n",
                 declared);
    return false;
}

// A window and its context for ONE cell, built in place and never moved (the renderer and the loop
// point at them).
struct gl_session {
    std::optional<glintfx::gltfx_window> window;
    std::optional<glintfx::gltfx_gl_context> context;
    gl_clear_color_fn clear_color = nullptr;
    gl_clear_fn clear = nullptr;
};

void print_open_failure(const char *what, const glintfx::gltfx_err &error) {
    std::fprintf(stderr, "present_empty_frame_parity_test: FALHOU %s: %s/%s\n", what,
                 std::string(glintfx::gltfx_err_code_name(error.code())).c_str(),
                 std::string(error.rejected_value()).c_str());
}

[[nodiscard]] bool resolve_gl_functions(gl_session &session) {
    void *clear_color_addr = session.context->proc_address("glClearColor");
    void *clear_addr = session.context->proc_address("glClear");
    if (clear_color_addr == nullptr || clear_addr == nullptr) {
        std::fprintf(stderr,
                     "present_empty_frame_parity_test: FALHOU proc_address glClearColor=%d "
                     "glClear=%d\n",
                     clear_color_addr != nullptr, clear_addr != nullptr);
        return false;
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal dlsym-style
    // function-to-object-pointer cast every GL loader already relies on (the sibling parity tests
    // carry the same markers for the same lines).
    session.clear_color = reinterpret_cast<gl_clear_color_fn>(clear_color_addr);
    session.clear = reinterpret_cast<gl_clear_fn>(clear_addr);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above
    return true;
}

[[nodiscard]] bool prepare_context(gl_session &session) {
    if (const auto current = session.context->make_current(); current.has_error()) {
        print_open_failure("make_current", current.err());
        return false;
    }
    const auto vsync_off =
        session.context->set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
    if (vsync_off.has_error()) {
        print_open_failure("set_option(vsync=0)", vsync_off.err());
        return false;
    }
    return resolve_gl_functions(session);
}

[[nodiscard]] bool open_session(glintfx::gltfx_display &display, gl_session &session,
                                const char *title) {
    const glintfx::gltfx_window_desc window_desc{
        .title = title,
        .application_id = "org.glintfx.present_empty_frame_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    auto window_opened = glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        print_open_failure("gltfx_window::open", window_opened.err());
        return false;
    }
    session.window.emplace(std::move(window_opened.value()));
    auto context_opened =
        glintfx::gltfx_gl_context::open(*session.window, glintfx::gltfx_gl_context_desc{});
    if (context_opened.has_error()) {
        print_open_failure("gltfx_gl_context::open", context_opened.err());
        return false;
    }
    session.context.emplace(std::move(context_opened.value()));
    return prepare_context(session);
}

// One GL drawing command: enough to make the frame a frame that draws.
void draw_clear(const gl_session &session) {
    session.clear_color(0.2F, 0.4F, 0.6F, 1.0F);
    session.clear(k_gl_color_buffer_bit);
}

// Classifies an EMPTY swap, prints its class as MEASURED and applies the verdict.
bool report_empty(const char *key, const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &swap,
                  expected_class expected) {
    const frame_class observed = classify(swap);
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.%s=%s\n", key,
                 class_name(observed));
    const std::string observed_text = std::string(class_name(observed)) + " (" + describe(swap) +
                                      "), expectativa " + expected_name(expected);
    if (observed == frame_class::skipped) {
        std::fprintf(stderr,
                     "present_empty_frame_parity_test: %s: o quadro vazio nao chegou ao sistema\n",
                     key);
    }
    return check(key, empty_frame_verdict(observed, expected), observed_text);
}

// A swap that must present (the first frame of a cell and every recovery).
bool report_presented(const char *name,
                      const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> &swap) {
    return check(name, classify(swap) == frame_class::presented, describe(swap));
}

bool cell_context(glintfx::gltfx_display &display, expected_class expected) {
    gl_session session;
    if (!open_session(display, session, "quadro vazio no contexto")) {
        return false;
    }
    draw_clear(session);
    bool ok = report_presented("context_first_presented", session.context->swap_buffers());
    ok = report_empty("context_empty_1", session.context->swap_buffers(), expected) && ok;
    ok = report_empty("context_empty_2", session.context->swap_buffers(), expected) && ok;
    draw_clear(session);
    const auto recovery = session.context->swap_buffers();
    const bool recovered = classify(recovery) == frame_class::presented;
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.context_recovery_presented=%d\n",
                 recovered ? 1 : 0);
    return check("context_recovery_presented", recovered, describe(recovery)) && ok;
}

// step() that must ask for a frame. nullopt: step() failed (printed). Otherwise should_render.
[[nodiscard]] std::optional<bool> step_should_render(glintfx::gltfx_loop &loop, const char *what) {
    auto ticked = loop.step();
    if (ticked.has_error()) {
        print_open_failure(what, ticked.err());
        return std::nullopt;
    }
    return ticked.value().should_render;
}

[[nodiscard]] bool loop_asks_for_frame(glintfx::gltfx_loop &loop, const char *name) {
    const std::optional<bool> asks = step_should_render(loop, name);
    return check(name, asks.value_or(false), asks.has_value() ? "should_render" : "step falhou");
}

bool cell_loop_body(glintfx::gltfx_loop &loop, const gl_session &session, expected_class expected) {
    if (!loop_asks_for_frame(loop, "loop_step_1_asks_frame")) {
        return false;
    }
    draw_clear(session);
    bool ok = report_presented("loop_first_presented", loop.present());
    if (!loop_asks_for_frame(loop, "loop_step_2_asks_frame")) {
        return false;
    }
    ok = report_empty("loop_empty", loop.present(), expected) && ok;
    const std::optional<bool> asks = step_should_render(loop, "loop_step_3");
    const bool asks_again = asks.value_or(false);
    std::fprintf(stdout,
                 "MEASURED present_empty_frame_parity_test.loop_should_render_after_refusal=%d\n",
                 asks_again ? 1 : 0);
    ok = check("loop_should_render_after_refusal", asks_again, asks_again ? "1" : "0") && ok;
    if (!asks_again) {
        return false;
    }
    draw_clear(session);
    const auto recovery = loop.present();
    const bool recovered = classify(recovery) == frame_class::presented;
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.loop_recovery_presented=%d\n",
                 recovered ? 1 : 0);
    return check("loop_recovery_presented", recovered, describe(recovery)) && ok;
}

bool cell_loop(glintfx::gltfx_display &display, expected_class expected) {
    gl_session session;
    if (!open_session(display, session, "quadro vazio no laco")) {
        return false;
    }
    auto loop_opened = glintfx::gltfx_loop::open(display, *session.window, *session.context);
    if (loop_opened.has_error()) {
        print_open_failure("gltfx_loop::open", loop_opened.err());
        return false;
    }
    glintfx::gltfx_loop loop = std::move(loop_opened.value());
    return cell_loop_body(loop, session, expected);
}

// P4 - a 2D frame with no clear and no piece. MEASURED, never a criterion by value: only `skipped`
// and `other` reprove, and the expectation of the apparatus does not apply (the 2D renderer may or
// may not issue a draw command for such a frame).
[[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>
draw2d_frame(gl_session &session, glintfx::gltfx_renderer_2d &renderer,
             const glintfx::gltfx_frame_2d_desc &desc, const char *label) {
    renderer.begin_frame(desc);
    const auto finished = renderer.finish_frame();
    check(label, finished.has_value(),
          finished.has_value() ? "finish_frame ok" : "finish_frame falhou");
    return session.context->swap_buffers();
}

bool cell_draw2d(glintfx::gltfx_display &display) {
    gl_session session;
    if (!open_session(display, session, "quadro vazio no desenho 2D")) {
        return false;
    }
    auto renderer_opened =
        glintfx::gltfx_renderer_2d::open(*session.context, glintfx::gltfx_renderer_2d_desc{});
    if (renderer_opened.has_error()) {
        print_open_failure("gltfx_renderer_2d::open", renderer_opened.err());
        return false;
    }
    glintfx::gltfx_renderer_2d renderer = std::move(renderer_opened.value());
    bool ok = report_presented(
        "draw2d_first_presented",
        draw2d_frame(session, renderer, glintfx::gltfx_frame_2d_desc{}, "draw2d_finish_1"));
    const auto empty = draw2d_frame(session, renderer, glintfx::gltfx_frame_2d_desc{std::nullopt},
                                    "draw2d_finish_2");
    const frame_class observed = classify(empty);
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.draw2d_empty=%s\n",
                 class_name(observed));
    ok = check("draw2d_empty",
               observed == frame_class::presented || observed == frame_class::refused,
               std::string(class_name(observed)) + " (" + describe(empty) + ")") &&
         ok;
    const glintfx::gltfx_frame_2d_desc colored{glintfx::gltfx_rgba{0.0F, 1.0F, 0.0F, 1.0F}};
    return report_presented("draw2d_recovery_presented",
                            draw2d_frame(session, renderer, colored, "draw2d_finish_3")) &&
           ok;
}

} // namespace

int main() {
    // Unbuffered stdout: the same fix every fixture of this family applies (_IONBF is the one mode
    // that really disables buffering with MSVC).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    expected_class expected = expected_class::none;
    if (!resolve_expectation(expected)) {
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "MEASURED present_empty_frame_parity_test.expectation=%s\n",
                 expected_name(expected));

    bool ok = cell_ruler();

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        print_open_failure("gltfx_display::open", display_opened.err());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    ok = cell_context(display, expected) && ok;
    ok = cell_loop(display, expected) && ok;
    ok = cell_draw2d(display) && ok;

    std::fprintf(stdout, "present_empty_frame_parity_test: %d verificacoes, %d falhas\n", g_checks,
                 g_failures);
    if (g_checks == 0) {
        std::fprintf(stderr, "present_empty_frame_parity_test: FALHOU zero verificacoes avaliadas "
                             "(GODS_LAWS.md L-40, piso de varredura nao-vazia)\n");
        return EXIT_FAILURE;
    }
    return (ok && g_failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
