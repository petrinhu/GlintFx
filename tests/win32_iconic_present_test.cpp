// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <print>
#include <vector>

#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"
#include "platform/win32/wgl_context_adapter.hpp"
#include "platform/win32/window_adapter.hpp"

// win32_iconic_present_test.cpp - X-WGL (docs/plano-w6b-placa-e-laco.
// md fatia 4, D-W6b-6/18, GODS_LAWS.md L-04/L-09/L-40): proves, against
// the REAL windows-latest GitHub Actions runner and the Mesa/D3D12
// aparato this project's own CI installs (F3, docs/plano-w6b-placa-e-
// laco.md sec. 0), the ONE mechanism D-W6b-6 promises on this platform
// - IsIconic() before SwapBuffers(), the Win32 mirror of the Wayland
// side's own frame-callback budget (egl_context_adapter.cpp, fatia 3,
// "mecanismo diferente, efeito igual").
//
// win32_gl_context_adapter/win32_window_adapter/win32_display_adapter
// have no GLINTFX_API (deliberately - GODS_LAWS.md L-19, "nada e
// exportado"), so this fixture compiles all three a SECOND time,
// directly into its own object set - the SAME technique every other
// win32/*_test.cpp fixture in this suite already uses (tests/
// CMakeLists.txt's own win32_display_connect_test comment).
//
// This project has no Windows toolchain on the machine that wrote this
// file (GODS_LAWS.md L-27, the same declared limitation every other
// win32/ test in this suite already carries) - the windows CI job is
// the first place any of this ever actually runs.

namespace {

// THE INSTRUMENT THIS FILE'S EARLIER TWO REVISIONS WERE MISSING (team-
// lead briefing, 06/09/2026, on the second CI reproval by the same
// name): a case that measures six booleans and reprova (comment
// header above) yet never once printed WHY the failing gltfx_rslt held
// an error, leaving the next reader to guess. Prints all three
// diagnostic fields a gltfx_err can ever carry for a Win32
// platform_failure (core/err.hpp's own "DIAGNOSTIC CONTEXT" paragraph)
// - the error's own gltfx_err_code by NAME (never the bare integer),
// rejected_value() (WHICH Win32 call this adapter attributes the
// failure to), and os_error_code() (the raw ::GetLastError() that call
// reported - only just wired up, this fatia's own wgl_context_
// adapter.cpp, for every platform_failure this file's adapter can
// produce, the one gemeo every OTHER win32/ adapter already had,
// GODS_LAWS.md L-17). A no-op when `result` holds a value: called
// unconditionally, right before every has_error() check below, so a
// future failure - whichever check trips it - always prints first.
template <typename T>
void print_error_detail_if_failed(std::string_view label,
                                  const glintfx::gltfx_rslt<T> &result) noexcept {
    if (!result.has_error()) {
        return;
    }
    const glintfx::gltfx_err &err = result.err();
    std::println("MEASURED {}.error_code={}", label, glintfx::gltfx_err_code_name(err.code()));
    std::println("MEASURED {}.rejected_value={}", label, err.rejected_value());
    std::println("MEASURED {}.os_error_code={}", label, err.os_error_code());
}

[[nodiscard]] bool
open_display_and_window(glintfx::platform::win32_display_adapter &display,
                        glintfx::platform::win32_window_adapter &window) noexcept {
    if (display.open().has_error()) {
        return false;
    }
    glintfx::platform::win32_window_desc desc{};
    desc.logical_width = 320;
    desc.logical_height = 240;
    desc.title = "glintfx win32 iconic present test";
    return !window.open(display, desc).has_error();
}

// GL-CI-SOFTWARE TOLERANCE (team-lead briefing, 07/09/2026, decisao do
// lider por AskUserQuestion, "tolerar, mas so sob prova" - GODS_LAWS.md
// L-04/L-40): the windows-latest verification runner carries no real
// GPU, and its software renderer occasionally refuses to present a
// frame with the OS itself reporting NO cause at all (a failing
// ::SwapBuffers() with ::GetLastError()==0). A swap_buffers() failure
// downgrades from a hard reproval to a printed, counted DOWNGRADE ONLY
// when ALL THREE factors below hold at once - any other combination
// (a different error code/rejected_value, a nonzero os_error_code, a
// non-software gpu().kind, or NO other successful swap in this SAME
// execution) still reproves exactly as before. The third factor
// matters most: a swap that fails before any real swap has ever
// succeeded here is a broken path, never instability, and reproves
// even under a software renderer.
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
    // NEVER by renderer-name text (include/glintfx/platform/gl/gpu.hpp's
    // own header comment forbids it) - only the CLOSED `kind` type.
    if (gpu_kind != glintfx::gltfx_gpu_kind::software) {
        return false;
    }
    return any_other_swap_succeeded;
}

