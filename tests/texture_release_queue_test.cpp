// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_release_queue.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_release_queue_test.cpp - R2D-TEXTURE, fatia T3 (D-W8-288): the queue of the textures a
// renderer holds for release (draw2d/texture_release_queue.hpp), against a FAKE context and a FAKE
// glDeleteTextures. The records are built here by hand: holding and releasing do not depend on how
// a texture was made. Holding releases nothing; releasing releases every one, in the order they
// came, each after its context was made current, and empties the queue.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver; the release at the end of a real
// frame is the cell TD of draw2d_parity_test.

using glintfx::gltfx_rslt;
using glintfx::texture_2d_impl;
using glintfx::draw2d::texture_gl_access;
using glintfx::draw2d::texture_release_queue;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

struct fake_state {
    int current_calls = 0;
    std::vector<GLuint> deleted;
    std::vector<int> current_calls_at_delete;
};
fake_state state;

gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++state.current_calls;
    return gltfx_rslt<void>::ok();
}

void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei n, const GLuint *names) {
    for (GLsizei i = 0; i < n; ++i) {
        state.deleted.push_back(names[i]);
        state.current_calls_at_delete.push_back(state.current_calls);
    }
}
GLenum GLINTFX_GL_APIENTRY f_get_error() { return 0; }

[[nodiscard]] gl_function_table fake_table() {
    gl_function_table t;
    t.glDeleteTextures = &f_delete_textures;
    t.glGetError = &f_get_error;
    return t;
}
const gl_function_table k_table = fake_table();
int owner_token = 0;

// A record as the making leaves it, with GL name `name`, on the fake context. Freed by the release.
[[nodiscard]] texture_2d_impl *record(std::uint32_t name) {
    return new texture_2d_impl{
        .access = texture_gl_access{&state, &fake_make_current, &k_table, &owner_token},
        .gl_name = name,
        .width = 2U,
        .height = 2U,
        .filter = glintfx::gltfx_texture_filter::linear,
        .reduced_levels = 0U,
    };
}

} // namespace

GLINTFX_TEST(holding_releases_nothing) {
    state = fake_state{};
    texture_release_queue queue;
    GLINTFX_CHECK(queue.empty());
    queue.hold(record(40U));
    GLINTFX_CHECK(!queue.empty());
    GLINTFX_CHECK(state.deleted.empty());
    GLINTFX_CHECK_EQ(state.current_calls, 0);
    queue.release_held();
}

// Three, so that a list that loses its middle (or its end) shows: 40, 41 and 42, in that order,
// each deleted after its context was made current once more.
GLINTFX_TEST(releasing_releases_every_held_texture_in_order_after_its_context) {
    state = fake_state{};
    texture_release_queue queue;
    queue.hold(record(40U));
    queue.hold(record(41U));
    queue.hold(record(42U));
    queue.release_held();
    GLINTFX_CHECK(queue.empty());
    const std::vector<GLuint> in_order{40U, 41U, 42U};
    GLINTFX_CHECK(state.deleted == in_order);
    const std::vector<int> after_current{1, 2, 3};
    GLINTFX_CHECK(state.current_calls_at_delete == after_current);
}

GLINTFX_TEST(releasing_an_empty_queue_does_nothing_and_it_can_hold_again) {
    state = fake_state{};
    texture_release_queue queue;
    queue.release_held();
    GLINTFX_CHECK_EQ(state.current_calls, 0);
    queue.hold(record(43U));
    queue.release_held();
    queue.release_held();
    const std::vector<GLuint> once{43U};
    GLINTFX_CHECK(state.deleted == once);
    GLINTFX_CHECK(queue.empty());
}
