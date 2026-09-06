// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <print>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gl_context_desc_validation.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_context_desc_validation_test.cpp - GL-CONTEXT (docs/plano-w6b-
// placa-e-laco.md fatia 2b, D-W6b-14/16/17, GODS_LAWS.md L-20): the
// TDD red/green witness for glintfx::platform::validate_gl_context_
// desc() (src/platform/gl/gl_context_desc_validation.hpp) - the five
// cases the plan's own fatia 2b row names for this atom.
//
// RED, SEEN: before gl_context_desc_validation.{hpp,cpp} existed, this
// file's own #include line failed to compile - validate_gl_context_
// desc() was undeclared.

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_entry;
using glintfx::gltfx_gl_context_desc;
using glintfx::platform::validate_gl_context_desc;

GLINTFX_TEST(validate_gl_context_desc_accepts_an_empty_list) {
    const gltfx_gl_context_desc desc{.options = nullptr, .option_count = 0};
    const glintfx::gltfx_rslt<void> result = validate_gl_context_desc(desc);
    GLINTFX_CHECK(result.has_value());
}

GLINTFX_TEST(validate_gl_context_desc_accepts_vsync_on) {
    const std::array<gltfx_gfx_option_entry, 1> options{
        {{.id = gltfx_gfx_option::vsync, .value = 1}}};
    const gltfx_gl_context_desc desc{.options = options.data(), .option_count = options.size()};
    const glintfx::gltfx_rslt<void> result = validate_gl_context_desc(desc);
    GLINTFX_CHECK(result.has_value());
}

GLINTFX_TEST(validate_gl_context_desc_refuses_gpu_preference_other_than_no_preference) {
    // prefer_dedicated == 2 (sec. 11.2's own table) - inside the
    // registry's own [0, 2] range, so gfx_option_validation.hpp alone
    // would accept it; D-W6b-14's own reservation is what refuses it
    // here, until GL-GPU-PREFERENCE exists.
    const std::array<gltfx_gfx_option_entry, 1> options{
        {{.id = gltfx_gfx_option::gpu_preference, .value = 2}}};
    const gltfx_gl_context_desc desc{.options = options.data(), .option_count = options.size()};
    const glintfx::gltfx_rslt<void> result = validate_gl_context_desc(desc);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "gpu_preference");
}

GLINTFX_TEST(validate_gl_context_desc_refuses_a_repeated_id) {
    const std::array<gltfx_gfx_option_entry, 2> options{{
        {.id = gltfx_gfx_option::vsync, .value = 1},
        {.id = gltfx_gfx_option::vsync, .value = 0},
    }};
    const gltfx_gl_context_desc desc{.options = options.data(), .option_count = options.size()};
    const glintfx::gltfx_rslt<void> result = validate_gl_context_desc(desc);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "vsync");
}

GLINTFX_TEST(validate_gl_context_desc_refuses_a_null_pointer_with_a_positive_count) {
    const gltfx_gl_context_desc desc{.options = nullptr, .option_count = 1};
    const glintfx::gltfx_rslt<void> result = validate_gl_context_desc(desc);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "options");

    std::println(
        "validate_gl_context_desc: 5/5 cases of this file's own closed enumeration passed");
}
