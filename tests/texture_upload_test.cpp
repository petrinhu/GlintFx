// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <span>
#include <string_view>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/texture_upload.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_upload_test.cpp - R2D-TEXTURE, fatia T2a: the making and the release of the GL texture
// object (draw2d/texture_upload.hpp), against a FAKE context and a table of FAKE GL functions that
// keep what the code asked of the card: the order against the context, the unpack state at each
// upload, every glTexImage2D with its bytes, the texture parameters, the deletions.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver. What a real card does with the
// texture is the live cell of the texture parity test (fatia T4b). The literal GL enumerators are
// the registry's (third_party/khronos/gl.xml), never the constants of the code under test, and the
// stored bytes are literals from the formula (the T1 order and the T2a order, item 3).

using glintfx::gltfx_all_mipmap_levels;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d_desc;
using glintfx::gltfx_texture_filter;
using glintfx::texture_2d_impl;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::create_texture_object;
using glintfx::draw2d::default_batch_allocator;
using glintfx::draw2d::release_texture_object;
using glintfx::draw2d::texture_gl_access;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

// gl.xml
constexpr GLenum k_texture_2d = 0x0DE1;
constexpr GLenum k_srgb8_alpha8 = 0x8C43;
constexpr GLenum k_rgba = 0x1908;
constexpr GLenum k_unsigned_byte = 0x1401;
constexpr GLenum k_base_level = 0x813C;
constexpr GLenum k_max_level = 0x813D;
constexpr GLenum k_unpack_alignment = 0x0CF5;
constexpr GLenum k_unpack_row_length = 0x0CF2;
constexpr GLenum k_unpack_skip_rows = 0x0CF3;
constexpr GLenum k_unpack_skip_pixels = 0x0CF4;
constexpr GLenum k_pixel_unpack_buffer = 0x88EC;
constexpr GLenum k_max_texture_size = 0x0D33;
constexpr GLenum k_texture0 = 0x84C0;
constexpr GLenum k_out_of_memory = 0x0505;
constexpr GLenum k_invalid_enum = 0x0500;
constexpr GLenum k_invalid_operation = 0x0502;

// One glTexImage2D as the card received it, with the state around it at that moment.
struct upload {
    GLenum target = 0;
    GLint level = -1;
    GLint internal_format = 0;
    GLsizei width = 0;
    GLsizei height = 0;
    GLint border = -1;
    GLenum format = 0;
    GLenum type = 0;
    std::vector<unsigned char> bytes;
    std::map<GLenum, GLint> unpack; // glPixelStorei values at this moment
    GLuint unpack_buffer = 0;
    GLuint bound_texture = 0;
    GLenum active_unit = 0;
};

struct fake_card {
    int gl_calls = 0;
    GLint max_side = 4096;
    std::vector<GLenum> error_queue;
    GLenum error_after_upload = 0;
    GLuint next_name = 40;
    std::vector<GLuint> generated;
    std::vector<GLuint> deleted;
    std::map<GLenum, GLint> unpack;
    GLuint unpack_buffer = 0;
    GLuint bound_texture = 0;
    GLenum active_unit = 0;
    std::map<GLenum, GLint> parameters; // glTexParameteri on GL_TEXTURE_2D
    std::vector<upload> uploads;
    int mipmap_generations = 0;
    int gl_calls_at_delete = -1;
};
fake_card card;

struct fake_context {
    bool fail = false;
    int current_calls = 0;
    int gl_calls_at_first_current = -1;
};
fake_context context;

int owner_token = 0;

void reset() {
    card = fake_card{};
    // State a careless consumer could have left behind: every one of these must be overwritten.
    card.unpack = {{k_unpack_alignment, 8},
                   {k_unpack_row_length, 3},
                   {k_unpack_skip_rows, 2},
                   {k_unpack_skip_pixels, 1}};
    card.unpack_buffer = 77;
    card.active_unit = k_texture0 + 3;
    context = fake_context{};
}

gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++context.current_calls;
    if (context.gl_calls_at_first_current < 0) {
        context.gl_calls_at_first_current = card.gl_calls;
    }
    if (context.fail) {
        return gltfx_rslt<void>::err(
            glintfx::gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("context"));
    }
    return gltfx_rslt<void>::ok();
}

