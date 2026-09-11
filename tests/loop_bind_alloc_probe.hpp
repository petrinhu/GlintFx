// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <glintfx/platform/loop/loop.hpp>

// loop_bind_alloc_probe.hpp - LOOP-CALLBACK-BIND (S1c, /var/tmp/
// glintfx-plan/loop-fix.md sec. 3.3/S1c): the allocation-counting half
// of tests/loop_bind_test.cpp's own sec. 3.3 property (iii), "custo
// zero, sem alocacao" - split into its OWN translation unit
// (loop_bind_alloc_probe.cpp) ON PURPOSE, MEASURED necessary, not
// decoration.
//
// WHAT WENT WRONG WITHOUT THIS SPLIT (measured, not guessed): a first
// version of this case called `new app()`, gltfx_adopt_loop_
// callbacks() and the returned destroy_context thunk ALL inside
// tests/loop_bind_test.cpp, wrapped in GLINTFX_TEST_NOINLINE functions
// (tests/harness/compiler_noinline.hpp) to try to stop the optimizer
// from seeing through them. That was not enough: GCC's own new/delete
// elision ([expr.new], "an implementation is allowed to omit a call to
// a replaceable global allocation function... if it can prove the
// result is unobserved") is an INTERPROCEDURAL analysis WITHIN one
// translation unit, and `[[gnu::noinline]]`/`__declspec(noinline)`
// only stop INLINING - they do not stop that separate analysis from
// seeing across the noinline boundary. Measured directly:
// `objdump -d build/tests/loop_bind_test` showed the noinline
// `allocate_app_opaque()` compiled down to nothing but the counter
// increment, no call to `operator new` anywhere - the SAME elision,
// just proven immune to noinline.
//
// WHY A SEPARATE .cpp FIXES IT: this project's own CMake has no
// `-flto`/`CMAKE_INTERPROCEDURAL_OPTIMIZATION` anywhere
// (`grep -rn 'flto\|INTERPROCEDURAL' cmake/*.cmake`, none) - so two
// SEPARATE translation units, compiled independently and only linked
// together, are analyzed independently too. allocate_and_adopt_probe()
// below both allocates AND calls gltfx_adopt_loop_callbacks() INSIDE
// loop_bind_alloc_probe.cpp, and returns the resulting
// gltfx_loop_callbacks BY VALUE - `destroy_context` in that returned
// value is, from tests/loop_bind_test.cpp's own point of view, a
// function pointer whose target it cannot know at compile time (it
// only knows the field's TYPE, gltfx_loop_context_destroy_fn); calling
// it there is a genuinely indirect call the optimizer cannot devirtualize.
// This is the SAME shape a real consumer's own code has in production:
// gltfx_adopt_loop_callbacks() is called in the CONSUMER's own
// translation unit, and destroy_context is eventually invoked from
// INSIDE libglintfx.so's own run()/set_callbacks() - a different
// translation unit, in a different binary, that never sees the
// concrete adopted type either.
namespace glintfx_test_probe {

// Total calls this probe's own operator new override (loop_bind_alloc_
// probe.cpp) has counted so far - the ONE global replacement in this
// test binary (tests/loop_bind_test.cpp itself defines none, to avoid
// a duplicate-definition link error: the replaceable global allocation
// functions may be defined only once in the whole program).
[[nodiscard]] std::size_t new_call_count();

// Allocates ONE probe object on the heap and binds camada 3's own
// gltfx_adopt_loop_callbacks() to it - see this header's own top
// comment for why both steps happen HERE, never in the caller's TU.
[[nodiscard]] glintfx::gltfx_loop_callbacks allocate_and_adopt_probe();

} // namespace glintfx_test_probe
