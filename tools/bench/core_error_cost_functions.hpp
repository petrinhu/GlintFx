// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// core_error_cost_functions.hpp - CE-7 of CORE-ERROR (TODO.md):
// declarations ONLY, no bodies. core_error_cost.cpp (main()) sees just
// this file; the bodies live in the SEPARATE translation unit
// core_error_cost_functions.cpp. This split is a MEASUREMENT
// correctness requirement, not a style choice: measured live, a
// single-TU version of this benchmark let GCC's interprocedural
// constant propagation (IPA-CP) see straight through direct_value()'s
// trivial "return 42" - even marked [[gnu::noinline]], which only
// forbids INLINING the body, not other IPA passes - and fold the
// entire 2-million-iteration loop into a closed-form constant, timing
// nothing (measured: 0.000 ns/op, impossible for a real function
// call). Two separate .o files, linked without LTO (this project sets
// none), is what actually forces the compiler to emit a REAL call at
// each loop iteration and measure the ABI's real cost, not the
// optimizer's ability to see through a trivial function body it
// happens to share a file with.

namespace glintfx::bench {

glintfx::gltfx_err make_code_only() noexcept;
glintfx::gltfx_err make_with_context() noexcept;

int direct_value() noexcept;
glintfx::gltfx_rslt<int> wrapped_value() noexcept;

} // namespace glintfx::bench
