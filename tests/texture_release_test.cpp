// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>

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

// EGL_BAD_ACCESS: any nonzero value the code under test does not invent.
constexpr std::int64_t k_context_os_error = 0x3002;

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
        glintfx::gltfx_err error(gltfx_err_code::platform_failure);
        error.with_rejected_value("context").with_os_error_code(k_context_os_error);
        return gltfx_rslt<void>::err(error);
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

// The log sink of the test: every event, its text copied out (an event is valid only during the
// call), with the first two fields as integers.
struct captured_event {
    std::string name;
    std::string category;
    glintfx::gltfx_log_severity severity = glintfx::gltfx_log_severity::unknown;
    std::size_t field_count = 0;
    std::string first_field;
    std::uint64_t first_value = 0;
    std::string second_field;
    std::int64_t second_value = 0;
};
std::vector<captured_event> captured;

void capture_sink(void *, const glintfx::gltfx_log_event &event) noexcept {
    captured_event entry;
    entry.name = std::string(event.name());
    entry.category = std::string(event.category());
    entry.severity = event.severity();
    entry.field_count = event.fields().size();
    if (event.fields().size() >= 2) {
        entry.first_field = std::string(event.fields()[0].name);
        entry.first_value = event.fields()[0].value.unsigned_integer();
        entry.second_field = std::string(event.fields()[1].name);
        entry.second_value = event.fields()[1].value.signed_integer();
    }
    captured.push_back(entry);
}

// Arms the capture for one case and disarms it at the end, even when a check fails.
struct armed_log {
    armed_log() {
        captured.clear();
        glintfx::gltfx_log_set_sink(
            glintfx::gltfx_log_sink{&capture_sink, nullptr, glintfx::gltfx_log_severity::trace});
    }
    ~armed_log() { glintfx::gltfx_log_set_sink(glintfx::gltfx_log_sink{}); }
    armed_log(const armed_log &) = delete;
    armed_log &operator=(const armed_log &) = delete;
};

} // namespace

GLINTFX_TEST(the_release_makes_the_context_current_then_deletes_the_texture) {
    state = fake_state{};
    const armed_log log;
    release_texture_object(record());
    GLINTFX_CHECK(state.current_calls >= 1);
    GLINTFX_CHECK_EQ(state.gl_calls_at_first_current, 0);
    GLINTFX_CHECK_EQ(state.deleted.size(), 1U);
    GLINTFX_CHECK(!state.deleted.empty() && state.deleted.at(0) == 40U);
    GLINTFX_CHECK(captured.empty()); // a release that worked says nothing
}

// If the context cannot be made current, the graphics object goes with the context: nothing is
// deleted through a context that is not current, the record is still freed (the sanitizer run
// would see it leak), and the loss is never silent (CONTRACT.md §6.4): ONE event
// texture_release_failed, category draw2d, severity err, with the context's error code and its
// system code as typed integers.
GLINTFX_TEST(a_release_whose_context_cannot_be_made_current_deletes_nothing) {
    state = fake_state{};
    state.fail_current = true;
    const armed_log log;
    release_texture_object(record());
    GLINTFX_CHECK(state.current_calls >= 1);
    GLINTFX_CHECK(state.deleted.empty());
    GLINTFX_CHECK_EQ(captured.size(), 1U);
    const captured_event event = captured.empty() ? captured_event{} : captured.front();
    GLINTFX_CHECK(event.name == "texture_release_failed");
    GLINTFX_CHECK(event.category == "draw2d");
    GLINTFX_CHECK(event.severity == glintfx::gltfx_log_severity::err);
    GLINTFX_CHECK_EQ(event.field_count, 2U);
    GLINTFX_CHECK(event.first_field == "error_code");
    GLINTFX_CHECK_EQ(event.first_value,
                     static_cast<std::uint64_t>(gltfx_err_code::platform_failure));
    GLINTFX_CHECK(event.second_field == "os_error_code");
    GLINTFX_CHECK_EQ(event.second_value, k_context_os_error);
}

// T3 (D-W8-288): a record whose renderer HOLDS it (a frame is open) is handed over and released
// later, by the renderer; one whose renderer does not hold it (no frame open) is released at once.
namespace {

glintfx::texture_2d_impl *handed_over = nullptr;

bool fake_hold_taking(void *, glintfx::texture_2d_impl *texture) noexcept {
    handed_over = texture;
    return true;
}

bool fake_hold_refusing(void *, glintfx::texture_2d_impl *) noexcept { return false; }

[[nodiscard]] texture_2d_impl *record_held_by(bool (*hold)(void *, texture_2d_impl *) noexcept) {
    texture_2d_impl *made = record();
    made->access.hold = hold;
    return made;
}

} // namespace

GLINTFX_TEST(a_texture_its_renderer_holds_is_handed_over_and_not_released_now) {
    state = fake_state{};
    handed_over = nullptr;
    texture_2d_impl *made = record_held_by(&fake_hold_taking);
    release_texture_object(made);
    GLINTFX_CHECK(handed_over == made);
    GLINTFX_CHECK_EQ(state.current_calls, 0);
    GLINTFX_CHECK(state.deleted.empty());
    // What the renderer does when the frame ends (texture_release_queue.hpp).
    glintfx::draw2d::release_texture_now(handed_over);
    GLINTFX_CHECK_EQ(state.deleted.size(), 1U);
}

GLINTFX_TEST(a_texture_its_renderer_does_not_hold_is_released_at_once) {
    state = fake_state{};
    release_texture_object(record_held_by(&fake_hold_refusing));
    GLINTFX_CHECK(state.current_calls >= 1);
    GLINTFX_CHECK_EQ(state.gl_calls_at_first_current, 0);
    GLINTFX_CHECK_EQ(state.deleted.size(), 1U);
}

GLINTFX_TEST(releasing_nothing_touches_nothing) {
    state = fake_state{};
    release_texture_object(nullptr);
    GLINTFX_CHECK_EQ(state.current_calls, 0);
    GLINTFX_CHECK_EQ(state.gl_calls, 0);
}
