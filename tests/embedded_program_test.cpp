// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <cstring>
#include <print>
#include <string>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/sink.hpp>

#include "draw2d/embedded_program.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// embedded_program_test.cpp - R2D-BATCH, fatia B3b (docs/plano-w7d.md sec. 4.3, D-W7D-15,
// PLANO-errata.md D-API-10). Proves the compile-and-link of the embedded drawing program against a
// gl_function_table of FAKES: which token an error carries, that the driver's message goes to the
// log (as a field, truncated) and never into the error, and that no GL object outlives a failure.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): there is no real context here, so this proves the
// sequencing and the error/log routing, NEVER that a real driver accepts the GLSL text. That is
// the job of the first cell of tests/parity/draw2d_parity_test.cpp, which runs in the container
// against a real context.
//
// THE LITERAL TOKENS asserted below are the ones frozen in the header of B0 ("vertex_shader",
// "fragment_shader", "program_link"; event "draw2d_program_rejected", field "driver_log"), never
// through the internal constants the code uses (D-P1-5's rule).

using glintfx::gltfx_err_code;
using glintfx::gltfx_log_event;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::draw2d::create_embedded_program;
using glintfx::draw2d::destroy_embedded_program;
using glintfx::draw2d::embedded_program;
using glintfx::render::gl_function_table;
using glintfx::render::GLchar;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

constexpr GLenum k_vertex_shader = 0x8B31;
constexpr GLenum k_fragment_shader = 0x8B30;
constexpr GLenum k_compile_status = 0x8B81;
constexpr GLenum k_link_status = 0x8B82;
constexpr GLenum k_info_log_length = 0x8B84;

// What the fake driver does and what it saw.
struct fake_driver {
    bool fail_vertex = false;
    bool fail_fragment = false;
    bool fail_link = false;
    bool create_shader_fails = false;      // glCreateShader answers 0
    bool create_program_fails = false;     // glCreateProgram answers 0
    int calls_on_zero_program = 0;         // attach/link/getprogramiv/getinfolog on program 0
    std::array<bool, 8> shader_attached{}; // a shader attached is only MARKED for deletion
    std::array<bool, 8> shader_delete_marked{};
    bool lie_about_length = false; // reports the FULL length of the message even when it was cut
    std::string message;           // the driver's own text for a SHADER that fails
    std::string program_message;   // and for the PROGRAM link (a different log, on purpose)
    std::array<GLenum, 8> shader_type{};
    std::array<bool, 8> shader_alive{};
    std::array<const char *, 8> shader_source{};
    GLuint shaders_created = 0;
    bool program_alive = false;
    GLuint program_id = 0;
    int programs_created = 0;
    int attach_calls = 0;
};

fake_driver driver;

void reset_driver() { driver = fake_driver{}; }

int live_shaders() {
    int alive = 0;
    for (const bool flag : driver.shader_alive) {
        alive += flag ? 1 : 0;
    }
    return alive;
}

