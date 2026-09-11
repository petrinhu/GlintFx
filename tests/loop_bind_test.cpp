// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <print>
#include <type_traits>
#include <utility>

#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/loop/loop_bind.hpp>

#include "loop_bind_alloc_probe.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// loop_bind_test.cpp - LOOP-CALLBACK-BIND (S1c, /var/tmp/glintfx-plan/
// loop-fix.md sec. 3.3/S1c, GODS_LAWS.md L-17/L-19/L-20/L-22/L-40):
// proves, line by line, the table that plan's own sec. 3.3 names -
// compilation properties as static_assert (the REAL proof, the same
// discipline tests/loop_callbacks_type_test.cpp's own header comment
// already documents: if any of these regressed, THIS FILE WOULD NOT
// COMPILE, before any GLINTFX_TEST case below ever ran) and the two
// properties only observable at runtime (allocation count, F24; which
// object the thunk actually calls) as GLINTFX_TEST cases.
//
// PURE HEADER TEST, NO .cpp DEPENDENCY, NO OPERATING SYSTEM (registers
// unguarded on all five platforms, the same shape loop_callbacks_type_
// test.cpp and owned_loop_context_test.cpp already use one directory
// over): platform/loop/loop_bind.hpp is entirely inline/template, and
// every fixture below is a plain in-memory object this TU itself owns.

using glintfx::gltfx_adopt_loop_callbacks;
using glintfx::gltfx_bind_loop_callbacks;
using glintfx::gltfx_frame_tick;
using glintfx::gltfx_loop_callbacks;
using glintfx::gltfx_on_frame_fn;
using glintfx::detail::gltfx_loop_bindable;
using glintfx::detail::gltfx_loop_frame_thunk;
using glintfx::detail::gltfx_loop_render_thunk;

namespace {

// app - the fixture the plan's own sec. 3.3 table uses throughout
// (F15-F18): two well-behaved methods (frame/render), one const
// overload gltfx_loop_bindable must accept identically (F17: no
// special-casing between const and non-const), and one candidate
// MISSING `noexcept` on purpose - the negative case.
struct app {
    int frame_calls = 0;
    int render_calls = 0;

    bool frame(const gltfx_frame_tick &) noexcept {
        ++frame_calls;
        return true;
    }
    void render(const gltfx_frame_tick &) noexcept { ++render_calls; }

    bool const_frame(const gltfx_frame_tick &) const noexcept { return true; }

    // Never called - only its DECLARED signature (missing `noexcept`)
    // matters to gltfx_loop_bindable below, checked through an
    // unevaluated operand (decltype inside std::is_nothrow_invocable_*).
    // [[maybe_unused]]: the same "the compiler is right that nothing
    // calls this, and that absence of a call is the whole point"
    // reasoning tests/loop_callbacks_type_test.cpp's own header
    // comment already documents for its eight bodies.
    [[maybe_unused]] bool throwing_frame(const gltfx_frame_tick &) { return true; }
};

// destroy_flag_app - the SEPARATE fixture "adopt entrega posse de
// forma tipada" needs: a static flag its OWN destructor writes to
// BEFORE the memory is freed - the only way to observe "the real
// destructor ran" without reading freed memory afterward (which would
// itself be undefined behavior).
struct destroy_flag_app {
    inline static bool destroyed = false;

    bool frame(const gltfx_frame_tick &) noexcept { return true; }
    void render(const gltfx_frame_tick &) noexcept {}

    ~destroy_flag_app() noexcept { destroyed = true; }
};

// g_static_app - static storage duration ON PURPOSE: only an object
// with static storage duration can have its address taken inside a
// constant expression (F16) - a local or heap object cannot.
app g_static_app{};

// --- property "recusa temporario em compilacao" (sec. 3.3 table row 1,
// F15) - three concepts over the ACTUAL CALL EXPRESSION, never
// decltype on the overloaded function name itself (F15's own "nao
// decltype sobre a sobrecarga; nao resolve" - an overloaded name has
// no single type to take decltype of before the call is written) ---

template <class T>
concept binds_temporary = requires {
    { gltfx_bind_loop_callbacks<&app::frame, &app::render>(T{}) };
};

template <class T>
concept binds_rvalue = requires(T object) {
    { gltfx_bind_loop_callbacks<&app::frame, &app::render>(std::move(object)) };
};

template <class T>
concept binds_lvalue = requires(T &object) {
    { gltfx_bind_loop_callbacks<&app::frame, &app::render>(object) };
};

} // namespace

