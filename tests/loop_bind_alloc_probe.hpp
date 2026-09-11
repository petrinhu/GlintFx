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
// result is unobserved") is NOT stopped by `[[gnu::noinline]]`/
// `__declspec(noinline)` - those only stop INLINING, a different
// optimization.
//
// REPRODUCED IN ISOLATION (S1c LOOP-CALLBACK-BIND mutation review),
// with the EXACT condition, not "noinline genericamente": a minimal
// single-TU reconstruction where ONE `[[gnu::noinline]]` function
// performs `new`, calls gltfx_adopt_loop_callbacks(), AND invokes the
// returned `destroy_context` on that SAME local value - all inside
// that one function body, built with this project's own release flags
// (`-O3 -DNDEBUG`) - compiles down to nothing but a bare counter
// increment and `ret`; `objdump -d` shows no call to `operator new`/
// `operator delete` anywhere in that function. The elision needs
// `destroy_context`'s VALUE to stay resolvable by the compiler's own
// dataflow inside that ONE function, never escaping through a
// return-by-value into a DIFFERENT call frame: the moment the same
// reconstruction instead RETURNS the `gltfx_loop_callbacks` by value
// and a SEPARATE function invokes `destroy_context` off the returned
// struct - still the SAME translation unit, still noinline - the call
// is a genuinely indirect one the compiler cannot resolve, and the
// allocation is real: `objdump -d` shows the actual call into
// `operator new`'s own body, matched instruction for instruction. This
// project's actual split goes one step further than that boundary, on
// purpose: `new`+`adopt()` and the `destroy_context()` invocation
// don't just live in different functions, they live in DIFFERENT
// TRANSLATION UNITS (this header/.cpp vs. tests/loop_bind_test.cpp) -
// the exact opacity a real consumer's own call site has against
// libglintfx.so in production (this header's own next paragraph).
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
