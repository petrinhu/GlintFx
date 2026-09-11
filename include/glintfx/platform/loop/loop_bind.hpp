// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <type_traits>

#include <glintfx/platform/loop/loop.hpp>

// platform/loop/loop_bind.hpp - LOOP-CALLBACK-BIND (S1c, /var/tmp/
// glintfx-plan/loop-fix.md sec. 3.3/S1c, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-28/L-32): camada 3 of the four-layer contract loop.hpp's own
// gltfx_loop_callbacks builds (docs/api-conventions.md R10) - the ONLY
// layer that is header-only, template-based, and buys the consumer
// NOTHING at the ABI boundary (it never touches loop_impl, never adds
// a symbol to the .so/.dll): it exists purely so a consumer who wants
// to bind an object-and-method pair to gltfx_loop_callbacks::on_frame/
// on_render never has to hand-write the two thunks (a free function
// that casts `void *context` back and calls through a stored pointer-
// to-member) themselves, and never has to hand-write the delete thunk
// camada 2's own `destroy_context` needs when they choose to hand over
// ownership (gltfx_adopt_loop_callbacks below).
//
// ============================================================
// THE EFFECT, IN ONE SENTENCE: a consumer who already has an object
// with an on_frame(tick)/on_render(tick) pair of methods gets a
// gltfx_loop_callbacks that calls them directly - no virtual dispatch,
// no std::function, no allocation of its own - by writing
// gltfx_bind_loop_callbacks<&my_type::on_frame, &my_type::on_render>
// (my_object) instead of two hand-rolled free functions.
// ============================================================