GLenum GLINTFX_GL_APIENTRY f_get_error() {
    ++card.gl_calls;
    if (card.error_queue.empty()) {
        return 0;
    }
    const GLenum first = card.error_queue.front();
    card.error_queue.erase(card.error_queue.begin());
    return first;
}
void GLINTFX_GL_APIENTRY f_get_integer(GLenum pname, GLint *data) {
    ++card.gl_calls;
    *data = pname == k_max_texture_size ? card.max_side : 0;
}
void GLINTFX_GL_APIENTRY f_bind_buffer(GLenum target, GLuint name) {
    ++card.gl_calls;
    if (target == k_pixel_unpack_buffer) {
        card.unpack_buffer = name;
    }
}
void GLINTFX_GL_APIENTRY f_pixel_store(GLenum pname, GLint value) {
    ++card.gl_calls;
    card.unpack[pname] = value;
}
void GLINTFX_GL_APIENTRY f_active_texture(GLenum unit) {
    ++card.gl_calls;
    card.active_unit = unit;
}
void GLINTFX_GL_APIENTRY f_gen_textures(GLsizei n, GLuint *names) {
    ++card.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        names[i] = card.next_name++;
        card.generated.push_back(names[i]);
    }
}
void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei n, const GLuint *names) {
    ++card.gl_calls;
    card.gl_calls_at_delete = card.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        card.deleted.push_back(names[i]);
    }
}
void GLINTFX_GL_APIENTRY f_bind_texture(GLenum target, GLuint name) {
    ++card.gl_calls;
    if (target == k_texture_2d) {
        card.bound_texture = name;
    }
}
void GLINTFX_GL_APIENTRY f_tex_parameter(GLenum target, GLenum pname, GLint value) {
    ++card.gl_calls;
    if (target == k_texture_2d) {
        card.parameters[pname] = value;
    }
}
void GLINTFX_GL_APIENTRY f_tex_image(GLenum target, GLint level, GLint internal_format,
                                     GLsizei width, GLsizei height, GLint border, GLenum format,
                                     GLenum type, const void *pixels) {
    ++card.gl_calls;
    upload sent{target,
                level,
                internal_format,
                width,
                height,
                border,
                format,
                type,
                {},
                card.unpack,
                card.unpack_buffer,
                card.bound_texture,
                card.active_unit};
    if (pixels != nullptr && width > 0 && height > 0) {
        const auto *first = static_cast<const unsigned char *>(pixels);
        sent.bytes.assign(first, first + static_cast<std::size_t>(width) * height * 4);
    }
    card.uploads.push_back(sent);
    if (card.error_after_upload != 0) {
        card.error_queue.push_back(card.error_after_upload);
    }
}
void GLINTFX_GL_APIENTRY f_generate_mipmap(GLenum) {
    ++card.gl_calls;
    ++card.mipmap_generations;
}

[[nodiscard]] gl_function_table fake_table() {
    gl_function_table t;
    t.glGetError = &f_get_error;
    t.glGetIntegerv = &f_get_integer;
    t.glBindBuffer = &f_bind_buffer;
    t.glPixelStorei = &f_pixel_store;
    t.glActiveTexture = &f_active_texture;
    t.glGenTextures = &f_gen_textures;
    t.glDeleteTextures = &f_delete_textures;
    t.glBindTexture = &f_bind_texture;
    t.glTexParameteri = &f_tex_parameter;
    t.glTexImage2D = &f_tex_image;
    t.glGenerateMipmap = &f_generate_mipmap;
    return t;
}
const gl_function_table k_table = fake_table();

[[nodiscard]] texture_gl_access access() {
    return texture_gl_access{&context, &fake_make_current, &k_table, &owner_token};
}

// Bytes the tests own; every view reads from here.
std::vector<std::byte> pixels;