void print_swap_tolerance_downgrade(std::string_view label,
                                    const glintfx::gltfx_err &err) noexcept {
    std::println("DOWNGRADE: {} tolerada - error_code={} rejected_value={} os_error_code={}, "
                 "gpu().kind=software, com outra troca de quadro desta MESMA execucao ja "
                 "bem-sucedida antes desta. (a) o ambiente diverge porque esta maquina de "
                 "verificacao do Windows nao tem placa de video real - o renderizador por "
                 "software as vezes recusa apresentar um quadro sem o sistema operacional "
                 "relatar causa nenhuma (os_error_code=0). (b) quem prova o comportamento "
                 "real da biblioteca no lugar desta chave e' a prova em placa dedicada, "
                 "conduzida pelo orquestrador.",
                 label, glintfx::gltfx_err_code_name(err.code()), err.rejected_value(),
                 err.os_error_code());
}

// GL-CI-SOFTWARE TOLERANCE, DIAGNOSTIC ON REFUSAL (team-lead briefing,
// FACADE-PIN, 07/09/2026: "eu nao consigo saber POR QUE" - the case
// that reproves in gl_context_parity_test.cpp never printed the four
// booleans/values should_tolerate_swap_failure() actually decided on,
// so a future reprova gives no rastro of WHICH factor tripped it).
// Printed ONLY when the tolerance is about to REFUSE (right before the
// GLINTFX_CHECK below turns that refusal into a reproval) - the four
// fields the function above reads, PLUS the fifth (any_other_swap_
// succeeded) it also needs, all five as their OWN MEASURED line
// (tests/tools/collect_measured.py's own <owner>.<key>=<value> shape,
// exactly ONE dot) - deliberately NOT the {label}.{field} shape print_
// error_detail_if_failed above uses (that shape nests a SECOND dot
// inside `label`, so it never actually matches collect_measured.py's
// own MEASURED_LINE regex; this function's whole reason to exist is
// being collectible, so it does not repeat that shape).
void print_swap_tolerance_refusal(std::string_view site, const glintfx::gltfx_err &err,
                                  glintfx::gltfx_gpu_kind gpu_kind,
                                  bool any_other_swap_succeeded) noexcept {
    std::println("MEASURED win32_iconic_present_test.{}_refusal_error_code={}", site,
                 glintfx::gltfx_err_code_name(err.code()));
    std::println("MEASURED win32_iconic_present_test.{}_refusal_rejected_value={}", site,
                 err.rejected_value());
    std::println("MEASURED win32_iconic_present_test.{}_refusal_os_error_code={}", site,
                 err.os_error_code());
    std::println("MEASURED win32_iconic_present_test.{}_refusal_gpu_kind={}", site,
                 static_cast<int>(gpu_kind));
    std::println("MEASURED win32_iconic_present_test.{}_refusal_any_swap_succeeded={}", site,
                 any_other_swap_succeeded);
}

// GODS_LAWS.md L-40's own non-empty-sweep floor, applied to the
// `swap_tolerated_downgrades` counter ITSELF, across EVERY exit path
// of the GLINTFX_TEST body below - not just the happy one. GLINTFX_
// CHECK throws case_check_failed to unwind the CURRENT case the
// instant a check fails (harness/check.hpp's own header comment,
// "CASE-FATAL"); a print placed at the BOTTOM of the test body - the
// shape this file used to have - never ran when the should_tolerate_
// swap_failure() GLINTFX_CHECK reproved, because the throw unwound
// straight past it (team-lead briefing, FACADE-PIN, 07/09/2026: "essa
// linha NAO apareceu na execucao que reprovou"). A destructor runs on
// ANY scope exit - normal fall-through OR exception unwinding alike -
// so binding the print to one guarantees it fires EXACTLY once, on
// every path, without duplicating the print statement at each GLINTFX_
// CHECK call site. `count` is a reference to the test's own local
// `swap_tolerated_downgrades`, so the destructor always reads its
// FINAL value, whatever path got there.
struct swap_tolerated_downgrades_reporter {
    const int &count;
    ~swap_tolerated_downgrades_reporter() noexcept {
        std::println("MEASURED win32_iconic_present_test.swap_tolerated_downgrades={}", count);
    }
};

} // namespace

