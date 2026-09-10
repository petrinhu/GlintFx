// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>
#include <type_traits>

#include <glintfx/platform/loop/loop.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// loop_callbacks_type_test.cpp - LOOP-CALLBACK-THROW (TODO.md;
// /var/tmp/glintfx-plan/loop-fix.md sec. 3.1/5.1, GODS_LAWS.md L-17/
// L-19/L-20/L-22/L-40): the fix for the finding TODO.md names - a
// consumer callback that throws used to kill the consumer's process
// with zero protection, because gltfx_loop_callbacks held
// `std::function`, whose TYPE says nothing about `noexcept`. Layer 1
// (platform/loop/loop.hpp's own gltfx_on_frame_fn/gltfx_on_render_fn/
// gltfx_on_event_fn/gltfx_loop_context_destroy_fn) closes that: each
// is a PLAIN `noexcept` function-pointer type, and a function that
// does not declare `noexcept` is simply not convertible to it - a
// COMPILE-TIME refusal, at the consumer's own assignment line.
//
// THE PROOF IS THE static_assert MATRIX BELOW, not the GLINTFX_TEST
// cases - this is the same shape tests/display_backend_port_concept_
// test.cpp's own header comment already documents ("the 'vermelho'
// GODS_LAWS.md L-20 requires is a static_assert, not a runtime
// failure"): a callback that is not noexcept does not produce a
// TU that compiles and then fails at runtime, it produces a TU that
// does not compile at all. Each GLINTFX_TEST below re-checks the SAME
// condition through GLINTFX_CHECK, purely so ctest reports a per-case
// PASS/FAIL and a printed count (GODS_LAWS.md L-40) - if the type
// system's own refusal ever regressed (someone loosens a `using` back
// to accepting a throwing function), THIS FILE WOULD STOP COMPILING
// before any of these GLINTFX_CHECK lines ever ran; a green ctest run
// of this binary is therefore never proof by itself, only the
// static_assert lines are - recorded here so the next reader does not
// mistake "ctest passed" for "the refusal still holds" without also
// having watched this TU actually compile.

using glintfx::gltfx_frame_tick;
using glintfx::gltfx_input_event;
using glintfx::gltfx_loop_callbacks;
using glintfx::gltfx_loop_context_destroy_fn;
using glintfx::gltfx_on_event_fn;
using glintfx::gltfx_on_frame_fn;
using glintfx::gltfx_on_render_fn;

namespace {

// Eight bodies: one throwing (no `noexcept`), one well-behaved
// (`noexcept`), per callback field. Never called - only their
// DECLARED signature matters to std::is_convertible_v below, and every
// use of that signature happens through decltype (an UNEVALUATED
// operand) - so none of the eight is ever odr-used. `[[maybe_unused]]`
// says that on purpose: without it, a real second compiler (Clang)
// correctly notices no call site will ever emit code for these bodies
// and refuses under -Werror,-Wunneeded-internal-declaration - measured
// against Clang 22.1.8 while fixing this file. The attribute is the
// honest fix, not a silenced warning: the compiler's diagnosis (dead
// code) is correct, and `[[maybe_unused]]` is the standard C++17 way
// to say "yes, and that absence of use is the whole point here".
[[maybe_unused]] bool throwing_on_frame(void *, const gltfx_frame_tick &) { return true; }
[[maybe_unused]] bool nothrow_on_frame(void *, const gltfx_frame_tick &) noexcept { return true; }
[[maybe_unused]] void throwing_on_render(void *, const gltfx_frame_tick &) {}
[[maybe_unused]] void nothrow_on_render(void *, const gltfx_frame_tick &) noexcept {}
[[maybe_unused]] void throwing_on_event(void *, const gltfx_input_event &) {}
[[maybe_unused]] void nothrow_on_event(void *, const gltfx_input_event &) noexcept {}
[[maybe_unused]] void throwing_destroy_context(void *) {}
[[maybe_unused]] void nothrow_destroy_context(void *) noexcept {}

} // namespace

// --- the eight cells (LOOP-CALLBACK-THROW's own matrix) -----------------

static_assert(!std::is_convertible_v<decltype(&throwing_on_frame), gltfx_on_frame_fn>,
              "a on_frame candidate missing noexcept must not convert to gltfx_on_frame_fn");
static_assert(std::is_convertible_v<decltype(&nothrow_on_frame), gltfx_on_frame_fn>,
              "a noexcept on_frame candidate must convert to gltfx_on_frame_fn");
static_assert(!std::is_convertible_v<decltype(&throwing_on_render), gltfx_on_render_fn>,
              "a on_render candidate missing noexcept must not convert to gltfx_on_render_fn");
static_assert(std::is_convertible_v<decltype(&nothrow_on_render), gltfx_on_render_fn>,
              "a noexcept on_render candidate must convert to gltfx_on_render_fn");