GLuint GLINTFX_GL_APIENTRY fake_create_shader(GLenum type) {
    if (driver.create_shader_fails) {
        return 0;
    }
    ++driver.shaders_created;
    const GLuint id = driver.shaders_created;
    driver.shader_type[id] = type;
    driver.shader_alive[id] = true;
    return id;
}
void GLINTFX_GL_APIENTRY fake_shader_source(GLuint shader, GLsizei, const GLchar *const *strings,
                                            const GLint *) {
    driver.shader_source[shader] = strings[0];
}
void GLINTFX_GL_APIENTRY fake_compile_shader(GLuint) {}
void GLINTFX_GL_APIENTRY fake_get_shaderiv(GLuint shader, GLenum pname, GLint *params) {
    if (pname == k_compile_status) {
        const bool fails =
            (driver.shader_type[shader] == k_vertex_shader && driver.fail_vertex) ||
            (driver.shader_type[shader] == k_fragment_shader && driver.fail_fragment);
        *params = fails ? 0 : 1;
    } else if (pname == k_info_log_length) {
        *params = static_cast<GLint>(driver.message.size() + 1);
    }
}
void copy_log(const std::string &message, GLsizei capacity, GLsizei *length, GLchar *buffer) {
    const auto room = static_cast<std::size_t>(capacity > 0 ? capacity - 1 : 0);
    const std::size_t count = message.size() < room ? message.size() : room;
    std::memcpy(buffer, message.data(), count);
    buffer[count] = '\0';
    *length = driver.lie_about_length ? static_cast<GLsizei>(message.size())
                                      : static_cast<GLsizei>(count);
}
void GLINTFX_GL_APIENTRY fake_get_shader_info_log(GLuint, GLsizei capacity, GLsizei *length,
                                                  GLchar *buffer) {
    copy_log(driver.message, capacity, length, buffer);
}
// Deleting a shader that is still ATTACHED only marks it (as GL does): it is freed when it is
// detached.
void GLINTFX_GL_APIENTRY fake_delete_shader(GLuint shader) {
    driver.shader_delete_marked[shader] = true;
    if (!driver.shader_attached[shader]) {
        driver.shader_alive[shader] = false;
    }
}
GLuint GLINTFX_GL_APIENTRY fake_create_program() {
    if (driver.create_program_fails) {
        return 0;
    }
    ++driver.programs_created;
    driver.program_id = 40;
    driver.program_alive = true;
    return driver.program_id;
}
void GLINTFX_GL_APIENTRY fake_attach_shader(GLuint program, GLuint shader) {
    ++driver.attach_calls;
    driver.calls_on_zero_program += program == 0 ? 1 : 0;
    driver.shader_attached[shader] = true;
}
void GLINTFX_GL_APIENTRY fake_detach_shader(GLuint, GLuint shader) {
    driver.shader_attached[shader] = false;
    if (driver.shader_delete_marked[shader]) {
        driver.shader_alive[shader] = false;
    }
}
void GLINTFX_GL_APIENTRY fake_link_program(GLuint program) {
    driver.calls_on_zero_program += program == 0 ? 1 : 0;
}
void GLINTFX_GL_APIENTRY fake_get_programiv(GLuint program, GLenum pname, GLint *params) {
    driver.calls_on_zero_program += program == 0 ? 1 : 0;
    if (pname == k_link_status) {
        *params = driver.fail_link ? 0 : 1;
    } else if (pname == k_info_log_length) {
        *params = static_cast<GLint>(driver.program_message.size() + 1);
    }
}
void GLINTFX_GL_APIENTRY fake_get_program_info_log(GLuint program, GLsizei capacity,
                                                   GLsizei *length, GLchar *buffer) {
    driver.calls_on_zero_program += program == 0 ? 1 : 0;
    copy_log(driver.program_message, capacity, length, buffer);
}
// Deleting a program detaches every shader still attached to it (as GL does).
void GLINTFX_GL_APIENTRY fake_delete_program(GLuint) {
    driver.program_alive = false;
    for (std::size_t i = 0; i < driver.shader_attached.size(); ++i) {
        if (driver.shader_attached[i]) {
            driver.shader_attached[i] = false;
            if (driver.shader_delete_marked[i]) {
                driver.shader_alive[i] = false;
            }
        }
    }
}
GLint GLINTFX_GL_APIENTRY fake_get_uniform_location(GLuint, const GLchar *name) {
    return std::strcmp(name, "u_viewport_pixels") == 0 ? 7 : -1;
}

gl_function_table fake_table() {
    gl_function_table table;
    table.glCreateShader = &fake_create_shader;
    table.glShaderSource = &fake_shader_source;
    table.glCompileShader = &fake_compile_shader;
    table.glGetShaderiv = &fake_get_shaderiv;
    table.glGetShaderInfoLog = &fake_get_shader_info_log;
    table.glDeleteShader = &fake_delete_shader;
    table.glCreateProgram = &fake_create_program;
    table.glAttachShader = &fake_attach_shader;
    table.glDetachShader = &fake_detach_shader;
    table.glLinkProgram = &fake_link_program;
    table.glGetProgramiv = &fake_get_programiv;
    table.glGetProgramInfoLog = &fake_get_program_info_log;
    table.glDeleteProgram = &fake_delete_program;
    table.glGetUniformLocation = &fake_get_uniform_location;
    return table;
}