[[nodiscard]] gltfx_image_view view_of(std::initializer_list<std::uint8_t> rgba,
                                       std::uint32_t width, std::uint32_t height) {
    pixels.clear();
    for (const std::uint8_t value : rgba) {
        pixels.push_back(std::byte{value});
    }
    return gltfx_image_view{.pixels = std::span<const std::byte>(pixels),
                            .width = width,
                            .height = height,
                            .bytes_per_row = static_cast<std::size_t>(width) * 4,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

// A width x height opaque white image.
[[nodiscard]] gltfx_image_view white(std::uint32_t width, std::uint32_t height) {
    pixels.assign(static_cast<std::size_t>(width) * height * 4, std::byte{255});
    return gltfx_image_view{.pixels = std::span<const std::byte>(pixels),
                            .width = width,
                            .height = height,
                            .bytes_per_row = static_cast<std::size_t>(width) * 4,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

[[nodiscard]] gltfx_texture_2d_desc levels(std::uint32_t count) {
    gltfx_texture_2d_desc desc;
    desc.mipmap_levels = count;
    return desc;
}

[[nodiscard]] gltfx_rslt<texture_2d_impl *> make(const gltfx_image_view &image,
                                                 const gltfx_texture_2d_desc &desc = {}) {
    return create_texture_object(access(), image, desc, default_batch_allocator());
}

[[nodiscard]] bool refused_as(const gltfx_rslt<texture_2d_impl *> &result, gltfx_err_code code,
                              std::string_view field) {
    return result.has_error() && result.err().code() == code &&
           result.err().rejected_value() == field;
}

// Releases what a successful make() returned, so no case leaks.
void release(const gltfx_rslt<texture_2d_impl *> &made) {
    if (made.has_value()) {
        release_texture_object(made.value());
    }
}

void *refuse_everything(void *, std::size_t) noexcept { return nullptr; }

// A texture parameter as last set, or -999 when it was never set (a check fails, never throws).
[[nodiscard]] GLint parameter(GLenum pname) {
    const auto found = card.parameters.find(pname);
    return found == card.parameters.end() ? -999 : found->second;
}

} // namespace

GLINTFX_TEST(an_invalid_request_is_refused_before_the_context_is_touched) {
    reset();
    const auto made = make(white(0, 2));
    GLINTFX_CHECK(refused_as(made, gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK_EQ(context.current_calls, 0);
    GLINTFX_CHECK_EQ(card.gl_calls, 0);
}

GLINTFX_TEST(the_context_is_made_current_before_the_first_gl_call) {
    reset();
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK(context.current_calls >= 1);
    GLINTFX_CHECK_EQ(context.gl_calls_at_first_current, 0);
    release(made);
}

GLINTFX_TEST(a_context_that_cannot_be_made_current_passes_its_error_on_untouched) {
    reset();
    context.fail = true;
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(refused_as(made, gltfx_err_code::platform_failure, "context"));
    GLINTFX_CHECK_EQ(card.gl_calls, 0);
}

// The limit is read from the card, and each side is compared with it: a texture exactly at the
// limit is made, one texel more on EITHER side is not.
GLINTFX_TEST(a_side_larger_than_the_card_allows_is_unsupported_by_name) {
    reset();
    card.max_side = 4;
    const auto at_limit = make(white(4, 4));
    GLINTFX_CHECK(at_limit.has_value());
    release(at_limit);
    reset();
    card.max_side = 4;
    GLINTFX_CHECK(refused_as(make(white(5, 1)), gltfx_err_code::unsupported, "width"));
    reset();
    card.max_side = 4;
    GLINTFX_CHECK(refused_as(make(white(1, 5)), gltfx_err_code::unsupported, "height"));
    GLINTFX_CHECK(card.generated.empty());
}

GLINTFX_TEST(the_texture_is_uploaded_as_srgb8_alpha8_from_rgba_bytes) {
    reset();
    const auto made = make(white(3, 2));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK_EQ(card.uploads.size(), 1U);
    const upload &sent = card.uploads.at(0);
    GLINTFX_CHECK_EQ(sent.target, k_texture_2d);
    GLINTFX_CHECK_EQ(sent.level, 0);
    GLINTFX_CHECK_EQ(sent.internal_format, static_cast<GLint>(k_srgb8_alpha8));
    GLINTFX_CHECK_EQ(sent.width, 3);
    GLINTFX_CHECK_EQ(sent.height, 2);
    GLINTFX_CHECK_EQ(sent.border, 0);
    GLINTFX_CHECK_EQ(sent.format, k_rgba);
    GLINTFX_CHECK_EQ(sent.type, k_unsigned_byte);
    GLINTFX_CHECK_EQ(card.mipmap_generations, 0);
    release(made);
}

// The bytes on the card are the STORED texels (pixel_premultiply.hpp), not the caller's: sRGB
// straight (255, 0, 0, 128) stores as (188, 0, 0, 128), the exact case of the T1-fix1 order.
GLINTFX_TEST(the_bytes_uploaded_are_the_stored_texels) {
    reset();
    const auto made = make(view_of({255, 0, 0, 128, 0, 255, 0, 255}, 2, 1));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK_EQ(card.uploads.size(), 1U);
    const std::vector<unsigned char> expected = {188, 0, 0, 128, 0, 255, 0, 255};
    GLINTFX_CHECK(card.uploads.at(0).bytes == expected);
    release(made);
}

// Whatever the consumer left, the upload reads tightly packed rows from client memory, on unit 0.
GLINTFX_TEST(the_unpack_state_is_set_before_the_upload) {
    reset();
    const auto made = make(white(3, 3));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK_EQ(card.uploads.size(), 1U);
    const upload &sent = card.uploads.at(0);
    GLINTFX_CHECK_EQ(sent.unpack_buffer, 0U);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_alignment), 1);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_row_length), 0);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_skip_rows), 0);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_skip_pixels), 0);
    GLINTFX_CHECK_EQ(sent.active_unit, k_texture0);
    GLINTFX_CHECK_EQ(card.generated.size(), 1U);
    GLINTFX_CHECK_EQ(sent.bound_texture, card.generated.at(0));
    GLINTFX_CHECK_EQ(card.bound_texture, 0U);
    release(made);
}

