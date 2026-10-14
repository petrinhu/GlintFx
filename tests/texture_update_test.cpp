// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/mipmap_reduce.hpp"
#include "draw2d/texture_update.hpp"
#include "draw2d/texture_upload.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_update_test.cpp - R2D-TEXTURE, fatia T3: the writing of an accepted update_texture()
// (draw2d/texture_update.hpp) into the GL texture object, against a FAKE context and a table of
// FAKE GL functions that keep what the code asked of the card: the order against the context, the
// unpack state at each glTexSubImage2D, every glTexSubImage2D with its rectangle and its bytes, the
// binding left behind. The textures are made by the making (texture_upload.hpp, proved by
// texture_upload_test) over the same fake card.
//
// THE ORACLE OF THE REDUCED LEVELS IS NOT THIS CODE: a texture made from the image, then updated,
// must keep the very texels of a texture MADE from the whole new image (the image with the
// rectangle pasted in), which the making computes level by level (mipmap_reduce_test). The touched
// rectangles of each level are the brute-force values of contas_t3b.py (ordem T3, Anexo C), and the
// stored bytes the literals of texture_upload_test (255, 0, 0, 128 is stored as 188, 0, 0, 128).
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver; the call on a real context is the
// cell TU of draw2d_parity_test.

using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rgba8;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d_desc;
using glintfx::texture_2d_impl;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::create_texture_object;
using glintfx::draw2d::default_batch_allocator;
using glintfx::draw2d::texel_rect;
using glintfx::draw2d::texture_gl_access;
using glintfx::draw2d::update_texture_object;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

// gl.xml
constexpr GLenum k_texture_2d = 0x0DE1;
constexpr GLenum k_rgba = 0x1908;
constexpr GLenum k_unsigned_byte = 0x1401;
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

// One glTexSubImage2D as the card received it, with the state around it at that moment.
struct sub_upload {
    GLint level = -1;
    GLint x = -1;
    GLint y = -1;
    GLsizei width = 0;
    GLsizei height = 0;
    GLenum format = 0;
    GLenum type = 0;
    std::vector<unsigned char> bytes; // the rectangle, read as GL reads it with the unpack state
    std::map<GLenum, GLint> unpack;
    GLuint unpack_buffer = 0;
    GLuint bound_texture = 0;
    GLenum active_unit = 0;
};

struct fake_card {
    int gl_calls = 0;
    std::vector<GLenum> error_queue;
    GLenum error_after_sub_upload = 0;
    GLuint next_name = 40;
    std::map<GLenum, GLint> unpack;
    GLuint unpack_buffer = 0;
    GLuint bound_texture = 0;
    GLenum active_unit = 0;
    std::vector<sub_upload> sub_uploads;
    std::vector<unsigned char> uploaded; // the bytes of every glTexImage2D, level after level
};
fake_card card;

struct fake_context {
    bool fail = false;
    int current_calls = 0;
    int gl_calls_at_last_current = -1;
};
fake_context context;

int owner_token = 0;

// State a careless consumer could have left behind: every one of these must be overwritten.
void plant_hostile_state() {
    card.unpack = {{k_unpack_alignment, 8},
                   {k_unpack_row_length, 3},
                   {k_unpack_skip_rows, 2},
                   {k_unpack_skip_pixels, 1}};
    card.unpack_buffer = 77;
    card.active_unit = k_texture0 + 3;
}

void reset() {
    card = fake_card{};
    context = fake_context{};
}

gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++context.current_calls;
    context.gl_calls_at_last_current = card.gl_calls;
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
    *data = pname == k_max_texture_size ? 4096 : 0;
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
    }
}
void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei, const GLuint *) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_bind_texture(GLenum target, GLuint name) {
    ++card.gl_calls;
    if (target == k_texture_2d) {
        card.bound_texture = name;
    }
}
void GLINTFX_GL_APIENTRY f_tex_parameter(GLenum, GLenum, GLint) { ++card.gl_calls; }
// The creation writes each level as tightly packed rows (alignment 1, row length 0, no skips): the
// bytes are width x height x 4, in a row.
void GLINTFX_GL_APIENTRY f_tex_image(GLenum, GLint, GLint, GLsizei width, GLsizei height, GLint,
                                     GLenum, GLenum, const void *pixels) {
    ++card.gl_calls;
    if (pixels != nullptr) {
        const auto *first = static_cast<const unsigned char *>(pixels);
        card.uploaded.insert(card.uploaded.end(), first,
                             first + static_cast<std::size_t>(width) * height * 4);
    }
}