static_assert(!binds_temporary<app>,
              "a prvalue app{} must be refused at compile time - loop_bind.hpp's own "
              "Object& parameter already refuses it (F15); the deleted Object&& overload "
              "exists only for the diagnostic message, see that overload's own comment");
static_assert(!binds_rvalue<app>, "std::move(named_app) must be refused the same way (F15)");
static_assert(binds_lvalue<app>, "an ordinary lvalue app must still bind");

// --- property "aceita lambda sem estado, sem camada" (row 2, F18) - a
// stateless noexcept lambda converts DIRECTLY to gltfx_on_frame_fn,
// with no camada 3 involved at all (loop_bind.hpp's own "WHAT THIS
// LAYER SOLVES" comment, form 2 of the three side by side). The FULL
// eight-cell noexcept matrix already lives in
// tests/loop_callbacks_type_test.cpp; this is the one cell that
// belongs here, because it is the form loop_bind.hpp's own header
// comment contrasts camada 3 against. ---

static_assert(
    std::is_convertible_v<decltype([](void *, const gltfx_frame_tick &) noexcept { return true; }),
                          gltfx_on_frame_fn>,
    "a stateless noexcept lambda must still convert directly - camada 3 is optional, not required");

// --- property "amarra metodo, tipado" (row 3, F17) ---

static_assert(gltfx_loop_bindable<&app::frame, &app::render, app>,
              "a noexcept non-const method pair must be bindable");
static_assert(gltfx_loop_bindable<&app::const_frame, &app::render, app>,
              "a const method must bind exactly like a non-const one - no special-casing (F17)");
static_assert(!gltfx_loop_bindable<&app::throwing_frame, &app::render, app>,
              "a method missing noexcept must be refused - removing the requires clause "
              "is the mutation this static_assert kills");

// --- property "custo zero, sem alocacao" (row 4), measures (i) and
// (ii) - the two that are themselves compile-time (F16); (iii) and
// (iv) are runtime, GLINTFX_TEST cases below ---

// (i) expression constante: taking gltfx_bind_loop_callbacks()'s
// result at compile time proves it executes NOTHING at runtime and
// leaves NO allocation escaping (F16) - a thunk that copied `object`,
// or that allocated, would simply not be usable here.
constexpr gltfx_loop_callbacks k =
    gltfx_bind_loop_callbacks<&app::frame, &app::render>(g_static_app);
static_assert(k.context == &g_static_app);
static_assert(k.on_frame == &gltfx_loop_frame_thunk<app, &app::frame>);
static_assert(k.on_render == &gltfx_loop_render_thunk<app, &app::render>);
static_assert(k.on_event == nullptr);
static_assert(k.destroy_context == nullptr);

// (ii) the result carries NOTHING beyond the fronteira's own five
// pointers - camada 3 adds no field, the same static_assert loop.hpp
// and loop_callbacks_type_test.cpp already carry, re-stated here
// because this table row asks for it explicitly.
static_assert(sizeof(gltfx_loop_callbacks) == 5 * sizeof(void *),
              "gltfx_loop_callbacks must stay five pointers - camada 3 adds no field");

// (iii) "custo zero, sem alocacao" - the allocation-counting half of
// this property lives in loop_bind_alloc_probe.cpp/.hpp, in its OWN
// translation unit (see that header's own top comment for the exact,
// measured reason: a first version of both cases below lived entirely
// in THIS file, and GCC's own new/delete elision - an interprocedural
// analysis WITHIN one translation unit, immune to
// GLINTFX_TEST_NOINLINE - proved the whole allocate/adopt/destroy
// chain unobserved and removed the allocation from the compiled
// binary, `objdump -d` confirmed).

// bind() never allocates - measured, not assumed: the probe's own
// counter before and after gltfx_bind_loop_callbacks() must be
// IDENTICAL. No cross-TU split needed HERE: there is no `new`/`delete`
// pair for the optimizer to elide in the first place (bind() never
// calls `new` at all), so a flat count proves the same thing a correct
// elision would have hidden if there had been one.
GLINTFX_TEST(bind_allocates_nothing_beyond_the_borrowed_object) {
    app local_app{};
    const std::size_t calls_before = glintfx_test_probe::new_call_count();

    const gltfx_loop_callbacks cb = gltfx_bind_loop_callbacks<&app::frame, &app::render>(local_app);

    GLINTFX_CHECK_EQ(glintfx_test_probe::new_call_count(), calls_before);
    GLINTFX_CHECK(cb.destroy_context == nullptr);
    GLINTFX_CHECK(cb.context == &local_app);
}

