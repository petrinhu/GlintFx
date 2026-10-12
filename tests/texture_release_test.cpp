// SPDX-License-Identifier: AGPL-3.0-or-later
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_release.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_release_test.cpp - R2D-TEXTURE, fatia T2a-fix1: the release of the GL texture object
// (draw2d/texture_release.hpp), against a FAKE context and the two FAKE GL functions it may call.
// The record is built here by hand: the release does not depend on how the texture was made.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver; the release on a real context is
// the destruction of the handle in draw2d_parity_test.

using glintfx::gltfx_err_code;
using glintfx::gltfx_rslt;
using glintfx::texture_2d_impl;
using glintfx::draw2d::release_texture_object;
using glintfx::draw2d::texture_gl_access;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

struct fake_state {
    bool fail_current = false;
    int current_calls = 0;
    int gl_calls = 0;
    int gl_calls_at_first_current = -1;
    std::vector<GLuint> deleted;
};
fake_state state;

gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++state.current_calls;
    if (state.gl_calls_at_first_current < 0) {
        state.gl_calls_at_first_current = state.gl_calls;
    }
    if (state.fail_current) {
        return gltfx_rslt<void>::err(
            glintfx::gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("context"));
    }
    return gltfx_rslt<void>::ok();
}

void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei n, const GLuint *names) {
    ++state.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        state.deleted.push_back(names[i]);
    }
}
GLenum GLINTFX_GL_APIENTRY f_get_error() {
    ++state.gl_calls;
    return 0;
}

[[nodiscard]] gl_function_table fake_table() {
    gl_function_table t;
    t.glDeleteTextures = &f_delete_textures;
    t.glGetError = &f_get_error;
    return t;
}
const gl_function_table k_table = fake_table();
int owner_token = 0;

// A record as the making leaves it: GL name 40, on the fake context. Freed by the release.
[[nodiscard]] texture_2d_impl *record() {
    return new texture_2d_impl{
        .access = texture_gl_access{&state, &fake_make_current, &k_table, &owner_token},
        .gl_name = 40U,
        .width = 2U,
        .height = 2U,
        .filter = glintfx::gltfx_texture_filter::linear,
        .reduced_levels = 0U,
    };
}

} // namespace

GLINTFX_TEST(the_release_makes_the_context_current_then_deletes_the_texture) {
    state = fake_state{};
    release_texture_object(record());
    GLINTFX_CHECK(state.current_calls >= 1);
    GLINTFX_CHECK_EQ(state.gl_calls_at_first_current, 0);
    GLINTFX_CHECK_EQ(state.deleted.size(), 1U);
    GLINTFX_CHECK(!state.deleted.empty() && state.deleted.at(0) == 40U);
}

// If the context cannot be made current, the graphics object goes with the context: nothing is
// deleted through a context that is not current, and the record is still freed (the sanitizer run
// would see it leak).
GLINTFX_TEST(a_release_whose_context_cannot_be_made_current_deletes_nothing) {
    state = fake_state{};
    state.fail_current = true;
    release_texture_object(record());
    GLINTFX_CHECK(state.current_calls >= 1);
    GLINTFX_CHECK(state.deleted.empty());
}

GLINTFX_TEST(releasing_nothing_touches_nothing) {
    state = fake_state{};
    release_texture_object(nullptr);
    GLINTFX_CHECK_EQ(state.current_calls, 0);
    GLINTFX_CHECK_EQ(state.gl_calls, 0);
}