// The rectangle as GL reads it from client memory: rows of GL_UNPACK_ROW_LENGTH texels (the width
// when it is 0), starting GL_UNPACK_SKIP_ROWS rows and GL_UNPACK_SKIP_PIXELS texels in (alignment
// 1, four bytes a texel; the code sets alignment 1, and the case checks it).
[[nodiscard]] std::vector<unsigned char> rectangle_read(const void *pixels, GLsizei width,
                                                        GLsizei height) {
    const auto row_length = static_cast<std::size_t>(
        card.unpack[k_unpack_row_length] == 0 ? width : card.unpack[k_unpack_row_length]);
    const auto skip_rows = static_cast<std::size_t>(card.unpack[k_unpack_skip_rows]);
    const auto skip_pixels = static_cast<std::size_t>(card.unpack[k_unpack_skip_pixels]);
    const auto *first = static_cast<const unsigned char *>(pixels);
    std::vector<unsigned char> bytes;
    for (std::size_t row = 0; row < static_cast<std::size_t>(height); ++row) {
        const unsigned char *start = first + ((skip_rows + row) * row_length + skip_pixels) * 4;
        bytes.insert(bytes.end(), start, start + static_cast<std::size_t>(width) * 4);
    }
    return bytes;
}

void GLINTFX_GL_APIENTRY f_tex_sub_image(GLenum target, GLint level, GLint x, GLint y,
                                         GLsizei width, GLsizei height, GLenum format, GLenum type,
                                         const void *pixels) {
    ++card.gl_calls;
    if (target != k_texture_2d) {
        return;
    }
    sub_upload sent{level,
                    x,
                    y,
                    width,
                    height,
                    format,
                    type,
                    {},
                    card.unpack,
                    card.unpack_buffer,
                    card.bound_texture,
                    card.active_unit};
    if (pixels != nullptr && width > 0 && height > 0) {
        sent.bytes = rectangle_read(pixels, width, height);
    }
    card.sub_uploads.push_back(sent);
    if (card.error_after_sub_upload != 0) {
        card.error_queue.push_back(card.error_after_sub_upload);
    }
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
    t.glTexSubImage2D = &f_tex_sub_image;
    return t;
}
const gl_function_table k_table = fake_table();

[[nodiscard]] texture_gl_access access() {
    return texture_gl_access{&context, &fake_make_current, &k_table, &owner_token};
}

