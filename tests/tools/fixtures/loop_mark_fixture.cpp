// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <new>
#include <type_traits>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>
#include <glintfx/platform/loop/loop_context_mark.hpp>

// loop_mark_fixture.cpp - proves BOTH halves of camada 4's own
// debug-only liveness guard (LOOP-CONTEXT-MARK, S1d, /var/tmp/
// glintfx-plan/loop-fix.md sec. 3.4 item 4): in a build where NDEBUG
// is undefined (Debug), calling a bound method on an object whose
// destructor already ran stops the process DETERMINISTICALLY, with a
// message naming exactly what went wrong, through the SAME assert()/
// abort() mechanism include/glintfx/core/err.hpp's own precondition
// guard already uses (F22) - proved live, for THAT guard, by
// tests/tools/check_rslt_precondition.py's own precondition_fixture.
// cpp, the direct precedent this file mirrors (same shape: one small
// program, no argv branching needed here since there is only ONE
// precondition to violate, unlike that file's "primary"/"void" pair).
//
// In a build where NDEBUG is defined (Release, this project's
// default), the guard compiles to nothing: this fixture does NOT call
// the thunk over the dead object in that mode at all - doing so would
// be a genuine, UNGUARDED use-after-destruction, the exact undefined
// behavior camada 4 never claims to prevent in Release
// (loop_context_mark.hpp's own "WHAT THIS LAYER DOES NOT SOLVE"). It
// only prints the two structural facts (sizeof, is_empty) tests/
// loop_context_mark_test.cpp's own static_assert(s) already prove at
// compile time, and exits 0.

namespace {

struct marked_app : glintfx::gltfx_loop_context_mark {
    bool frame(const glintfx::gltfx_frame_tick &) noexcept { return true; }
    void render(const glintfx::gltfx_frame_tick &) noexcept {}
};

// suppress_core_dump_for_intentional_crash - same reasoning as
// tests/precondition_fixtures/precondition_fixture.cpp's own copy of
// this function (that file's own header comment, copied here rather
// than shared, because this fixture is OUT OF SCOPE for edits from
// any OTHER sub-fatia - the same "read here, never edited" rule that
// file's own sibling script states for it): this program dies ON
// PURPOSE in Debug (SIGABRT from the debug-only assert), and leaving
// core dumps enabled floods the developer's crash pipeline with false
// "crashed unexpectedly" reports (measured there: 15 dumps in 30
// minutes). PR_SET_DUMPABLE=0 stops the piped core_pattern
// (systemd-coredump on this machine, GODS_LAWS.md L-25) from ever
// running, without changing signal delivery or exit status.
void suppress_core_dump_for_intentional_crash() {
#if defined(__linux__)
    (void)prctl(PR_SET_DUMPABLE, 0);
#endif
}

} // namespace

int main() {
    suppress_core_dump_for_intentional_crash();

    std::printf("loop_mark_fixture: sizeof(marked_app)=%zu is_empty(gltfx_loop_context_mark)=%d\n",
                sizeof(marked_app),
                static_cast<int>(std::is_empty_v<glintfx::gltfx_loop_context_mark>));

#ifndef NDEBUG
    // Debug: build a marked object in storage THIS function owns,
    // bind it, destroy it explicitly WITHOUT freeing the storage (the
    // one deterministic signal this layer has is the destructor
    // itself clearing the mark - loop_context_mark.hpp's own header
    // comment), then call the bound thunk over the now-dead object.
    alignas(marked_app) unsigned char storage[sizeof(marked_app)];
    auto *object = new (static_cast<void *>(storage)) marked_app();

    const glintfx::gltfx_loop_callbacks callbacks =
        glintfx::gltfx_bind_loop_callbacks<&marked_app::frame, &marked_app::render>(*object);

    object->~marked_app();

    // Precondition violation ON PURPOSE: this is the point of this
    // program, not a mistake. loop_bind.hpp's own assert() inside the
    // thunk is expected to fire BEFORE this call ever reaches
    // marked_app::frame() - reading gltfx_mark_word through a pointer
    // whose pointee's lifetime already ended is the SAME structural,
    // not-guaranteed-by-the-standard access category docs/api-
    // conventions.md R1 already documents for gltfx_rslt<T>'s own
    // Release-mode fault (that category is what makes this a
    // LEGITIMATE proof of a debug-only guard, not a new kind of risk
    // this fatia invents).
    const glintfx::gltfx_frame_tick tick{};
    const bool result = callbacks.on_frame(callbacks.context, tick);
    std::printf("UNEXPECTED: on_frame() returned %d without the mark guard firing (Debug, "
                "the assert did not stop the process this run)\n",
                static_cast<int>(result));
#else
    std::printf("loop_mark_fixture: Release build (NDEBUG defined) - the guard compiles to "
                "nothing, so this fixture never calls through a dead marked object (that "
                "would be real, unguarded undefined behavior, never exercised here)\n");
#endif

    return 0;
}