GLINTFX_TEST(win32_gl_context_swap_buffers_skips_iconic_window) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    const glintfx::gltfx_rslt<void> opened = context.open(window, options);
    std::println("MEASURED win32_iconic_present_test.context_open_ok={}", !opened.has_error());
    print_error_detail_if_failed("win32_iconic_present_test.context_open", opened);
    GLINTFX_CHECK(!opened.has_error());

    // The window was created WITHOUT WS_VISIBLE (D-W5-8/D-W5-13,
    // window_adapter.hpp's own header comment) - mapped here only
    // because THIS case needs a real, visible top-level window to
    // minimize/restore, never because open() itself ever calls
    // ShowWindow.
    ::ShowWindow(window.native_handle(), SW_SHOWNOACTIVATE);
    // pump_events() after EVERY ShowWindow() (display_adapter.hpp's
    // own header comment on pump_events(): "the non-blocking event
    // pump ARCH-PORTS's own consumer loop calls once per frame" - a
    // REAL consumer never goes from a window-state change straight to
    // GL work without pumping in between). ShowWindow()'s own
    // WM_WINDOWPOSCHANGED -> WM_SIZE/WM_MOVE chain is SENT (delivered
    // synchronously, window_message_route.hpp's own header comment),
    // but Windows also POSTS messages around a visibility/iconic
    // transition (WM_PAINT, WM_NCACTIVATE among them) that only reach
    // this window's own queue through a pump - this fixture is the
    // FIRST win32 test in this suite to call ShowWindow() at all, and
    // was the first to skip the pump every other consumer of this
    // adapter is documented to perform.
    GLINTFX_CHECK(!display.pump_events().has_error());

    // GL-CI-SOFTWARE TOLERANCE (team-lead briefing, 07/09/2026):
    // `any_swap_succeeded` tracks whether SOME OTHER frame swap of
    // THIS SAME execution already reached `presented` - the third
    // factor should_tolerate_swap_failure() requires before ever
    // downgrading a swap_buffers() failure below.
    bool any_swap_succeeded = false;
    int swap_tolerated_downgrades = 0;
    // See swap_tolerated_downgrades_reporter's own header comment above:
    // this print MUST survive every GLINTFX_CHECK reproval below, so it
    // is bound to a destructor instead of sitting at the bottom of this
    // function body.
    const swap_tolerated_downgrades_reporter downgrades_reporter{swap_tolerated_downgrades};

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> presented_before_minimize =
        context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.presented_before_minimize",
                                 presented_before_minimize);
    if (presented_before_minimize.has_error()) {
        const bool tolerated = should_tolerate_swap_failure(presented_before_minimize.err(),
                                                            context.gpu().kind, any_swap_succeeded);
        if (!tolerated) {
            print_swap_tolerance_refusal("presented_before_minimize",
                                         presented_before_minimize.err(), context.gpu().kind,
                                         any_swap_succeeded);
        }
        GLINTFX_CHECK(tolerated);
        print_swap_tolerance_downgrade("win32_iconic_present_test.presented_before_minimize",
                                       presented_before_minimize.err());
        ++swap_tolerated_downgrades;
    } else {
        const bool presented_before =
            presented_before_minimize.value() == glintfx::gltfx_present_outcome::presented;
        std::println("MEASURED win32_iconic_present_test.presented_before_minimize={}",
                     presented_before);
        GLINTFX_CHECK(presented_before);
        any_swap_succeeded = true;
    }
    const std::uint32_t swaps_before_minimize = context.swap_calls_issued();

    ::ShowWindow(window.native_handle(), SW_MINIMIZE);
    GLINTFX_CHECK(!display.pump_events().has_error()); // same reasoning as the pump above
    const BOOL is_iconic_now = ::IsIconic(window.native_handle());
    std::println("MEASURED win32_iconic_present_test.is_iconic_after_minimize={}",
                 is_iconic_now != 0);
    GLINTFX_CHECK(is_iconic_now != 0);

    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> while_hidden = context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.while_hidden", while_hidden);
    GLINTFX_CHECK(!while_hidden.has_error());
    const bool skipped_hidden =
        while_hidden.value() == glintfx::gltfx_present_outcome::skipped_hidden;
    std::println("MEASURED win32_iconic_present_test.skipped_while_iconic={}", skipped_hidden);
    GLINTFX_CHECK(skipped_hidden);
    // THE FACT NO RETURN VALUE ALONE COULD PROVE (this file's own
    // header comment): swap_buffers() must NEVER have actually called
    // ::SwapBuffers() while iconic - a mutant that returns
    // skipped_hidden but swaps anyway would still pass the assertion
    // above.
    GLINTFX_CHECK(context.swap_calls_issued() == swaps_before_minimize);

    ::ShowWindow(window.native_handle(), SW_RESTORE);
    // SECOND CI REPROVAL BY THIS SAME NAME (team-lead briefing,
    // 06/09/2026, GODS_LAWS.md L-42's own "segunda reprovacao obriga
    // busca antes da terceira"): the pump added below (the fix the
    // FIRST reproval's own comment, preserved above unedited, argued
    // for) did NOT close the gap - the real windows-latest runner
    // still measured `!after_restore.has_error()` FALSE with the pump
    // in place. Two hypotheses remained on the table, restated from
    // the first reproval: a real WGL adapter defect on restore (fix
    // the product), or the headless runner genuinely needing more than
    // one pump cycle to finish delivering the SW_RESTORE transition
    // before a present can succeed (Chromium's own message_pump_win.cc
    // loops PeekMessage more than once per iteration for exactly this
    // class of "one pump is not always enough" reasoning - the same
    // busca this fatia's own team-lead ordered before this attempt).
    // NEITHER hypothesis is asserted here as the answer: the code
    // path in win32_gl_context_adapter (src/platform/win32/wgl_
    // context_adapter.cpp) was re-read end to end for this reproval
    // and still shows no WM_SIZE/WM_ACTIVATE handling and no path that
    // could leave a stale DC/context across a minimize cycle (m_dc is
    // CS_OWNDC, display_adapter.cpp's own class registration - not the
    // shared/common DC Windows can silently invalidate), which argues
    // AGAINST a product defect without proving the environment either.
    // print_error_detail_if_failed() below is what turns "it failed
    // again" into an actual verdict on the NEXT run: gltfx_err_code,
    // WHICH Win32 call this adapter blames (rejected_value()), and the
    // raw ::GetLastError() that call reported (os_error_code(), only
    // just wired up by this same reproval - wgl_context_adapter.cpp's
    // swap_buffers() was the one platform_failure site in this file
    // that never attached it, GODS_LAWS.md L-17's own gemeo). This
    // project has no Windows toolchain to reproduce either hypothesis
    // locally (GODS_LAWS.md L-27) - the windows-latest job is still the
    // only place that can settle which one this is, but it will now do
    // so with a code and a GetLastError() printed, not a bare
    // pass/fail.
    GLINTFX_CHECK(!display.pump_events().has_error());
    const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> after_restore =
        context.swap_buffers();
    print_error_detail_if_failed("win32_iconic_present_test.after_restore", after_restore);
    if (after_restore.has_error()) {
        // GL-CI-SOFTWARE TOLERANCE (team-lead briefing, 07/09/2026,
        // decisao do lider por AskUserQuestion): THIS is the exact
        // failure the tolerance exists for - a GPU-less Windows CI
        // runner refusing to present after a minimize/restore cycle,
        // ::GetLastError() reporting nothing. Tolerated ONLY when
        // should_tolerate_swap_failure() holds; any other shape of
        // failure still reproves via the GLINTFX_CHECK below, exactly
        // as before this tolerance existed.
        const bool tolerated = should_tolerate_swap_failure(after_restore.err(), context.gpu().kind,
                                                            any_swap_succeeded);
        if (!tolerated) {
            print_swap_tolerance_refusal("after_restore", after_restore.err(), context.gpu().kind,
                                         any_swap_succeeded);
        }
        GLINTFX_CHECK(tolerated);
        print_swap_tolerance_downgrade("win32_iconic_present_test.after_restore",
                                       after_restore.err());
        ++swap_tolerated_downgrades;
        // Tolerated means this swap proves neither `presented` nor
        // `swap_calls_issued() == swaps_before_minimize + 1` - the
        // DOWNGRADE line above already names, in (b), what covers the
        // library's real behaviour in this key's place instead.
    } else {
        const bool presented_after_restore =
            after_restore.value() == glintfx::gltfx_present_outcome::presented;
        std::println("MEASURED win32_iconic_present_test.presented_after_restore={}",
                     presented_after_restore);
        GLINTFX_CHECK(presented_after_restore);
        GLINTFX_CHECK(context.swap_calls_issued() == swaps_before_minimize + 1);
    }

    // swap_tolerated_downgrades itself is printed by downgrades_
    // reporter's destructor above (fires on this normal fall-through
    // AND on any GLINTFX_CHECK reproval earlier in this function),
    // never by a print sitting here - see its own header comment.
}