// An image the case owns: `texels` row by row, straight sRGB.
struct owned_image {
    std::vector<std::byte> bytes;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

[[nodiscard]] owned_image image_of(const std::vector<gltfx_rgba8> &texels, std::uint32_t width,
                                   std::uint32_t height) {
    std::vector<std::byte> bytes;
    for (const gltfx_rgba8 texel : texels) {
        bytes.push_back(std::byte{texel.red});
        bytes.push_back(std::byte{texel.green});
        bytes.push_back(std::byte{texel.blue});
        bytes.push_back(std::byte{texel.alpha});
    }
    // A prvalue, never a named local (INBOX MSVC-ASAN-PADDING-RETURN, T2a-win).
    return owned_image{std::move(bytes), width, height};
}

[[nodiscard]] gltfx_image_view view_of(const owned_image &image) {
    return gltfx_image_view{.pixels = std::span<const std::byte>(image.bytes),
                            .width = image.width,
                            .height = image.height,
                            .bytes_per_row = static_cast<std::size_t>(image.width) * 4,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

// `width` x `height` texels, each a different colour (alpha too), so a texel from the wrong place
// shows.
[[nodiscard]] std::vector<gltfx_rgba8> varied(std::uint32_t width, std::uint32_t height,
                                              std::uint32_t seed) {
    std::vector<gltfx_rgba8> texels;
    for (std::uint32_t i = 0; i < width * height; ++i) {
        const std::uint32_t v = (i + seed) * 2654435761U;
        texels.push_back(gltfx_rgba8{
            static_cast<std::uint8_t>(v >> 24U), static_cast<std::uint8_t>(v >> 16U),
            static_cast<std::uint8_t>(v >> 8U), static_cast<std::uint8_t>(64U + (v % 192U))});
    }
    return texels;
}

// `base` with the `width` x `height` texels of `patch` pasted at (x, y).
[[nodiscard]] std::vector<gltfx_rgba8> pasted(std::vector<gltfx_rgba8> base,
                                              std::uint32_t base_width,
                                              const std::vector<gltfx_rgba8> &patch,
                                              const texel_rect &at) {
    const std::uint32_t width = at.columns.last - at.columns.first + 1U;
    for (std::uint32_t row = at.rows.first; row <= at.rows.last; ++row) {
        for (std::uint32_t column = at.columns.first; column <= at.columns.last; ++column) {
            base[row * base_width + column] =
                patch[(row - at.rows.first) * width + (column - at.columns.first)];
        }
    }
    return base;
}

// A prvalue, never a named local (INBOX MSVC-ASAN-PADDING-RETURN, T2a-win).
[[nodiscard]] gltfx_texture_2d_desc levels(std::uint32_t count, bool updatable) {
    return gltfx_texture_2d_desc{.mipmap_levels = count, .updatable = updatable};
}

// Made updatable unless the case says otherwise.
[[nodiscard]] texture_2d_impl *made(const owned_image &image, std::uint32_t reduced_levels,
                                    bool updatable = true) {
    const gltfx_rslt<texture_2d_impl *> result = create_texture_object(
        access(), view_of(image), levels(reduced_levels, updatable), default_batch_allocator());
    GLINTFX_CHECK(result.has_value());
    return result.has_value() ? result.value() : nullptr;
}

[[nodiscard]] bool same_texels(std::span<const gltfx_rgba8> a, std::span<const gltfx_rgba8> b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].red != b[i].red || a[i].green != b[i].green || a[i].blue != b[i].blue ||
            a[i].alpha != b[i].alpha) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool refused_as(const gltfx_rslt<void> &result, gltfx_err_code code,
                              std::string_view field, std::int64_t os_code) {
    return result.has_error() && result.err().code() == code &&
           result.err().rejected_value() == field && result.err().os_error_code() == os_code;
}

[[nodiscard]] bool same_rect(const sub_upload &sent, GLint level, GLint x, GLint y, GLsizei width,
                             GLsizei height) {
    return sent.level == level && sent.x == x && sent.y == y && sent.width == width &&
           sent.height == height;
}

void *refuse_everything(void *, std::size_t) noexcept { return nullptr; }

constexpr texel_rect k_two_by_one_at_1_2{{1U, 2U}, {2U, 2U}};

} // namespace

GLINTFX_TEST(an_update_makes_the_context_current_before_the_first_gl_call) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    const owned_image patch = image_of({{255, 0, 0, 128}, {0, 255, 0, 255}}, 2U, 1U);
    const int calls_before = card.gl_calls;
    GLINTFX_CHECK(update_texture_object(*texture, k_two_by_one_at_1_2, view_of(patch),
                                        default_batch_allocator())
                      .has_value());
    GLINTFX_CHECK_EQ(context.gl_calls_at_last_current, calls_before);
    delete texture;
}

GLINTFX_TEST(a_context_that_cannot_be_made_current_passes_its_error_on_untouched) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    const owned_image patch = image_of({{255, 0, 0, 128}, {0, 255, 0, 255}}, 2U, 1U);
    context.fail = true;
    const int calls_before = card.gl_calls;
    const gltfx_rslt<void> result = update_texture_object(
        *texture, k_two_by_one_at_1_2, view_of(patch), default_batch_allocator());
    GLINTFX_CHECK(refused_as(result, gltfx_err_code::platform_failure, "context", 0));
    GLINTFX_CHECK_EQ(card.gl_calls, calls_before);
    delete texture;
}