// The log sink of the test: the LAST event only, its text copied out (the event is valid only
// during the call).
struct captured_event {
    int count = 0;
    gltfx_log_severity severity = gltfx_log_severity::unknown;
    std::string category;
    std::string name;
    int field_count = 0;
    std::string first_field_name;
    std::string first_field_text;
    bool second_field_is_boolean = false;
    bool second_field_value = false;
    std::string second_field_name;
    std::string third_field_name;
    std::string third_field_text;
};
captured_event captured;

void capture_sink(void *, const gltfx_log_event &event) noexcept {
    ++captured.count;
    captured.severity = event.severity();
    captured.category = std::string(event.category());
    captured.name = std::string(event.name());
    captured.field_count = static_cast<int>(event.fields().size());
    if (!event.fields().empty()) {
        captured.first_field_name = std::string(event.fields()[0].name);
        captured.first_field_text = std::string(event.fields()[0].value.text());
    }
    if (event.fields().size() > 1) {
        captured.second_field_name = std::string(event.fields()[1].name);
        captured.second_field_is_boolean =
            event.fields()[1].value.kind == glintfx::gltfx_log_value_kind::boolean;
        captured.second_field_value = event.fields()[1].value.boolean();
    }
    if (event.fields().size() > 2) {
        captured.third_field_name = std::string(event.fields()[2].name);
        captured.third_field_text = std::string(event.fields()[2].value.text());
    }
}

void arm() {
    reset_driver();
    captured = captured_event{};
    gltfx_log_set_sink(gltfx_log_sink{&capture_sink, nullptr, gltfx_log_severity::trace});
}
void disarm() { gltfx_log_set_sink(gltfx_log_sink{}); }

} // namespace

GLINTFX_TEST(embedded_program_success_links_and_leaves_only_the_program_alive) {
    arm();
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK_EQ(result.value().program, GLuint{40});
    GLINTFX_CHECK_EQ(result.value().viewport_location, 7);
    GLINTFX_CHECK_EQ(driver.shaders_created, GLuint{2});
    GLINTFX_CHECK_EQ(driver.shader_type[1], k_vertex_shader);
    GLINTFX_CHECK_EQ(driver.shader_type[2], k_fragment_shader);
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    GLINTFX_CHECK(driver.program_alive);
    GLINTFX_CHECK_EQ(driver.attach_calls, 2);
    GLINTFX_CHECK_EQ(captured.count, 0); // a success says nothing to the log
    // the text handed to the driver is exactly the embedded one
    GLINTFX_CHECK(driver.shader_source[1] ==
                  glintfx::draw2d::embedded_vertex_shader_source().data());
    GLINTFX_CHECK(driver.shader_source[2] ==
                  glintfx::draw2d::embedded_fragment_shader_source().data());
    embedded_program program = result.value();
    destroy_embedded_program(table, program);
    GLINTFX_CHECK(!driver.program_alive);
    GLINTFX_CHECK_EQ(program.program, GLuint{0});
    GLINTFX_CHECK_EQ(program.viewport_location, -1);
    disarm();
}

GLINTFX_TEST(embedded_program_vertex_failure_has_its_token_and_routes_the_message_to_the_log) {
    arm();
    driver.fail_vertex = true;
    driver.message = "0:3(1): error: syntax error";
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "vertex_shader");
    // the driver's sentence is NOT in the error...
    GLINTFX_CHECK(result.err().rejected_value().find("syntax") == std::string_view::npos);
    // ...it is the field `driver_log` of the event `draw2d_program_rejected`
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK(captured.name == "draw2d_program_rejected");
    GLINTFX_CHECK(captured.category == "draw2d");
    GLINTFX_CHECK(captured.severity == gltfx_log_severity::err);
    GLINTFX_CHECK_EQ(captured.field_count, 3);
    GLINTFX_CHECK(captured.first_field_name == "driver_log");
    GLINTFX_CHECK(captured.first_field_text == "0:3(1): error: syntax error");
    GLINTFX_CHECK(captured.second_field_name == "driver_log_truncated");
    GLINTFX_CHECK(captured.second_field_is_boolean);
    GLINTFX_CHECK(!captured.second_field_value); // it fit: not truncated
    GLINTFX_CHECK(captured.third_field_name == "reason");
    GLINTFX_CHECK(captured.third_field_text == "shader_compile_failed");
    // nothing left alive, and the fragment shader was never even created
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    GLINTFX_CHECK(!driver.program_alive);
    GLINTFX_CHECK_EQ(driver.shaders_created, GLuint{1});
    GLINTFX_CHECK_EQ(driver.programs_created, 0);
    disarm();
}

