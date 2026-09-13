// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>
#include <type_traits>
#include <utility>

#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/loop/loop_context_mark.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// loop_context_mark_test.cpp - LOOP-CONTEXT-MARK (S1d, /var/tmp/
// glintfx-plan/loop-fix.md sec. 3.4/S1d, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-40): proves camada 4's own sec. 3.4 items 1-3 - BOTH halves
// of the Debug/Release split run in the SAME translation unit, under
// `#ifdef NDEBUG`, with the count of what each half checked printed
// in both (never a test conditioned on a mode it never proves it ran
// under, GODS_LAWS.md L-40). Item 4 (the debug-only guard actually
// FIRING, in a real subprocess) is proved separately, by
// tests/tools/check_loop_mark_precondition.py - this file proves the
// three properties observable INSIDE one process without ever
// touching a dead object across a translation-unit boundary.
//
// PURE HEADER TEST, NO .cpp DEPENDENCY, NO OPERATING SYSTEM (registers
// unguarded on all five platforms, the same shape loop_bind_test.cpp
// one directory over already uses): platform/loop/loop_context_mark.
// hpp and loop_bind.hpp are both entirely inline/template.

using glintfx::gltfx_bind_loop_callbacks;
using glintfx::gltfx_frame_tick;
using glintfx::gltfx_loop_callbacks;
using glintfx::gltfx_loop_context_mark;
#ifndef NDEBUG
using glintfx::gltfx_loop_context_mark_is_live;
#endif

namespace {

// marked_app - opts in by inheriting from gltfx_loop_context_mark, the
// only thing a consumer has to do to get camada 4's own liveness
// check (loop_context_mark.hpp's own "THE EFFECT, IN ONE SENTENCE").
struct marked_app : gltfx_loop_context_mark {
    int frame_calls = 0;
    int render_calls = 0;

    bool frame(const gltfx_frame_tick &) noexcept {
        ++frame_calls;
        return true;
    }
    void render(const gltfx_frame_tick &) noexcept { ++render_calls; }
};

// unmarked_app - the CONTROL: an ordinary object that never inherits
// from gltfx_loop_context_mark. loop_bind.hpp's own `if constexpr`
// branch does not exist for this type at all - binding and calling
// through it must behave EXACTLY as it did before camada 4 existed
// (tests/loop_bind_test.cpp's own fixture app already proves this in
// full; this one case here is only the "still true after camada 4
// landed" regression check).
struct unmarked_app {
    int frame_calls = 0;
    int render_calls = 0;

    bool frame(const gltfx_frame_tick &) noexcept {
        ++frame_calls;
        return true;
    }
    void render(const gltfx_frame_tick &) noexcept { ++render_calls; }
};

} // namespace

// --- sec. 3.4 item 2: custo zero em Release, provado por tamanho (F19)
// - and its Debug-side mirror: the mark word must actually exist ---

#ifdef NDEBUG
static_assert(sizeof(marked_app) == sizeof(unmarked_app),
              "in a build with NDEBUG defined, inheriting from gltfx_loop_context_mark must "
              "cost EXACTLY ZERO - the empty-base-class optimization must make a marked "
              "object's size identical to an unmarked one (F19)");
static_assert(std::is_empty_v<gltfx_loop_context_mark>,
              "gltfx_loop_context_mark must be an empty class when NDEBUG is defined");
#else
static_assert(sizeof(marked_app) > sizeof(unmarked_app),
              "in a build with NDEBUG undefined, the mark word must actually exist - a "
              "marked object must be strictly LARGER than an unmarked one");
#endif

// --- sec. 3.4 item 3: nenhum comportamento a mais em Release, provado
// por execucao - identical in BOTH modes: binding and calling through
// a marked object works exactly like an unmarked one when the object
// is alive, in every build ---

GLINTFX_TEST(marked_object_binds_and_calls_through_identically_in_both_modes) {
    marked_app app{};
    const gltfx_loop_callbacks cb =
        gltfx_bind_loop_callbacks<&marked_app::frame, &marked_app::render>(app);
    const gltfx_frame_tick tick{};

    GLINTFX_CHECK(cb.on_frame(cb.context, tick));
    GLINTFX_CHECK_EQ(app.frame_calls, 1);
    cb.on_render(cb.context, tick);
    GLINTFX_CHECK_EQ(app.render_calls, 1);
}