// Level 0 is written at the rectangle, from tightly packed rows of the stored texels, into the
// texture, on unit 0, from no buffer, over a hostile unpack state.
GLINTFX_TEST(level_0_is_written_at_the_rectangle_with_the_stored_texels) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    const owned_image patch = image_of({{255, 0, 0, 128}, {0, 255, 0, 255}}, 2U, 1U);
    plant_hostile_state();
    GLINTFX_CHECK(update_texture_object(*texture, k_two_by_one_at_1_2, view_of(patch),
                                        default_batch_allocator())
                      .has_value());
    GLINTFX_CHECK_EQ(card.sub_uploads.size(), std::size_t{1}); // no reduced level to write
    const sub_upload sent = card.sub_uploads.empty() ? sub_upload{} : card.sub_uploads.front();
    GLINTFX_CHECK(same_rect(sent, 0, 1, 2, 2, 1));
    GLINTFX_CHECK(sent.format == k_rgba && sent.type == k_unsigned_byte);
    const std::vector<unsigned char> stored = {188, 0, 0, 128, 0, 255, 0, 255};
    GLINTFX_CHECK(sent.bytes == stored);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_alignment), 1);
    GLINTFX_CHECK_EQ(sent.unpack.at(k_unpack_row_length), 0);
    GLINTFX_CHECK(sent.unpack_buffer == 0 && sent.bound_texture == texture->gl_name &&
                  sent.active_unit == k_texture0);
    delete texture;
}

GLINTFX_TEST(the_unpack_state_and_the_binding_are_left_as_create_leaves_them) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(8U, 8U, 1U), 8U, 8U), 3U);
    const owned_image patch = image_of(varied(3U, 2U, 9U), 3U, 2U);
    plant_hostile_state();
    GLINTFX_CHECK(update_texture_object(*texture, texel_rect{{2U, 4U}, {3U, 4U}}, view_of(patch),
                                        default_batch_allocator())
                      .has_value());
    GLINTFX_CHECK_EQ(card.unpack.at(k_unpack_alignment), 1);
    GLINTFX_CHECK_EQ(card.unpack.at(k_unpack_row_length), 0);
    GLINTFX_CHECK_EQ(card.unpack.at(k_unpack_skip_rows), 0);
    GLINTFX_CHECK_EQ(card.unpack.at(k_unpack_skip_pixels), 0);
    GLINTFX_CHECK(card.unpack_buffer == 0 && card.bound_texture == 0 &&
                  card.active_unit == k_texture0);
    delete texture;
}

// An 8 x 8 texture with 3 reduced levels, the 3 x 2 rectangle at (2, 3): level 1 touches columns
// and rows 1 to 2, level 2 columns and rows 0 to 1, level 3 its one texel (contas_t3b.py). Each is
// read from the kept texels with the row length of its level and the corner of its rectangle.
GLINTFX_TEST(each_reduced_level_is_written_at_the_texels_the_change_touches) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(8U, 8U, 1U), 8U, 8U), 3U);
    const owned_image patch = image_of(varied(3U, 2U, 9U), 3U, 2U);
    GLINTFX_CHECK(update_texture_object(*texture, texel_rect{{2U, 4U}, {3U, 4U}}, view_of(patch),
                                        default_batch_allocator())
                      .has_value());
    GLINTFX_CHECK_EQ(card.sub_uploads.size(), std::size_t{4});
    if (card.sub_uploads.size() == 4U) {
        GLINTFX_CHECK(same_rect(card.sub_uploads[0], 0, 2, 3, 3, 2));
        GLINTFX_CHECK(same_rect(card.sub_uploads[1], 1, 1, 1, 2, 2));
        GLINTFX_CHECK(same_rect(card.sub_uploads[2], 2, 0, 0, 2, 2));
        GLINTFX_CHECK(same_rect(card.sub_uploads[3], 3, 0, 0, 1, 1));
        GLINTFX_CHECK_EQ(card.sub_uploads[1].unpack.at(k_unpack_row_length), 4);
        GLINTFX_CHECK_EQ(card.sub_uploads[1].unpack.at(k_unpack_skip_pixels), 1);
        GLINTFX_CHECK_EQ(card.sub_uploads[1].unpack.at(k_unpack_skip_rows), 1);
        GLINTFX_CHECK_EQ(card.sub_uploads[2].unpack.at(k_unpack_row_length), 2);
        GLINTFX_CHECK_EQ(card.sub_uploads[3].unpack.at(k_unpack_row_length), 1);
    }
    delete texture;
}