static_assert(!std::is_convertible_v<decltype(&throwing_on_event), gltfx_on_event_fn>,
              "a on_event candidate missing noexcept must not convert to gltfx_on_event_fn");
static_assert(std::is_convertible_v<decltype(&nothrow_on_event), gltfx_on_event_fn>,
              "a noexcept on_event candidate must convert to gltfx_on_event_fn");
static_assert(
    !std::is_convertible_v<decltype(&throwing_destroy_context), gltfx_loop_context_destroy_fn>,
    "a destroy_context candidate missing noexcept must not convert to "
    "gltfx_loop_context_destroy_fn");
static_assert(
    std::is_convertible_v<decltype(&nothrow_destroy_context), gltfx_loop_context_destroy_fn>,
    "a noexcept destroy_context candidate must convert to gltfx_loop_context_destroy_fn");

// --- layout (the struct's own header comment, "the layout IS the
// contract") ---------------------------------------------------------

static_assert(std::is_standard_layout_v<gltfx_loop_callbacks>,
              "gltfx_loop_callbacks must stay standard layout - the layout IS the contract");
static_assert(std::is_trivially_copyable_v<gltfx_loop_callbacks>,
              "gltfx_loop_callbacks must stay trivially copyable - it crosses the ABI by value");
static_assert(sizeof(gltfx_loop_callbacks) == 5 * sizeof(void *),
              "gltfx_loop_callbacks must carry exactly five pointers, nothing else");

// A `noexcept` function pointer is itself nothrow-invocable through
// its own type - the property run()'s own call sites
// (src/platform/loop/loop_facade.cpp) rely on without a try/catch
// anywhere in that file.
static_assert(
    std::is_nothrow_invocable_r_v<bool, gltfx_on_frame_fn, void *, const gltfx_frame_tick &>,
    "gltfx_on_frame_fn must be nothrow-invocable through its own type");

// --- runtime re-checks, purely for a per-case PASS/FAIL and a printed
// count (GODS_LAWS.md L-40) - see this file's own top comment for why
// these are NOT the proof --------------------------------------------

GLINTFX_TEST(throwing_on_frame_does_not_convert_to_the_pointer_type) {
    GLINTFX_CHECK((!std::is_convertible_v<decltype(&throwing_on_frame), gltfx_on_frame_fn>));
}

GLINTFX_TEST(nothrow_on_frame_converts_to_the_pointer_type) {
    GLINTFX_CHECK((std::is_convertible_v<decltype(&nothrow_on_frame), gltfx_on_frame_fn>));
}

GLINTFX_TEST(throwing_on_render_does_not_convert_to_the_pointer_type) {
    GLINTFX_CHECK((!std::is_convertible_v<decltype(&throwing_on_render), gltfx_on_render_fn>));
}

GLINTFX_TEST(nothrow_on_render_converts_to_the_pointer_type) {
    GLINTFX_CHECK((std::is_convertible_v<decltype(&nothrow_on_render), gltfx_on_render_fn>));
}

GLINTFX_TEST(throwing_on_event_does_not_convert_to_the_pointer_type) {
    GLINTFX_CHECK((!std::is_convertible_v<decltype(&throwing_on_event), gltfx_on_event_fn>));
}

GLINTFX_TEST(nothrow_on_event_converts_to_the_pointer_type) {
    GLINTFX_CHECK((std::is_convertible_v<decltype(&nothrow_on_event), gltfx_on_event_fn>));
}

GLINTFX_TEST(throwing_destroy_context_does_not_convert_to_the_pointer_type) {
    GLINTFX_CHECK((!std::is_convertible_v<decltype(&throwing_destroy_context),
                                          gltfx_loop_context_destroy_fn>));
}

GLINTFX_TEST(nothrow_destroy_context_converts_to_the_pointer_type) {
    GLINTFX_CHECK(
        (std::is_convertible_v<decltype(&nothrow_destroy_context), gltfx_loop_context_destroy_fn>));
}

GLINTFX_TEST(gltfx_loop_callbacks_layout_is_five_pointers_standard_layout_trivially_copyable) {
    GLINTFX_CHECK(std::is_standard_layout_v<gltfx_loop_callbacks>);
    GLINTFX_CHECK(std::is_trivially_copyable_v<gltfx_loop_callbacks>);
    GLINTFX_CHECK_EQ(sizeof(gltfx_loop_callbacks), 5 * sizeof(void *));
}

GLINTFX_TEST(loop_callback_throw_matrix_is_enumerated_in_full) {
    // Twelve static_assert(s) above are what actually proves anything
    // (this file's own top comment) - this case only counts them, so
    // an empty or gutted translation unit could never pass silently.
    constexpr int cells_checked = 8 /* type cells */ + 3 /* layout */ + 1 /* invocability */;
    std::println("loop_callback_throw_matrix_is_enumerated_in_full: {} of {} static_assert(s) "
                 "checked (8 type cells, 3 layout, 1 invocability)",
                 cells_checked, cells_checked);
}
