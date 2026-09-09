// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/gl/gl_surface_size_policy.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_surface_size_policy_test.cpp - GL-CONTEXT (docs/plano-w6b-placa-
// e-laco.md fatia 2b, D-W6b-5, GODS_LAWS.md L-20): the TDD red/green
// witness for glintfx::platform::resolve_gl_surface_size() (src/
// platform/gl/gl_surface_size_policy.hpp) - the four cases the plan's
// own fatia 2b row names, exercised with SYNTHETIC scale-2 values
// (docs/plano-w6b-placa-e-laco.md sec. 0's own "memoria da casa: o
// servidor nunca reporta escala != 1" - the same reason window_state_
// test.cpp already had to invent buffer_scale=2/dpi=144 instead of
// trusting either CI executor).
//
// RED, SEEN: before gl_surface_size_policy.{hpp,cpp} existed, this
// file's own #include line failed to compile.

using glintfx::platform::gl_surface_size_decision;
using glintfx::platform::resolve_gl_surface_size;

GLINTFX_TEST(resolve_gl_surface_size_does_not_resize_when_sizes_already_match) {
    const glintfx::gltfx_rslt<gl_surface_size_decision> result =
        resolve_gl_surface_size(800, 600, 800, 600);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(!result.value().should_resize);
}

GLINTFX_TEST(resolve_gl_surface_size_doubles_the_buffer_at_synthetic_scale_two) {
    // A logical 800x600 window at buffer scale 2 (window_state.hpp's
    // own formula) reports pixel_size() 1600x1200 - the surface was
    // last resized to the scale-1 size, 800x600.
    const glintfx::gltfx_rslt<gl_surface_size_decision> result =
        resolve_gl_surface_size(1600, 1200, 800, 600);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result.value().should_resize);
    GLINTFX_CHECK_EQ(result.value().width, static_cast<std::uint32_t>(1600));
    GLINTFX_CHECK_EQ(result.value().height, static_cast<std::uint32_t>(1200));
}

GLINTFX_TEST(resolve_gl_surface_size_shrinks_back_when_scale_returns_to_one) {
    const glintfx::gltfx_rslt<gl_surface_size_decision> result =
        resolve_gl_surface_size(800, 600, 1600, 1200);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result.value().should_resize);
    GLINTFX_CHECK_EQ(result.value().width, static_cast<std::uint32_t>(800));
    GLINTFX_CHECK_EQ(result.value().height, static_cast<std::uint32_t>(600));
}

GLINTFX_TEST(resolve_gl_surface_size_never_accepts_a_zero_pixel_window_size) {
    const glintfx::gltfx_rslt<gl_surface_size_decision> width_zero =
        resolve_gl_surface_size(0, 600, 800, 600);
    GLINTFX_CHECK(width_zero.has_error());
    GLINTFX_CHECK(width_zero.err().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(width_zero.err().rejected_value() == "pixel_size");

    const glintfx::gltfx_rslt<gl_surface_size_decision> height_zero =
        resolve_gl_surface_size(800, 0, 800, 600);
    GLINTFX_CHECK(height_zero.has_error());
    GLINTFX_CHECK(height_zero.err().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(height_zero.err().rejected_value() == "pixel_size");

    std::println("resolve_gl_surface_size: 4/4 cases of this file's own closed enumeration passed");
}