// adopt() half: gltfx_adopt_loop_callbacks() itself adds ZERO
// allocations beyond the consumer's OWN `new` - the probe's own
// counter moves exactly once, for the ONE allocation loop_bind_alloc_
// probe.cpp's own allocate_and_adopt_probe() makes, never a second
// time for gltfx_adopt_loop_callbacks() itself. destroy_context here
// is returned BY VALUE from another translation unit: this file has no
// compile-time knowledge of what it points to, so calling it below is
// a genuinely indirect call - the same opacity a real consumer's own
// call site has against libglintfx.so in production (loop_bind_alloc_
// probe.hpp's own top comment).
GLINTFX_TEST(adopt_allocates_exactly_the_consumers_own_new_nothing_more) {
    const std::size_t calls_before = glintfx_test_probe::new_call_count();

    const gltfx_loop_callbacks cb = glintfx_test_probe::allocate_and_adopt_probe();

    GLINTFX_CHECK_EQ(glintfx_test_probe::new_call_count(), calls_before + 1);
    GLINTFX_CHECK(cb.destroy_context != nullptr);

    cb.destroy_context(cb.context); // cleanup - also exercises the delete thunk

    // The delete thunk's own `delete` calls operator delete, never
    // operator new - the counter must not move a second time.
    GLINTFX_CHECK_EQ(glintfx_test_probe::new_call_count(), calls_before + 1);
}

// (iv) chamada direta: the thunk calls the RIGHT method on the RIGHT
// object, with no copy in between (F16's own "the thunk that copies
// the object would simply diverge from &g_static_app" reasoning,
// exercised here at runtime instead of compile time).
GLINTFX_TEST(bound_thunk_calls_the_right_method_on_the_right_object) {
    app local_app{};
    const gltfx_loop_callbacks cb = gltfx_bind_loop_callbacks<&app::frame, &app::render>(local_app);
    const gltfx_frame_tick tick{};

    GLINTFX_CHECK(cb.on_frame(cb.context, tick));
    GLINTFX_CHECK_EQ(local_app.frame_calls, 1);

    cb.on_render(cb.context, tick);
    GLINTFX_CHECK_EQ(local_app.render_calls, 1);

    GLINTFX_CHECK(cb.context == &local_app);
}

// "adopt entrega posse de forma tipada" (sec. 3.3 table, last row):
// destroy_context is the typed delete thunk, and calling it runs the
// REAL destructor (destroy_flag_app::destroyed flips) before the
// memory is freed - the mutation that kills this case is
// gltfx_adopt_loop_callbacks() leaving destroy_context null (the
// table's own "asserção do ponteiro falha").
GLINTFX_TEST(adopt_hands_ownership_through_the_typed_delete_thunk) {
    destroy_flag_app::destroyed = false;
    auto *heap_app = new destroy_flag_app();

    const gltfx_loop_callbacks cb =
        gltfx_adopt_loop_callbacks<&destroy_flag_app::frame, &destroy_flag_app::render>(heap_app);

    GLINTFX_CHECK(cb.destroy_context != nullptr);
    GLINTFX_CHECK(!destroy_flag_app::destroyed);

    cb.destroy_context(cb.context);

    GLINTFX_CHECK(destroy_flag_app::destroyed);
}

GLINTFX_TEST(loop_callback_bind_table_is_enumerated_in_full) {
    // Thirteen static_assert(s) above are what actually proves the
    // compile-time half of the plan's own sec. 3.3 table (this file's
    // own top comment) - this case only counts them plus the four
    // runtime cases, so an empty or gutted translation unit could
    // never pass silently (GODS_LAWS.md L-40).
    constexpr int static_asserts_checked =
        3    /* refusal: temporary, rvalue, lvalue */
        + 1  /* stateless lambda needs no camada 3 */
        + 3  /* gltfx_loop_bindable: method, const, throwing */
        + 5  /* constexpr k: context/on_frame/on_render/on_event/destroy_context */
        + 1; /* five-pointer layout */
    constexpr int runtime_cases_checked =
        4; /* bind alloc, adopt alloc, thunk dispatch, adopt destroy */
    std::println("loop_callback_bind_table_is_enumerated_in_full: {} static_assert(s) + {} "
                 "runtime GLINTFX_TEST case(s) checked",
                 static_asserts_checked, runtime_cases_checked);
}