GLINTFX_TEST(embedded_program_fragment_failure_has_its_token_and_frees_the_vertex_shader) {
    arm();
    driver.fail_fragment = true;
    driver.message = "fragment says no";
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "fragment_shader");
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK(captured.first_field_text == "fragment says no");
    GLINTFX_CHECK_EQ(driver.shaders_created, GLuint{2});
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    GLINTFX_CHECK(!driver.program_alive);
    disarm();
}

GLINTFX_TEST(embedded_program_link_failure_has_its_token_and_frees_everything) {
    arm();
    driver.fail_link = true;
    driver.program_message = "link: undefined varying";
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "program_link");
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK(captured.name == "draw2d_program_rejected");
    GLINTFX_CHECK(captured.first_field_text == "link: undefined varying");
    GLINTFX_CHECK_EQ(driver.programs_created, 1);
    GLINTFX_CHECK(!driver.program_alive);
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    disarm();
}

// The driver's message can be any length: it is cut to the stack buffer (1 KiB, one byte of it the
// terminator), never refused, never overrun.
GLINTFX_TEST(embedded_program_a_long_driver_message_is_truncated_not_refused) {
    arm();
    driver.fail_vertex = true;
    driver.message = std::string(5000, 'x');
    driver.message[0] = 'A';
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().rejected_value() == "vertex_shader");
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK_EQ(captured.first_field_text.size(), std::size_t{1023});
    GLINTFX_CHECK(captured.first_field_text[0] == 'A');
    GLINTFX_CHECK(captured.first_field_text.find_first_not_of('x', 1) == std::string::npos);
    GLINTFX_CHECK(captured.second_field_is_boolean);
    GLINTFX_CHECK(captured.second_field_value); // and the cut is SAID
    disarm();
}

// An empty driver message is still one event, with an empty field (the rejection is what matters).
GLINTFX_TEST(embedded_program_an_empty_driver_message_still_logs_the_rejection) {
    arm();
    driver.fail_link = true;
    driver.program_message = "";
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK_EQ(captured.field_count, 3);
    GLINTFX_CHECK(captured.first_field_text.empty());
    GLINTFX_CHECK(!captured.second_field_value);
    disarm();
}

GLINTFX_TEST(embedded_program_destroy_on_an_empty_program_is_harmless) {
    arm();
    const gl_function_table table = fake_table();
    embedded_program program;
    destroy_embedded_program(table, program);
    GLINTFX_CHECK_EQ(program.program, GLuint{0});
    disarm();
}

// The two sources: GLSL 330 core, the three attribute locations the constants name, the one
// uniform the program reads.
GLINTFX_TEST(embedded_program_sources_are_glsl_330_core_with_the_agreed_interface) {
    const std::string_view vertex = glintfx::draw2d::embedded_vertex_shader_source();
    const std::string_view fragment = glintfx::draw2d::embedded_fragment_shader_source();
    GLINTFX_CHECK(vertex.starts_with("#version 330 core"));
    GLINTFX_CHECK(fragment.starts_with("#version 330 core"));
    GLINTFX_CHECK(vertex.find("layout(location = 0) in vec2 a_position") != std::string_view::npos);
    GLINTFX_CHECK(vertex.find("layout(location = 1) in vec2 a_texcoord") != std::string_view::npos);
    GLINTFX_CHECK(vertex.find("layout(location = 2) in vec4 a_color") != std::string_view::npos);
    GLINTFX_CHECK(vertex.find("uniform vec2 u_viewport_pixels") != std::string_view::npos);
    GLINTFX_CHECK(fragment.find("out vec4 frag_color") != std::string_view::npos);
    // the sources are NUL-terminated views (they are handed to glShaderSource as C strings)
    GLINTFX_CHECK(vertex.data()[vertex.size()] == '\0');
    GLINTFX_CHECK(fragment.data()[fragment.size()] == '\0');
    GLINTFX_CHECK_EQ(glintfx::draw2d::k_attribute_position, std::uint32_t{0});
    GLINTFX_CHECK_EQ(glintfx::draw2d::k_attribute_texcoord, std::uint32_t{1});
    GLINTFX_CHECK_EQ(glintfx::draw2d::k_attribute_color, std::uint32_t{2});
    std::println("embedded_program_test: fontes conferidas ({} + {} bytes)", vertex.size(),
                 fragment.size());
}