// D-W6b-18: v-sync `adaptive` on Windows is honored (wglSwapIntervalEXT
// (-1)) ONLY when this system's own WGL_EXT_swap_control_tear is
// present, and refused BY NAME otherwise - never silently downgraded
// to plain `on`, and the mirror of the Wayland side's own UNCONDITIONAL
// refusal (Wayland/EGL has no equivalent extension at all, ever).
// Whether the Mesa/D3D12 aparato this project's own CI installs
// actually advertises the extension is a MEASURED, driver-dependent
// fact (GODS_LAWS.md L-44) - printed BEFORE it decides which branch
// this case asserts, never assumed either way.
GLINTFX_TEST(win32_gl_context_adaptive_vsync_matches_driver_capability) {
    glintfx::platform::win32_display_adapter display;
    glintfx::platform::win32_window_adapter window;
    GLINTFX_CHECK(open_display_and_window(display, window));

    glintfx::platform::win32_gl_context_adapter context;
    const std::vector<glintfx::gltfx_gfx_option_entry> options;
    GLINTFX_CHECK(!context.open(window, options).has_error());

    const glintfx::gltfx_gfx_option_support vsync_support =
        context.option_support(glintfx::gltfx_gfx_option::vsync);
    const bool vsync_id_supported = vsync_support == glintfx::gltfx_gfx_option_support::supported;
    std::println("MEASURED win32_iconic_present_test.vsync_option_support_supported={}",
                 vsync_id_supported);
    // vsync itself (on/off) is supported on every driver this project
    // targets in practice - only `adaptive`, a specific VALUE, may be
    // refused (D-W6b-16's own id-vs-value distinction, egl_context_
    // adapter.cpp's own option_support() documents the identical
    // shape one directory over).
    GLINTFX_CHECK(vsync_id_supported);

    const glintfx::gltfx_rslt<void> adaptive =
        context.apply_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 2});
    const bool adaptive_accepted = !adaptive.has_error();
    std::println("MEASURED win32_iconic_present_test.vsync_adaptive_accepted={}",
                 adaptive_accepted);

    if (!adaptive_accepted) {
        // D-W6b-18: refused BY NAME, never silently downgraded to `on`.
        GLINTFX_CHECK(adaptive.err().code() == glintfx::gltfx_err_code::unsupported);
        GLINTFX_CHECK(adaptive.err().rejected_value() == "vsync");
    }
    // adaptive_accepted == true needs no further assertion: D-W6b-18's own success case IS
    // "apply_option() returned without error" - the same fact already MEASURED and printed above,
    // and re-asserting !adaptive.has_error() inside `if (adaptive_accepted)` would be dead code by
    // construction (cppcheck's own oppositeInnerCondition, caught live in this fatia's own preci.sh
    // run: the branch condition already guarantees it).
}

#endif // defined(_WIN32)
