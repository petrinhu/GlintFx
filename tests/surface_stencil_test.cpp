// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <string>
#include <vector>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>

#include "draw2d/surface_stencil.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// surface_stencil_test.cpp - R2D-SPRITE-SHADE H3b-2 (D-W8-1328). Proves surface_stencil.hpp against
// a gl_function_table of FAKES that answer as the drivers were measured to answer (llvmpipe, Mesa
// 26.1.8, $A/evidencia/sonda_bits.txt): GL_STENCIL with a framebuffer of the consumer bound is
// GL_INVALID_ENUM; the size asked of a surface with no stencil is GL_INVALID_OPERATION.
//
// THE RULE OF THE START (H3a-fix2): every item starts AWAY from what the function has to answer, so
// a case that passes cannot be passing by what it planted: the answer variable of each question
// holds the wrong value until the fake writes it, and a question the model does not know counts in
// dropped_calls.
using glintfx::draw2d::surface_has_stencil;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLuint;

namespace {

constexpr GLenum k_draw_framebuffer = 0x8CA9;
constexpr GLenum k_draw_framebuffer_binding = 0x8CA6;
constexpr GLenum k_stencil = 0x1802;
constexpr GLenum k_object_type = 0x8CD0;
constexpr GLenum k_stencil_size = 0x8217;
constexpr GLint k_none = 0;
constexpr GLint k_framebuffer_default = 0x8218;
constexpr GLenum k_invalid_enum = 0x0500;
constexpr GLenum k_invalid_operation = 0x0502;

struct card_model {
    GLuint draw_framebuffer = 0;
    GLint stencil_type = k_framebuffer_default;
    GLint stencil_size = 8;
    GLenum error_on_type = 0;
    GLenum error_on_size = 0;
    std::vector<GLenum> queue;
    std::vector<GLenum> asked; // the pname of each question, in order
    int dropped_calls = 0;
};

card_model model;

void GLINTFX_GL_APIENTRY fake_get_integerv(GLenum name, GLint *out) {
    if (name != k_draw_framebuffer_binding) {
        ++model.dropped_calls;
        return;
    }
    *out = static_cast<GLint>(model.draw_framebuffer);
}

void GLINTFX_GL_APIENTRY fake_bind_framebuffer(GLenum target, GLuint id) {
    if (target != k_draw_framebuffer) {
        ++model.dropped_calls;
        return;
    }
    model.draw_framebuffer = id;
}

void GLINTFX_GL_APIENTRY fake_attachment(GLenum target, GLenum attachment, GLenum pname,
                                         GLint *out) {
    model.asked.push_back(pname);
    if (target != k_draw_framebuffer || attachment != k_stencil) {
        ++model.dropped_calls;
        return;
    }
    if (model.draw_framebuffer != 0) {
        model.queue.push_back(k_invalid_enum); // measured: GL_STENCIL names the default framebuffer
        return;
    }
    if (pname == k_object_type) {
        if (model.error_on_type != 0) {
            model.queue.push_back(model.error_on_type);
            return;
        }
        *out = model.stencil_type;
        return;
    }
    if (pname != k_stencil_size) {
        ++model.dropped_calls;
        return;
    }
    if (model.stencil_type == k_none) {
        model.queue.push_back(k_invalid_operation); // measured on Mesa
        return;
    }
    if (model.error_on_size != 0) {
        model.queue.push_back(model.error_on_size);
        return;
    }
    *out = model.stencil_size;
}

GLenum GLINTFX_GL_APIENTRY fake_get_error() {
    if (model.queue.empty()) {
        return 0;
    }
    const GLenum first = model.queue.front();
    model.queue.erase(model.queue.begin());
    return first;
}

// The events of the log sink: their names and the first field, copied out during the call.
std::vector<std::string> event_names;
std::vector<std::uint64_t> event_errors;

void capture_sink(void *, const glintfx::gltfx_log_event &event) noexcept {
    event_names.emplace_back(event.name());
    event_errors.push_back(event.fields().empty() ? 0U
                                                  : event.fields()[0].value.unsigned_integer());
}

gl_function_table fake_table() {
    model = card_model{};
    event_names.clear();
    event_errors.clear();
    glintfx::gltfx_log_set_sink(
        glintfx::gltfx_log_sink{&capture_sink, nullptr, glintfx::gltfx_log_severity::trace});
    gl_function_table table;
    table.glGetIntegerv = &fake_get_integerv;
    table.glBindFramebuffer = &fake_bind_framebuffer;
    table.glGetFramebufferAttachmentParameteriv = &fake_attachment;
    table.glGetError = &fake_get_error;
    return table;
}

} // namespace

