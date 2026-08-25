// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core_error_cost_functions.hpp"

// core_error_cost_functions.cpp - CE-7 of CORE-ERROR (TODO.md): the
// bodies main() (core_error_cost.cpp) cannot see - see this pair's own
// .hpp for why the split TU is a measurement correctness requirement,
// not a style choice.

namespace glintfx::bench {

gltfx_err make_code_only() noexcept { return gltfx_err(gltfx_err_code::parse_failure); }

gltfx_err make_with_context() noexcept {
    gltfx_err err(gltfx_err_code::parse_failure);
    err.with_path("assets/scene.rcss").with_position(12, 5);
    return err;
}

int direct_value() noexcept { return 42; }

gltfx_rslt<int> wrapped_value() noexcept { return gltfx_rslt<int>::ok(42); }

} // namespace glintfx::bench