// Without mipmaps the texture is complete with level 0 alone: GL_TEXTURE_MAX_LEVEL 0.
GLINTFX_TEST(no_mipmap_sets_the_max_level_to_zero) {
    reset();
    const auto made = make(white(4, 4));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK_EQ(parameter(k_base_level), 0);
    GLINTFX_CHECK_EQ(parameter(k_max_level), 0);
    GLINTFX_CHECK_EQ(card.uploads.size(), 1U);
    release(made);
}

// 4 x 2 with every level: 4 x 2, 2 x 1 and 1 x 1, computed by the library and uploaded one by
// one; glGenerateMipmap is never called. The 2 x 2 block of three white and one transparent
// texel reduces to (225, 225, 225, 191) (mipmap_reduce_test, the same formula).
GLINTFX_TEST(every_reduced_level_is_computed_and_uploaded) {
    reset();
    const auto made =
        make(view_of({255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255,
                      255, 255, 255, 255, 0,   0,   0,   0,   0, 0, 0, 255, 0, 0, 0, 255},
                     4, 2),
             levels(gltfx_all_mipmap_levels));
    GLINTFX_CHECK(made.has_value());
    GLINTFX_CHECK_EQ(card.mipmap_generations, 0);
    GLINTFX_CHECK_EQ(parameter(k_max_level), 2);
    GLINTFX_CHECK_EQ(card.uploads.size(), 3U);
    GLINTFX_CHECK_EQ(card.uploads.at(1).level, 1);
    GLINTFX_CHECK_EQ(card.uploads.at(1).width, 2);
    GLINTFX_CHECK_EQ(card.uploads.at(1).height, 1);
    GLINTFX_CHECK_EQ(card.uploads.at(1).internal_format, static_cast<GLint>(k_srgb8_alpha8));
    const std::vector<unsigned char> level_1 = {225, 225, 225, 191, 0, 0, 0, 255};
    GLINTFX_CHECK(card.uploads.at(1).bytes == level_1);
    GLINTFX_CHECK_EQ(card.uploads.at(2).level, 2);
    GLINTFX_CHECK_EQ(card.uploads.at(2).width, 1);
    GLINTFX_CHECK_EQ(card.uploads.at(2).height, 1);
    GLINTFX_CHECK(made.has_value() && made.value()->reduced_levels == 2U);
    release(made);
}