namespace {

// The bytes of the texels of `rect` in one level of kept texels: the level starts at `offset` and
// has rows of `level_width` texels.
[[nodiscard]] std::vector<unsigned char> rect_bytes(std::span<const gltfx_rgba8> kept,
                                                    std::size_t offset, std::uint32_t level_width,
                                                    const texel_rect &rect) {
    std::vector<unsigned char> bytes;
    for (std::uint32_t row = rect.rows.first; row <= rect.rows.last; ++row) {
        for (std::uint32_t column = rect.columns.first; column <= rect.columns.last; ++column) {
            const gltfx_rgba8 texel =
                kept[offset + static_cast<std::size_t>(row) * level_width + column];
            bytes.insert(bytes.end(), {texel.red, texel.green, texel.blue, texel.alpha});
        }
    }
    return bytes;
}

} // namespace

// The oracle: updated, the texture keeps exactly the texels of a texture MADE from the whole new
// image, on every level; and the bytes written to each level are those texels, at the rectangle
// that level touches (levels at offsets 0, 64, 80 and 84, of widths 8, 4, 2 and 1).
GLINTFX_TEST(the_kept_texels_equal_those_of_a_texture_made_from_the_whole_new_image) {
    reset();
    const std::vector<gltfx_rgba8> before = varied(8U, 8U, 1U);
    const std::vector<gltfx_rgba8> patch_texels = varied(3U, 2U, 9U);
    const texel_rect at{{2U, 4U}, {3U, 4U}};
    texture_2d_impl *updated = made(image_of(before, 8U, 8U), 3U);
    texture_2d_impl *whole = made(image_of(pasted(before, 8U, patch_texels, at), 8U, 8U), 3U);
    card.sub_uploads.clear();
    GLINTFX_CHECK(update_texture_object(*updated, at, view_of(image_of(patch_texels, 3U, 2U)),
                                        default_batch_allocator())
                      .has_value());
    GLINTFX_CHECK_EQ(updated->kept.texels().size(), std::size_t{64 + 16 + 4 + 1});
    GLINTFX_CHECK(same_texels(updated->kept.texels(), whole->kept.texels()));
    const std::span<const gltfx_rgba8> expected = whole->kept.texels();
    GLINTFX_CHECK_EQ(card.sub_uploads.size(), std::size_t{4});
    if (card.sub_uploads.size() == 4U) {
        GLINTFX_CHECK(card.sub_uploads[0].bytes == rect_bytes(expected, 0U, 8U, at));
        GLINTFX_CHECK(card.sub_uploads[1].bytes ==
                      rect_bytes(expected, 64U, 4U, texel_rect{{1U, 2U}, {1U, 2U}}));
        GLINTFX_CHECK(card.sub_uploads[2].bytes ==
                      rect_bytes(expected, 80U, 2U, texel_rect{{0U, 1U}, {0U, 1U}}));
        GLINTFX_CHECK(card.sub_uploads[3].bytes ==
                      rect_bytes(expected, 84U, 1U, texel_rect{{0U, 0U}, {0U, 0U}}));
    }
    delete updated;
    delete whole;
}

GLINTFX_TEST(a_texture_without_reduced_levels_keeps_no_texels) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    GLINTFX_CHECK(texture->kept.texels().empty());
    delete texture;
}