// A driver that reports a length LONGER than what it wrote (or than the buffer) must not make the
// library read past its own buffer: the field is cut to the buffer, whatever the driver says.
GLINTFX_TEST(embedded_program_a_driver_that_overstates_the_length_cannot_overrun_the_buffer) {
    arm();
    driver.fail_vertex = true;
    driver.lie_about_length = true;
    driver.message = std::string(5000, 'y');
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK_EQ(captured.first_field_text.size(), std::size_t{1023});
    GLINTFX_CHECK(captured.second_field_value); // the driver's info-log length is the truth
    disarm();
}

// The boundary of the cut, both sides: 1023 characters plus the terminator FIT the 1024-byte buffer
// (not truncated); 1024 characters do not (truncated, and the text is still the first 1023).
GLINTFX_TEST(embedded_program_truncation_is_said_exactly_at_the_capacity_boundary) {
    for (const std::size_t size : {std::size_t{1023}, std::size_t{1024}}) {
        arm();
        driver.fail_fragment = true;
        driver.message = std::string(size, 'z');
        const gl_function_table table = fake_table();
        auto result = create_embedded_program(table);
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK_EQ(captured.count, 1);
        GLINTFX_CHECK_EQ(captured.first_field_text.size(), std::size_t{1023});
        GLINTFX_CHECK(captured.second_field_is_boolean);
        GLINTFX_CHECK_EQ(captured.second_field_value, size == 1024);
        disarm();
    }
}

// The link path reports the truncation too (it reads the PROGRAM's log, not a shader's).
GLINTFX_TEST(embedded_program_a_long_link_message_says_it_was_truncated) {
    arm();
    driver.fail_link = true;
    driver.program_message = std::string(3000, 'k');
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().rejected_value() == "program_link");
    GLINTFX_CHECK(captured.second_field_value);
    disarm();
}

// glCreateProgram answering 0: nothing to attach to. The library must NOT call attach, link or
// getprogramiv on program 0 (each would leave GL_INVALID_VALUE in the consumer's error queue,
// R-B3), must free both shaders, and refuses as a link failure with the reason as a token.
GLINTFX_TEST(embedded_program_a_zero_program_is_refused_without_touching_program_zero) {
    arm();
    driver.create_program_fails = true;
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "program_link");
    GLINTFX_CHECK_EQ(driver.calls_on_zero_program, 0);
    GLINTFX_CHECK_EQ(driver.attach_calls, 0);
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK(captured.name == "draw2d_program_rejected");
    GLINTFX_CHECK(captured.first_field_text.empty()); // no driver message: it was not the driver's
    GLINTFX_CHECK(captured.third_field_text == "create_program_returned_zero");
    disarm();
}

// glCreateShader answering 0: the reason is a TOKEN and driver_log stays empty (a sentence of ours
// has no place in the event, R7).
GLINTFX_TEST(embedded_program_a_zero_shader_says_why_by_token_with_an_empty_driver_log) {
    arm();
    driver.create_shader_fails = true;
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().rejected_value() == "vertex_shader");
    GLINTFX_CHECK_EQ(captured.count, 1);
    GLINTFX_CHECK(captured.first_field_text.empty());
    GLINTFX_CHECK(!captured.second_field_value);
    GLINTFX_CHECK(captured.third_field_name == "reason");
    GLINTFX_CHECK(captured.third_field_text == "create_shader_returned_zero");
    GLINTFX_CHECK_EQ(live_shaders(), 0);
    disarm();
}

// The link path says why too.
GLINTFX_TEST(embedded_program_a_link_failure_says_program_link_failed) {
    arm();
    driver.fail_link = true;
    driver.program_message = "no";
    const gl_function_table table = fake_table();
    auto result = create_embedded_program(table);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(captured.third_field_text == "program_link_failed");
    disarm();
}