GLINTFX_TEST(the_card_running_out_of_memory_is_out_of_memory_with_the_gl_code) {
    reset();
    card.error_after_upload = k_out_of_memory;
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(refused_as(made, gltfx_err_code::out_of_memory, "texture_create"));
    GLINTFX_CHECK(made.has_error() && made.err().os_error_code() == k_out_of_memory);
    GLINTFX_CHECK_EQ(card.generated.size(), 1U);
    GLINTFX_CHECK(card.deleted == card.generated);
    GLINTFX_CHECK_EQ(card.bound_texture, 0U);
}

GLINTFX_TEST(another_gl_error_is_a_platform_failure_with_the_gl_code) {
    reset();
    card.error_after_upload = k_invalid_enum;
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(refused_as(made, gltfx_err_code::platform_failure, "texture_create"));
    GLINTFX_CHECK(made.has_error() && made.err().os_error_code() == k_invalid_enum);
    GLINTFX_CHECK(card.deleted == card.generated);
}

// An error the consumer left in the queue is not this call's.
GLINTFX_TEST(a_stale_gl_error_is_not_blamed_on_the_texture) {
    reset();
    card.error_queue = {k_invalid_operation, k_invalid_enum};
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(made.has_value());
    release(made);
}

// The library's own memory: refused before any GL object exists.
GLINTFX_TEST(the_library_running_out_of_memory_is_out_of_memory_with_no_gl_object) {
    reset();
    const batch_allocator refusing{&refuse_everything, default_batch_allocator().release};
    const auto made = create_texture_object(access(), white(2, 2), {}, refusing);
    GLINTFX_CHECK(refused_as(made, gltfx_err_code::out_of_memory, "texture_create"));
    GLINTFX_CHECK(made.has_error() && made.err().os_error_code() == 0);
    GLINTFX_CHECK(card.generated.empty());
}

GLINTFX_TEST(the_record_names_the_texture_its_size_and_its_owner) {
    reset();
    gltfx_texture_2d_desc desc = levels(1);
    desc.filter = gltfx_texture_filter::nearest;
    const auto made = make(white(5, 3), desc);
    GLINTFX_CHECK(made.has_value());
    const texture_2d_impl *record = made.has_value() ? made.value() : nullptr;
    GLINTFX_CHECK(record != nullptr);
    GLINTFX_CHECK_EQ(card.generated.size(), 1U);
    GLINTFX_CHECK_EQ(record->gl_name, card.generated.at(0));
    GLINTFX_CHECK_EQ(record->width, 5U);
    GLINTFX_CHECK_EQ(record->height, 3U);
    GLINTFX_CHECK_EQ(record->reduced_levels, 1U);
    GLINTFX_CHECK(record->filter == gltfx_texture_filter::nearest);
    GLINTFX_CHECK(record->access.owner == &owner_token);
    release(made);
}

// The public text: if the context cannot be made current, the graphics object goes with the
// context. Nothing is deleted through a context that is not current, and the record is still freed
// (the sanitizer run would see it leak).
GLINTFX_TEST(a_release_whose_context_cannot_be_made_current_deletes_nothing) {
    reset();
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(made.has_value());
    context = fake_context{};
    context.fail = true;
    card.deleted.clear();
    release(made);
    GLINTFX_CHECK(context.current_calls >= 1);
    GLINTFX_CHECK(card.deleted.empty());
}

GLINTFX_TEST(the_release_makes_the_context_current_then_deletes_the_texture) {
    reset();
    const auto made = make(white(2, 2));
    GLINTFX_CHECK(made.has_value());
    const GLuint name = card.generated.empty() ? 0U : card.generated.at(0);
    context = fake_context{};
    card.deleted.clear();
    card.gl_calls = 0;
    release(made);
    GLINTFX_CHECK(context.current_calls >= 1);
    GLINTFX_CHECK_EQ(context.gl_calls_at_first_current, 0);
    GLINTFX_CHECK_EQ(card.deleted.size(), 1U);
    GLINTFX_CHECK(!card.deleted.empty() && card.deleted.at(0) == name);
    release_texture_object(nullptr);
}