// T3 (kept_texels.hpp, gltfx_texture_2d_desc::updatable): an updatable texture with reduced levels
// keeps the texels of every level, the very bytes the creation wrote, level after level (4 x 2,
// then 2 x 1, then 1 x 1: 11 texels), and its record says it is updatable.
GLINTFX_TEST(an_updatable_texture_with_reduced_levels_keeps_the_texels_it_uploaded) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 2U, 3U), 4U, 2U), 2U);
    GLINTFX_CHECK(texture->updatable);
    const std::span<const gltfx_rgba8> kept = texture->kept.texels();
    GLINTFX_CHECK_EQ(kept.size(), std::size_t{8 + 2 + 1});
    GLINTFX_CHECK(rect_bytes(kept, 0U, 11U, texel_rect{{0U, 10U}, {0U, 0U}}) == card.uploaded);
    delete texture;
}

// The leader's decision (T3-1, "Só quem declarar"): a texture made without updatable keeps NO
// texels, even with reduced levels, and its record says it is not updatable.
GLINTFX_TEST(a_texture_made_without_updatable_keeps_no_texels_even_with_reduced_levels) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 2U, 3U), 4U, 2U), 2U, false);
    GLINTFX_CHECK(!texture->updatable);
    GLINTFX_CHECK(texture->kept.texels().empty());
    GLINTFX_CHECK_EQ(card.uploaded.size(), std::size_t{44}); // (8 + 2 + 1) texels x 4 bytes
    delete texture;
}

// The first GL error after the writes names the update, with its GL code; the binding is left at 0.
GLINTFX_TEST(a_gl_error_after_the_writes_is_named_with_its_gl_code) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    const owned_image patch = image_of({{255, 0, 0, 128}, {0, 255, 0, 255}}, 2U, 1U);
    card.error_after_sub_upload = k_out_of_memory;
    GLINTFX_CHECK(refused_as(update_texture_object(*texture, k_two_by_one_at_1_2, view_of(patch),
                                                   default_batch_allocator()),
                             gltfx_err_code::out_of_memory, "texture_update", k_out_of_memory));
    GLINTFX_CHECK_EQ(card.bound_texture, 0U);
    card.error_queue.clear();
    card.error_after_sub_upload = k_invalid_operation;
    GLINTFX_CHECK(refused_as(update_texture_object(*texture, k_two_by_one_at_1_2, view_of(patch),
                                                   default_batch_allocator()),
                             gltfx_err_code::platform_failure, "texture_update",
                             k_invalid_operation));
    GLINTFX_CHECK_EQ(card.bound_texture, 0U);
    delete texture;
}

GLINTFX_TEST(a_stale_gl_error_is_not_blamed_on_the_update) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(4U, 4U, 1U), 4U, 4U), 0U);
    const owned_image patch = image_of({{255, 0, 0, 128}, {0, 255, 0, 255}}, 2U, 1U);
    card.error_queue = {k_invalid_enum, k_invalid_operation};
    GLINTFX_CHECK(update_texture_object(*texture, k_two_by_one_at_1_2, view_of(patch),
                                        default_batch_allocator())
                      .has_value());
    delete texture;
}

// The library's own memory: refused before any write, and the kept texels unchanged.
GLINTFX_TEST(the_library_running_out_of_memory_is_out_of_memory_with_no_write) {
    reset();
    texture_2d_impl *texture = made(image_of(varied(8U, 8U, 1U), 8U, 8U), 3U);
    const std::vector<gltfx_rgba8> kept_before(texture->kept.texels().begin(),
                                               texture->kept.texels().end());
    const owned_image patch = image_of(varied(3U, 2U, 9U), 3U, 2U);
    const batch_allocator refusing{&refuse_everything, default_batch_allocator().release};
    GLINTFX_CHECK(refused_as(
        update_texture_object(*texture, texel_rect{{2U, 4U}, {3U, 4U}}, view_of(patch), refusing),
        gltfx_err_code::out_of_memory, "texture_update", 0));
    GLINTFX_CHECK(card.sub_uploads.empty());
    GLINTFX_CHECK(same_texels(texture->kept.texels(), kept_before));
    delete texture;
}
