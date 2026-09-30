// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string>

#include <glintfx/core/err_code.hpp>

#include "draw2d/gl_load_refusal.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_load_refusal_test.cpp - R2D-BATCH, fatia B4: the word open() uses when the driver lacks a GL
// function. Pure: no context, no GL.

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::draw2d::gl_load_refusal;

GLINTFX_TEST(gl_load_refusal_not_found_becomes_unsupported_and_keeps_the_function_name) {
    const gltfx_err refusal = gl_load_refusal(
        gltfx_err(gltfx_err_code::not_found).with_rejected_value("glGenVertexArrays"));
    GLINTFX_CHECK(refusal.code() == gltfx_err_code::unsupported);
    GLINTFX_CHECK_EQ(std::string(refusal.rejected_value()), std::string("glGenVertexArrays"));
}

GLINTFX_TEST(gl_load_refusal_any_other_error_comes_back_as_it_was) {
    const gltfx_err other =
        gl_load_refusal(gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("x"));
    GLINTFX_CHECK(other.code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK_EQ(std::string(other.rejected_value()), std::string("x"));
    const gltfx_err oom = gl_load_refusal(gltfx_err(gltfx_err_code::out_of_memory));
    GLINTFX_CHECK(oom.code() == gltfx_err_code::out_of_memory);
}