// The answer the driver gave on a surface with 8 bits: the type, then the size.
GLINTFX_TEST(surface_stencil_a_surface_with_bits_is_asked_the_type_and_then_the_size) {
    const gl_function_table table = fake_table();
    GLINTFX_CHECK(surface_has_stencil(table));
    GLINTFX_CHECK((model.asked == std::vector<GLenum>{k_object_type, k_stencil_size}));
    GLINTFX_CHECK_EQ(model.dropped_calls, 0);
    GLINTFX_CHECK(model.queue.empty());
    GLINTFX_CHECK(event_names.empty());
}

// No stencil attachment: no bits, and the size is NOT asked, which Mesa answers with an error.
GLINTFX_TEST(surface_stencil_a_surface_with_no_attachment_has_no_bits_and_no_error) {
    const gl_function_table table = fake_table();
    model.stencil_type = k_none;
    GLINTFX_CHECK(!surface_has_stencil(table));
    GLINTFX_CHECK((model.asked == std::vector<GLenum>{k_object_type}));
    GLINTFX_CHECK(model.queue.empty());
    GLINTFX_CHECK(event_names.empty());
}

// An attachment that answers a size of zero: no bits.
GLINTFX_TEST(surface_stencil_an_attachment_of_zero_bits_has_no_bits) {
    const gl_function_table table = fake_table();
    model.stencil_size = 0;
    GLINTFX_CHECK(!surface_has_stencil(table));
}

// An error on either question says nothing about the bits (SDL issue 12070; Khronos 59800): the
// surface counts as having them, the error is not left for the consumer, and the log says so, with
// the code of the GL error. The size that was never written stays 0, so the "present" can only come
// from the error's own path.
GLINTFX_TEST(surface_stencil_an_error_on_the_reading_counts_as_present_and_is_drained) {
    gl_function_table table = fake_table();
    model.stencil_type = k_none;
    model.error_on_type = k_invalid_enum;
    GLINTFX_CHECK(surface_has_stencil(table));
    GLINTFX_CHECK((model.asked == std::vector<GLenum>{k_object_type}));
    GLINTFX_CHECK(model.queue.empty());
    GLINTFX_CHECK((event_names == std::vector<std::string>{"draw2d_stencil_bits_unread"}));
    GLINTFX_CHECK((event_errors == std::vector<std::uint64_t>{k_invalid_enum}));
    table = fake_table();
    model.stencil_size = 0;
    model.error_on_size = k_invalid_operation;
    GLINTFX_CHECK(surface_has_stencil(table));
    GLINTFX_CHECK(model.queue.empty());
    GLINTFX_CHECK((event_names == std::vector<std::string>{"draw2d_stencil_bits_unread"}));
    GLINTFX_CHECK((event_errors == std::vector<std::uint64_t>{k_invalid_operation}));
}

// With a framebuffer of the consumer bound, the question goes to the default framebuffer, and the
// binding comes back as it was. A surface with no bits under a bound framebuffer answers "no bits":
// asking without binding would make an error of it, and an error reads "present".
GLINTFX_TEST(surface_stencil_a_framebuffer_of_the_consumer_does_not_change_the_answer) {
    const gl_function_table table = fake_table();
    model.draw_framebuffer = 7U;
    model.stencil_type = k_none;
    GLINTFX_CHECK(!surface_has_stencil(table));
    GLINTFX_CHECK_EQ(model.draw_framebuffer, 7U);
    GLINTFX_CHECK(model.queue.empty());
    GLINTFX_CHECK_EQ(model.dropped_calls, 0);
}

// What was in the queue before is not an answer to these questions: a surface with no bits and a
// stray error waiting answers "no bits".
GLINTFX_TEST(surface_stencil_an_error_already_in_the_queue_is_not_the_answer) {
    const gl_function_table table = fake_table();
    model.stencil_type = k_none;
    model.queue.push_back(k_invalid_operation);
    GLINTFX_CHECK(!surface_has_stencil(table));
    GLINTFX_CHECK(model.queue.empty());
}