GLINTFX_TEST(unmarked_object_binds_and_calls_through_camada_4_never_involved) {
    unmarked_app app{};
    const gltfx_loop_callbacks cb =
        gltfx_bind_loop_callbacks<&unmarked_app::frame, &unmarked_app::render>(app);
    const gltfx_frame_tick tick{};

    GLINTFX_CHECK(cb.on_frame(cb.context, tick));
    GLINTFX_CHECK_EQ(app.frame_calls, 1);
    cb.on_render(cb.context, tick);
    GLINTFX_CHECK_EQ(app.render_calls, 1);
}

#ifndef NDEBUG

// --- sec. 3.4 item 1, Debug-only observable properties: the mark
// itself exists only in this build, so these two cases only exist
// under #ifndef NDEBUG - the SAME reason gltfx_loop_context_mark_is_
// live() itself is only declared here (loop_context_mark.hpp) ---

GLINTFX_TEST(copy_of_a_live_object_is_live) {
    marked_app original{};
    marked_app copy = original; // NOLINT - reason: copy is the point of this case
    GLINTFX_CHECK(gltfx_loop_context_mark_is_live(original));
    GLINTFX_CHECK(gltfx_loop_context_mark_is_live(copy));
}

GLINTFX_TEST(moved_from_object_stays_alive_and_marked) {
    marked_app original{};
    // loop_context_mark.hpp's own "WHAT THIS LAYER DOES NOT SOLVE": a
    // move never runs the source's destructor, so the moved-from
    // object stays alive and marked - this is DECLARED behavior, not
    // an accident this case merely observes.
    marked_app moved_to = std::move(original);
    // cppcheck-suppress accessMoved ; reason: accessing the moved-from
    // object is this case's own point (see the comment block above) -
    // not an accident cppcheck's generic heuristic understands.
    GLINTFX_CHECK(gltfx_loop_context_mark_is_live(original));
    GLINTFX_CHECK(gltfx_loop_context_mark_is_live(moved_to));
}

#endif

GLINTFX_TEST(loop_context_mark_table_is_enumerated_in_full) {
#ifdef NDEBUG
    constexpr int static_asserts_checked = 2; // sizeof equal, is_empty
    constexpr int runtime_cases_checked = 2;  // marked binds, unmarked binds
    constexpr const char *build_mode = "NDEBUG defined (Release)";
#else
    constexpr int static_asserts_checked = 1; // sizeof strictly greater
    constexpr int runtime_cases_checked =
        4; // marked binds, unmarked binds, copy-is-live, moved-from-is-live
    constexpr const char *build_mode = "NDEBUG undefined (Debug)";
#endif

    // GENUINE comparison, not documentation (S1d review, GODS_LAWS.md
    // L-36/L-40): the two constants above used to be printed and NEVER
    // compared against anything - renumbering them to any value still
    // compiled and still passed, the exact "afirma que mede e não mede"
    // shape. glintfx::test::all_cases() (harness/test_registry.hpp,
    // already included above) is the REAL, live count of every
    // GLINTFX_TEST registered in THIS translation unit - glintfx_add_
    // test() (cmake/GlintfxTest.cmake) builds one standalone executable
    // per test file with no extra target_sources for this one (tests/
    // CMakeLists.txt's own loop_context_mark_test entry), so this
    // binary's registry holds exactly this file's own cases, nothing
    // borrowed from a sibling TU. runtime_cases_checked's own cases plus
    // this very GLINTFX_TEST (it registers itself too) is the whole
    // registry; a case added or removed here without updating the
    // constant above now FAILS this check instead of silently drifting.
    const int registered_cases = static_cast<int>(glintfx::test::all_cases().size());
    GLINTFX_CHECK_EQ(registered_cases, runtime_cases_checked + 1);

    std::println("loop_context_mark_table_is_enumerated_in_full: {} static_assert(s) + {} "
                 "runtime GLINTFX_TEST case(s) checked ({}), {} case(s) registered",
                 static_asserts_checked, runtime_cases_checked, build_mode, registered_cases);
}