namespace glintfx {

namespace detail {

// gltfx_loop_bindable - THE compile-time gate both public functions
// below share (used exactly three times: here, once per `requires`
// clause below - CONTRACT.md's own "rule of three" for extracting a
// helper, GODS_LAWS.md L-17 DRY, L-32 against inventing an abstraction
// nobody needs twice yet). Object::*OnFrame must be callable on
// `Object &` with a `const gltfx_frame_tick &`, returning something
// convertible to `bool`, WITHOUT throwing; Object::*OnRender the same,
// except its return value is discarded (void).
// std::is_nothrow_invocable_r_v/std::is_nothrow_invocable_v are what
// F17 (/var/tmp/glintfx-plan/loop-fix.md) measured: they accept a
// const method, a non-const method, and reject one missing `noexcept`,
// with NO special-casing needed for either.
template <auto OnFrame, auto OnRender, class Object>
concept gltfx_loop_bindable =
    std::is_nothrow_invocable_r_v<bool, decltype(OnFrame), Object &, const gltfx_frame_tick &> &&
    std::is_nothrow_invocable_v<decltype(OnRender), Object &, const gltfx_frame_tick &>;

// The three thunks - EACH a plain free-function template, called
// through the SAME `void *context` calling convention layer 1 already
// fixes (loop.hpp's own gltfx_on_frame_fn/gltfx_on_render_fn/
// gltfx_loop_context_destroy_fn). `Method` is a template NON-TYPE
// parameter (a pointer-to-member VALUE baked into the template
// instantiation, not a runtime field this library stores anywhere) -
// the call inside each thunk is a DIRECT `(object->*Method)(...)`,
// never a lookup through a pointer-to-member kept in a struct. That is
// what "tipado e sem custo em execucao" means.
//
// WHY `detail`, MEASURED, NOT GUESSED (L-32: never invent a namespace
// "for later"): `grep -rn 'namespace .*detail' include/` found exactly
// one precedent already in this tree - `glintfx::asset::detail`
// (include/glintfx/platform/asset/file.hpp) - hiding a helper a
// consumer never names either. Loop's own public surface has no
// per-domain namespace to nest under the way asset's does (loop.hpp
// itself declares gltfx_loop/gltfx_loop_callbacks straight in
// `glintfx`, the SAME flat shape window.hpp and gl/context.hpp already
// use) - so the equivalent nesting here is `glintfx::detail`, the same
// idiom one level shallower, not a new namespace shape invented for
// this file. A consumer NEVER writes the name of any of the three
// thunks below - they call gltfx_bind_loop_callbacks()/gltfx_adopt_
// loop_callbacks() further down, and the address a thunk instantiation
// happens to have is an implementation fact those two functions hand
// back through gltfx_loop_callbacks::on_frame/on_render/destroy_
// context, never an API surface of its own.
template <class Object, auto Method>
bool gltfx_loop_frame_thunk(void *context, const gltfx_frame_tick &tick) noexcept {
    return (static_cast<Object *>(context)->*Method)(tick);
}

template <class Object, auto Method>
void gltfx_loop_render_thunk(void *context, const gltfx_frame_tick &tick) noexcept {
    (static_cast<Object *>(context)->*Method)(tick);
}

// gltfx_loop_delete_thunk - the ONLY thing gltfx_adopt_loop_callbacks()
// below buys over camada 2 alone: without it, the only typed way to
// use camada 2's own destroy_context would be for the consumer to
// write this exact one-liner themselves - which is precisely what
// camada 3 exists to remove (D-LF-8). `noexcept` here carries the SAME
// promise gltfx_loop_context_destroy_fn's own type already requires
// (loop.hpp): if Object's destructor throws, the C++ language ends the
// process at this boundary (std::terminate) before this library's own
// run() is even reached - the same "noexcept is a promise, not a
// proof" truth layer 1's own header comment already states, applied
// here to a destructor instead of a per-frame callback.
template <class Object> void gltfx_loop_delete_thunk(void *context) noexcept {
    delete static_cast<Object *>(context);
}

} // namespace detail

// ============================================================
// WHAT THIS LAYER SOLVES:
// ============================================================
//   - Binds an object-and-method pair to gltfx_loop_callbacks without
//     the consumer writing a single thunk by hand - gltfx_bind_loop_
//     callbacks() (borrow) or gltfx_adopt_loop_callbacks() (hand over,
//     typed, camada 2's own destroy_context filled in for you).
//   - Refuses a temporary at the consumer's OWN call site, in
//     compilation (the `gltfx_bind_loop_callbacks(Object &&) = delete`
//     overload below) - std::move(local_object) or a prvalue argument
//     never compiles.
//   - Refuses, again in compilation, an OnFrame/OnRender that is not
//     `noexcept` (gltfx_loop_bindable above) - the SAME "the mistake
//     surfaces on YOUR build, not as a crash inside this library"
//     property layer 1 already gives a plain function pointer,
//     extended here to a pointer-to-member.
//   - Costs NOTHING beyond layer 1's own five pointers: no allocation,
//     no virtual dispatch, a constant expression when `object` has
//     static storage duration (tests/loop_bind_test.cpp's own
//     constexpr case).
// ============================================================
// WHAT THIS LAYER DOES NOT SOLVE - truth (c), stated so nobody has to
// infer it from an absence (/var/tmp/glintfx-plan/loop-fix.md sec.
// 3.3):
// ============================================================
//   Borrowing (gltfx_bind_loop_callbacks) makes YOU responsible for
//   `object` outliving the loop - exactly as layer 1's own bare
//   `void *context` already required; camada 3 changes nothing about
//   WHO is responsible, only HOW the pointer is produced. The
//   temporary refusal above catches only the single most common
//   mistake (passing `my_type{}` directly): it does NOT catch a local
//   object that goes out of scope before run() is called, and it does
//   NOT catch a std::unique_ptr<my_type> reset to something else
//   partway through the loop's own lifetime - both stay the
//   consumer's own responsibility to avoid, the same way an ordinary
//   dangling pointer always is in C++.
//
//   Three ways to fill in gltfx_loop_callbacks::on_frame/on_render,
//   side by side - camada 3 (this header) is needed for ONLY the
//   third:
//
//     // 1. A free function - plain assignment, no camada 3:
//     callbacks.on_frame = &my_free_on_frame;
//
//     // 2. A stateless lambda that converts directly (layer 1's own
//     //    property, tests/loop_callbacks_type_test.cpp) - still no
//     //    camada 3, as long as it is declared `noexcept`:
//     callbacks.on_frame = [](void *, const gltfx_frame_tick &) noexcept { return true; };
//
//     // 3. An object-and-method pair - what THIS header is for:
//     callbacks = gltfx_bind_loop_callbacks<&app::on_frame, &app::on_render>(my_app);
// ============================================================

// gltfx_bind_loop_callbacks - BORROWS `object`: `context` is
// `&object`, `destroy_context` is left null (layer 2's own "nullptr
// means borrowed", loop.hpp). Never allocates, never copies `object` -
// the returned gltfx_loop_callbacks::context is the SAME address as
// `&object`, byte for byte (tests/loop_bind_test.cpp's own constexpr
// case (i)).
template <auto OnFrame, auto OnRender, class Object>
    requires detail::gltfx_loop_bindable<OnFrame, OnRender, Object>
[[nodiscard]] constexpr gltfx_loop_callbacks gltfx_bind_loop_callbacks(Object &object) noexcept {
    return gltfx_loop_callbacks{
        .context = &object,
        .on_frame = &detail::gltfx_loop_frame_thunk<Object, OnFrame>,
        .on_render = &detail::gltfx_loop_render_thunk<Object, OnRender>,
        .on_event = nullptr,
        .destroy_context = nullptr,
    };
}

// The overload the compiler actually PICKS for a temporary (F15,
// /var/tmp/glintfx-plan/loop-fix.md) - NOT what refuses it. The
// non-const lvalue reference above already refuses an rvalue argument
// on its own: measured (S1c red-before-green, this sub-fatia's own
// implementation step) by compiling gltfx_bind_loop_callbacks() with
// ONLY the `Object &` overload above and no deleted overload at all -
// tests/loop_bind_test.cpp's own `binds_temporary`/`binds_rvalue`
// static_assert(s) were already green. This overload exists ONLY for
// the MESSAGE a consumer reads: without it, a temporary produces a
// generic "no matching function" from the `Object &` overload's own
// failed reference binding; with it, the temporary becomes the BEST
// match (an rvalue binds an rvalue reference exactly), so the
// diagnostic becomes "use of deleted function
// 'gltfx_bind_loop_callbacks(Object&&)'", pointing at the consumer's
// own call site instead of a generic deduction failure. Keep this
// comment - a future reader who deletes this overload "as dead code"
// would be right that nothing stops compiling, and wrong that nothing
// was lost.
template <auto OnFrame, auto OnRender, class Object>
gltfx_loop_callbacks gltfx_bind_loop_callbacks(Object &&) = delete;

// gltfx_adopt_loop_callbacks - HANDS OVER `heap_object`: `context` is
// `heap_object`, `destroy_context` is the typed delete thunk above -
// layer 2's own "non-null means handed over" (loop.hpp) - so `run()`
// (or `set_callbacks()`) destroys it exactly once, through `delete`,
// on whichever lifetime the consumer's own call chooses (gltfx_loop_
// callbacks::destroy_context's own header comment). `heap_object` MUST
// already be on the heap (`new Object(...)`) - this function never
// allocates one; it only fills in the typed thunk that will eventually
// `delete` it (D-LF-8: the only alternative, without this function, is
// the consumer hand-writing that exact one-liner themselves for every
// type they adopt). Takes a raw pointer, never `std::unique_ptr` or
// anything else from the standard library, on purpose - the same
// fronteira-extended-to-the-header-by-coherence rule the plan's own
// sec. 3.3 states for this exact signature (GODS_LAWS.md L-07/L-22).
template <auto OnFrame, auto OnRender, class Object>
    requires detail::gltfx_loop_bindable<OnFrame, OnRender, Object>
[[nodiscard]] constexpr gltfx_loop_callbacks
gltfx_adopt_loop_callbacks(Object *heap_object) noexcept {
    return gltfx_loop_callbacks{
        .context = heap_object,
        .on_frame = &detail::gltfx_loop_frame_thunk<Object, OnFrame>,
        .on_render = &detail::gltfx_loop_render_thunk<Object, OnRender>,
        .on_event = nullptr,
        .destroy_context = &detail::gltfx_loop_delete_thunk<Object>,
    };
}

} // namespace glintfx
