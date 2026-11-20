// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_atlas_impl.hpp"

#include "harness/check.hpp"
#include "harness/compiler_noinline.hpp"
#include "harness/test_registry.hpp"

// texture_atlas_test.cpp - R2D-TEXTURE, fatia T6-B1 (draw2d/texture_atlas_impl.hpp; GODS_LAWS.md
// L-17, L-20, L-22). The texture atlas against a FAKE context and a table of FAKE GL functions that
// keep what the code asked of the card (the shape of texture_update_test): the empty image the
// texture is made from, and every glTexSubImage2D with its rectangle and its bytes. The interior,
// the allocator's interior and the making and writing of a texture are compiled a second time into
// this test (tests/CMakeLists.txt). Every place and every texel expected is a literal worked out by
// hand from texture_atlas.hpp and core/atlas_allocator.hpp (HOW A RECTANGLE IS PLACED), never read
// back from the program. Opaque straight-sRGB texels are stored as they come (alpha 1 multiplies
// nothing), so the bytes the card receives are the image's bytes, extruded.
//
// A DECLARED DOWNGRADE (GODS_LAWS.md L-09): no window, no driver; the atlas on a real context is
// the cells of draw2d_texture_parity_test.

using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_draw_2d_refusal;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_atlas_desc;
using glintfx::gltfx_texture_filter;
using glintfx::texture_2d_impl;
using glintfx::texture_atlas_impl;
using glintfx::draw2d::add_image_to_atlas;
using glintfx::draw2d::atlas_receipt_read;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::destroy_texture_atlas;
using glintfx::draw2d::make_texture_atlas;
using glintfx::draw2d::read_atlas_receipt;
using glintfx::draw2d::region_in_atlas;
using glintfx::draw2d::release_from_texture_atlas;
using glintfx::draw2d::texture_atlas_made;
using glintfx::draw2d::texture_gl_access;
using glintfx::render::gl_function_table;
using glintfx::render::GLenum;
using glintfx::render::GLint;
using glintfx::render::GLsizei;
using glintfx::render::GLuint;

namespace {

// gl.xml
constexpr GLenum k_texture_2d = 0x0DE1;
constexpr GLenum k_max_texture_size = 0x0D33;
constexpr GLenum k_out_of_memory = 0x0505;

// One glTexSubImage2D of level 0 as the card received it (the atlas below has no reduced level
// where the bytes are read).
struct sub_upload {
    GLint level = -1;
    GLint x = -1;
    GLint y = -1;
    GLsizei width = 0;
    GLsizei height = 0;
    std::vector<unsigned char> bytes;
};

struct fake_card {
    int gl_calls = 0;
    std::vector<GLenum> error_queue;
    GLenum error_after_sub_upload = 0;
    GLuint next_name = 40;
    std::vector<sub_upload> sub_uploads;
    std::vector<unsigned char> uploaded; // the bytes of every glTexImage2D, level after level
};
fake_card card;

struct fake_context {
    bool fail = false;
    int current_calls = 0;
};
fake_context context;

int owner_token = 0;

void reset() {
    card = fake_card{};
    context = fake_context{};
}

glintfx::gltfx_rslt<void> fake_make_current(void *) noexcept {
    ++context.current_calls;
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
void GLINTFX_GL_APIENTRY f_any_two(GLenum, GLuint) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_pixel_store(GLenum, GLint) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_active_texture(GLenum) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_gen_textures(GLsizei n, GLuint *names) {
    ++card.gl_calls;
    for (GLsizei i = 0; i < n; ++i) {
        names[i] = card.next_name++;
    }
}
void GLINTFX_GL_APIENTRY f_delete_textures(GLsizei, const GLuint *) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_tex_parameter(GLenum, GLenum, GLint) { ++card.gl_calls; }
void GLINTFX_GL_APIENTRY f_tex_image(GLenum, GLint, GLint, GLsizei width, GLsizei height, GLint,
                                     GLenum, GLenum, const void *pixels) {
    ++card.gl_calls;
    if (pixels != nullptr) {
        const auto *first = static_cast<const unsigned char *>(pixels);
        card.uploaded.insert(card.uploaded.end(), first,
                             first + static_cast<std::size_t>(width) * height * 4);
    }
}
// Level 0 of an atlas without reduced levels is written tightly packed (texture_update.hpp, step
// 5).
void GLINTFX_GL_APIENTRY f_tex_sub_image(GLenum target, GLint level, GLint x, GLint y,
                                         GLsizei width, GLsizei height, GLenum, GLenum,
                                         const void *pixels) {
    ++card.gl_calls;
    if (target != k_texture_2d) {
        return;
    }
    sub_upload sent{level, x, y, width, height, {}};
    if (pixels != nullptr) {
        const auto *first = static_cast<const unsigned char *>(pixels);
        sent.bytes.assign(first, first + static_cast<std::size_t>(width) * height * 4);
    }
    card.sub_uploads.push_back(std::move(sent));
    if (card.error_after_sub_upload != 0) {
        card.error_queue.push_back(card.error_after_sub_upload);
    }
}

[[nodiscard]] gl_function_table fake_table() {
    gl_function_table t;
    t.glGetError = &f_get_error;
    t.glGetIntegerv = &f_get_integer;
    t.glBindBuffer = &f_any_two;
    t.glPixelStorei = &f_pixel_store;
    t.glActiveTexture = &f_active_texture;
    t.glGenTextures = &f_gen_textures;
    t.glDeleteTextures = &f_delete_textures;
    t.glBindTexture = &f_any_two;
    t.glTexParameteri = &f_tex_parameter;
    t.glTexImage2D = &f_tex_image;
    t.glTexSubImage2D = &f_tex_sub_image;
    return t;
}
const gl_function_table k_table = fake_table();

[[nodiscard]] texture_gl_access access() {
    return texture_gl_access{&context, &fake_make_current, &k_table, &owner_token};
}

// A memory that answers like realloc until it is told to refuse its n-th request from now, and
// counts what it hands out and takes back.
struct armed_state {
    int requests = 0;
    int refuse_at = 0; // 0: never
    int blocks = 0;    // handed out and not taken back
    // The bytes of each request since arm(), which is how THE COST of texture_atlas.hpp is read.
    std::array<std::size_t, 64> asked{};
};
armed_state armed;

void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    if (static_cast<std::size_t>(armed.requests) < armed.asked.size()) {
        armed.asked[static_cast<std::size_t>(armed.requests)] = bytes;
    }
    ++armed.requests;
    if (armed.refuse_at != 0 && armed.requests == armed.refuse_at) {
        return nullptr;
    }
    void *grown = std::realloc(block, bytes);
    armed.blocks += (block == nullptr && grown != nullptr) ? 1 : 0;
    return grown;
}
void armed_release(void *block) noexcept {
    armed.blocks -= block != nullptr ? 1 : 0;
    std::free(block);
}
constexpr batch_allocator k_armed{&armed_reallocate, &armed_release};

void arm(int refuse_at) {
    armed.requests = 0;
    armed.refuse_at = refuse_at;
    armed.asked.fill(0U);
}

[[nodiscard]] gltfx_texture_atlas_desc desc_of(std::uint32_t side, std::uint32_t margin,
                                               std::uint32_t levels) {
    return gltfx_texture_atlas_desc{.width = side,
                                    .height = side,
                                    .margin_pixels = margin,
                                    .filter = gltfx_texture_filter::nearest,
                                    .mipmap_levels = levels};
}

// An atlas made on the fake card, and given back with its texture at the end of the case.
struct open_atlas {
    texture_atlas_impl *atlas = nullptr;
    texture_2d_impl *texture = nullptr;
    open_atlas(const open_atlas &) = delete;
    open_atlas &operator=(const open_atlas &) = delete;
    explicit open_atlas(const gltfx_texture_atlas_desc &desc) {
        gltfx_rslt<texture_atlas_made> made = make_texture_atlas(access(), desc, k_armed);
        if (made.has_value()) {
            atlas = made.value().atlas;
            texture = made.value().texture;
        }
    }
    ~open_atlas() {
        destroy_texture_atlas(atlas);
        delete texture;
    }
};

// An image of opaque texels, texel (x, y) the bytes 10 * x + 1, 10 * y + 2, seed, 255.
struct owned_image {
    std::vector<std::byte> bytes;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

[[nodiscard]] owned_image image_of(std::uint32_t width, std::uint32_t height, std::uint8_t seed) {
    std::vector<std::byte> bytes;
    for (std::uint32_t i = 0; i < width * height; ++i) {
        bytes.push_back(static_cast<std::byte>(10U * (i % width) + 1U));
        bytes.push_back(static_cast<std::byte>(10U * (i / width) + 2U));
        bytes.push_back(static_cast<std::byte>(seed));
        bytes.push_back(std::byte{255});
    }
    return owned_image{std::move(bytes), width, height};
}

[[nodiscard]] gltfx_image_view view_of(const owned_image &image) {
    return gltfx_image_view{.pixels = std::span<const std::byte>(image.bytes),
                            .width = image.width,
                            .height = image.height,
                            .bytes_per_row = static_cast<std::size_t>(image.width) * 4,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

// NOLINTNEXTLINE(misc-const-correctness) reason: it changes the atlas that a owns
[[nodiscard]] gltfx_rslt<gltfx_atlas_slot> add(open_atlas &a, const owned_image &image) {
    return add_image_to_atlas(a.atlas, a.texture, view_of(image));
}

template <typename T>
[[nodiscard]] bool refused_as(const gltfx_rslt<T> &got, gltfx_err_code code,
                              std::string_view name) {
    return got.has_error() && got.err().code() == code && got.err().rejected_value() == name;
}

[[nodiscard]] bool rect_is(const gltfx_atlas_slot &slot, const glintfx::gltfx_atlas_rect &want) {
    return slot.rect.x == want.x && slot.rect.y == want.y && slot.rect.width == want.width &&
           slot.rect.height == want.height;
}

[[nodiscard]] bool region_is(const gltfx_rslt<gltfx_rect_texel> &got,
                             const gltfx_rect_texel &want) {
    return got.has_value() && got.value().corner.x == want.corner.x &&
           got.value().corner.y == want.corner.y && got.value().size.x == want.size.x &&
           got.value().size.y == want.size.y;
}

// How many requests for memory one add makes on a fresh atlas; 0 when the add fails.
[[nodiscard]] int requests_of_an_add() {
    open_atlas a(desc_of(32, 1, 0));
    arm(0);
    return add(a, image_of(2, 2, 1)).has_value() ? armed.requests : 0;
}

// One add on a fresh atlas with the memory refusing its n-th request: true when the add is refused
// as out_of_memory and the next add, the memory answering again, takes the place at (0, 0).
[[nodiscard]] bool refused_with_no_cell_taken(int n) {
    open_atlas a(desc_of(32, 1, 0));
    arm(n);
    const gltfx_rslt<gltfx_atlas_slot> failed = add(a, image_of(2, 2, 1));
    arm(0);
    const gltfx_rslt<gltfx_atlas_slot> next = add(a, image_of(2, 2, 1));
    return failed.has_error() && failed.err().code() == gltfx_err_code::out_of_memory &&
           next.has_value() && rect_is(next.value(), {0, 0, 4, 4});
}

// The four bytes of image texel (x, y), as image_of() wrote them.
[[nodiscard]] std::vector<unsigned char> texel(std::uint32_t x, std::uint32_t y,
                                               std::uint8_t seed) {
    return {static_cast<unsigned char>(10U * x + 1U), static_cast<unsigned char>(10U * y + 2U),
            seed, 255};
}

} // namespace

// R2D-TEXTURE T6-B2: this translation unit replaces the global operator new, every form, only to
// COUNT, because "reading a receipt asks for no memory" (renderer_2d.hpp) is a promise about EVERY
// memory, not only the atlas's armed one: a refusal built as a gltfx_err would ask for its context
// here. The same technique, the same forms and the same reason for an atomic counter as
// err_no_alloc_test. TWO DECLARED LIMITS: in a Windows shared build a gltfx_err inside the DLL asks
// the DLL's own operator new, which this count does not see; and under AddressSanitizer the
// override may lose to the sanitizer's own, so the control below is not asserted there (the armed
// allocator and the plain legs still are).
namespace {
std::atomic<std::size_t> g_global_news{0};
#if defined(__SANITIZE_ADDRESS__)
constexpr bool k_global_news_are_counted = false;
#else
constexpr bool k_global_news_are_counted = true;
#endif
} // namespace

// NOLINTBEGIN(misc-new-delete-overloads,cppcoreguidelines-no-malloc) reason: see above
// None of these is inlined: seen through, an operator new is a malloc() next to an operator delete,
// or an operator delete is a free() next to an operator new, and GCC's -Wmismatched-new-delete
// reproves that pair, measured in the Release and in the sanitizer builds.
GLINTFX_TEST_NOINLINE void *operator new(std::size_t size) {
    ++g_global_news;
    void *block = std::malloc(size == 0 ? 1 : size);
    if (block == nullptr) {
        throw std::bad_alloc{};
    }
    return block;
}
GLINTFX_TEST_NOINLINE void *operator new[](std::size_t size) { return ::operator new(size); }
GLINTFX_TEST_NOINLINE void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    ++g_global_news;
    return std::malloc(size == 0 ? 1 : size);
}
GLINTFX_TEST_NOINLINE void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept {
    return ::operator new(size, tag);
}
GLINTFX_TEST_NOINLINE void operator delete(void *block) noexcept { std::free(block); }
GLINTFX_TEST_NOINLINE void operator delete(void *block, std::size_t) noexcept { std::free(block); }
GLINTFX_TEST_NOINLINE void operator delete(void *block, const std::nothrow_t &) noexcept {
    std::free(block);
}
GLINTFX_TEST_NOINLINE void operator delete[](void *block) noexcept { std::free(block); }
GLINTFX_TEST_NOINLINE void operator delete[](void *block, std::size_t) noexcept {
    std::free(block);
}
GLINTFX_TEST_NOINLINE void operator delete[](void *block, const std::nothrow_t &) noexcept {
    std::free(block);
}
// NOLINTEND(misc-new-delete-overloads,cppcoreguidelines-no-malloc) reason: see above

namespace {

[[nodiscard]] bool read_is(const atlas_receipt_read &got, gltfx_draw_2d_refusal refusal,
                           const gltfx_rect_texel &region) {
    return got.refusal == refusal && got.region.corner.x == region.corner.x &&
           got.region.corner.y == region.corner.y && got.region.size.x == region.size.x &&
           got.region.size.y == region.size.y;
}

constexpr gltfx_rect_texel k_no_region{};

// An owner number this test never sees handed out: owners are handed out from 1 up, one per
// allocator opened. 4294967295 is handed out only to the 4294967295th allocator opened in the
// process (core/atlas_allocator.hpp, A SLOT IS A RECEIPT); this test opens a few, so it never
// reaches it.
constexpr std::uint32_t k_owner_never_handed_out = 0xFFFFFFFFU;

} // namespace

// T6-B2: a current receipt reads the region of its image, the place worked out by hand: margin 2,
// so the second image of a row of cells of 7 x 7 starts 2 texels into a cell at x = 7, and the same
// value region_of() gives.
GLINTFX_TEST(texture_atlas_a_current_receipt_reads_the_region_of_its_image) {
    reset();
    arm(0);
    open_atlas a(desc_of(64, 2, 0));
    const gltfx_rslt<gltfx_atlas_slot> first = add(a, image_of(3, 3, 1));
    const gltfx_rslt<gltfx_atlas_slot> second = add(a, image_of(3, 2, 2));
    GLINTFX_CHECK(first.has_value() && second.has_value());
    if (!first.has_value() || !second.has_value()) {
        return;
    }
    const gltfx_rect_texel want{{9.0, 2.0}, {3.0, 2.0}};
    GLINTFX_CHECK(
        read_is(read_atlas_receipt(a.atlas, second.value()), gltfx_draw_2d_refusal::none, want));
    GLINTFX_CHECK(region_is(glintfx::draw2d::region_in_atlas(a.atlas, second.value()), want));
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, first.value()), gltfx_draw_2d_refusal::none,
                          gltfx_rect_texel{{2.0, 2.0}, {3.0, 3.0}}));
}

// T6-B2: each receipt that is not current reads its reason and an empty region, in the order of
// region_of(): an owner of 0 is stale (never handed out); another atlas's owner is another
// atlas's; with this atlas's owner, the slot table says, field by field.
GLINTFX_TEST(texture_atlas_a_receipt_not_current_reads_its_reason_and_no_region) {
    reset();
    arm(0);
    open_atlas a(desc_of(64, 1, 0));
    open_atlas other(desc_of(64, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> taken = add(a, image_of(4, 4, 1));
    const gltfx_rslt<gltfx_atlas_slot> foreign = add(other, image_of(4, 4, 2));
    GLINTFX_CHECK(taken.has_value() && foreign.has_value());
    if (!taken.has_value() || !foreign.has_value()) {
        return;
    }
    using enum gltfx_draw_2d_refusal;
    const auto reads = [&](const gltfx_atlas_slot &slot, gltfx_draw_2d_refusal why) {
        return read_is(read_atlas_receipt(a.atlas, slot), why, k_no_region);
    };
    GLINTFX_CHECK(reads(foreign.value(), region_other_atlas));
    GLINTFX_CHECK(reads(gltfx_atlas_slot{}, stale_region));
    gltfx_atlas_slot changed = taken.value();
    changed.rect.width += 1U;
    GLINTFX_CHECK(reads(changed, stale_region));
    changed = taken.value();
    changed.generation += 1U;
    GLINTFX_CHECK(reads(changed, stale_region));
    changed = taken.value();
    changed.id += 1U;
    GLINTFX_CHECK(reads(changed, stale_region));
    changed = taken.value();
    changed.owner = 0U;
    GLINTFX_CHECK(reads(changed, stale_region));
    GLINTFX_CHECK(
        read_is(read_atlas_receipt(nullptr, taken.value()), texture_not_open, k_no_region));
    GLINTFX_CHECK(glintfx::draw2d::release_from_texture_atlas(a.atlas, taken.value()).has_value());
    GLINTFX_CHECK(reads(taken.value(), stale_region));
}

// T6-B2: reading a receipt, the owner is read first (texture_atlas.hpp, THE RECEIPT). An owner of
// 0 is stale. An owner other than 0 that is not this atlas's reads as another atlas's, whatever the
// other fields say; with this atlas's owner, the slot table decides. The receipt of `other` is the
// one of `a` with only its owner changed (the same cell, number and generation): only the owner
// tells the two apart.
GLINTFX_TEST(texture_atlas_reading_a_receipt_reads_its_owner_first) {
    reset();
    arm(0);
    open_atlas a(desc_of(64, 1, 0));
    open_atlas other(desc_of(64, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> taken = add(a, image_of(4, 4, 1));
    const gltfx_rslt<gltfx_atlas_slot> foreign = add(other, image_of(4, 4, 2));
    GLINTFX_CHECK(taken.has_value() && foreign.has_value());
    if (!taken.has_value() || !foreign.has_value()) {
        return;
    }
    using enum gltfx_draw_2d_refusal;
    GLINTFX_CHECK(rect_is(foreign.value(), taken.value().rect) &&
                  foreign.value().id == taken.value().id &&
                  foreign.value().generation == taken.value().generation);
    gltfx_atlas_slot changed = taken.value();
    changed.owner = foreign.value().owner;
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, changed), region_other_atlas, k_no_region));
    changed.owner = k_owner_never_handed_out;
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, changed), region_other_atlas, k_no_region));
    // A receipt of `other` with its rect changed is still `other`'s: the owner comes first.
    changed = foreign.value();
    changed.rect.width += 1U;
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, changed), region_other_atlas, k_no_region));
    GLINTFX_CHECK(refused_as(glintfx::draw2d::region_in_atlas(a.atlas, changed),
                             gltfx_err_code::invalid_argument, "slot_other_allocator"));
    // With this atlas's owner, the four fields of a current receipt are that receipt: margin 1, so
    // the image of 4 x 4 starts 1 texel into the cell at the origin.
    changed = foreign.value();
    changed.owner = taken.value().owner;
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, changed), none,
                          gltfx_rect_texel{{1.0, 1.0}, {4.0, 4.0}}));
}

// T6-B2: when the number of a released receipt is handed out again, the new receipt reads the new
// image, and the old one stays stale.
GLINTFX_TEST(texture_atlas_a_number_handed_out_again_leaves_the_old_receipt_stale) {
    reset();
    arm(0);
    open_atlas a(desc_of(64, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> taken = add(a, image_of(4, 4, 1));
    GLINTFX_CHECK(taken.has_value());
    if (!taken.has_value()) {
        return;
    }
    GLINTFX_CHECK(glintfx::draw2d::release_from_texture_atlas(a.atlas, taken.value()).has_value());
    const gltfx_rslt<gltfx_atlas_slot> again = add(a, image_of(2, 2, 3));
    GLINTFX_CHECK(again.has_value());
    if (!again.has_value()) {
        return;
    }
    GLINTFX_CHECK_EQ(again.value().id, taken.value().id); // the number handed out again
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, taken.value()),
                          gltfx_draw_2d_refusal::stale_region, k_no_region));
    GLINTFX_CHECK(read_is(read_atlas_receipt(a.atlas, again.value()), gltfx_draw_2d_refusal::none,
                          gltfx_rect_texel{{1.0, 1.0}, {2.0, 2.0}}));
}

// T6-B2: reading a receipt asks for no memory, of any kind, whatever it answers: neither the
// atlas's own (armed) nor the global operator new (see above).
GLINTFX_TEST(texture_atlas_reading_a_receipt_asks_for_no_memory) {
    reset();
    arm(0);
    open_atlas a(desc_of(64, 1, 0));
    open_atlas other(desc_of(64, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> taken = add(a, image_of(4, 4, 1));
    const gltfx_rslt<gltfx_atlas_slot> foreign = add(other, image_of(4, 4, 2));
    GLINTFX_CHECK(taken.has_value() && foreign.has_value());
    if (!taken.has_value() || !foreign.has_value()) {
        return;
    }
    gltfx_atlas_slot changed = taken.value();
    changed.generation += 1U;
    arm(0);
    const std::size_t before = g_global_news.load();
    const std::array<atlas_receipt_read, 5> reads{
        read_atlas_receipt(a.atlas, taken.value()),
        read_atlas_receipt(a.atlas, foreign.value()),
        read_atlas_receipt(a.atlas, gltfx_atlas_slot{}),
        read_atlas_receipt(a.atlas, changed),
        read_atlas_receipt(nullptr, taken.value()),
    };
    const std::size_t after = g_global_news.load();
    GLINTFX_CHECK_EQ(after - before, std::size_t{0});
    GLINTFX_CHECK_EQ(armed.requests, 0);
    GLINTFX_CHECK(reads[0].refusal == gltfx_draw_2d_refusal::none);
    GLINTFX_CHECK(reads[1].refusal == gltfx_draw_2d_refusal::region_other_atlas);
    // The counter does count (the control of this case): a block asked here shows in it. A direct
    // call kept in a volatile, because a compiler may drop a new and a delete it can pair.
    void *volatile asked = ::operator new(16);
    ::operator delete(asked);
    GLINTFX_CHECK(!k_global_news_are_counted || g_global_news.load() > after);
}

GLINTFX_TEST(texture_atlas_open_makes_one_updatable_texture_with_its_levels_from_an_empty_image) {
    reset();
    const open_atlas a(desc_of(16, 4, 2));
    GLINTFX_CHECK(a.atlas != nullptr && a.texture != nullptr);
    GLINTFX_CHECK(a.texture->width == 16U && a.texture->height == 16U);
    GLINTFX_CHECK(a.texture->reduced_levels == 2U && a.texture->updatable);
    GLINTFX_CHECK(a.texture->filter == gltfx_texture_filter::nearest);
    // Levels 0, 1 and 2: 16 x 16, 8 x 8 and 4 x 4 texels of four zero bytes.
    GLINTFX_CHECK(card.uploaded.size() == std::size_t{256 + 64 + 16} * 4U);
    bool all_zero = true;
    for (const unsigned char byte : card.uploaded) {
        all_zero = all_zero && byte == 0;
    }
    GLINTFX_CHECK(all_zero);
}

GLINTFX_TEST(texture_atlas_open_refused_leaves_nothing_behind) {
    reset();
    arm(0);
    const int blocks_before = armed.blocks;
    const gltfx_rslt<texture_atlas_made> bad_margin =
        make_texture_atlas(access(), desc_of(16, 3, 2), k_armed);
    GLINTFX_CHECK(refused_as(bad_margin, gltfx_err_code::invalid_argument, "margin_pixels"));
    GLINTFX_CHECK(card.gl_calls == 0 && context.current_calls == 0 && armed.requests == 0);
    context.fail = true;
    const gltfx_rslt<texture_atlas_made> no_context =
        make_texture_atlas(access(), desc_of(16, 4, 2), k_armed);
    GLINTFX_CHECK(refused_as(no_context, gltfx_err_code::platform_failure, "context"));
    GLINTFX_CHECK(armed.blocks == blocks_before);
    context.fail = false;
    arm(1); // the interior's own block
    const gltfx_rslt<texture_atlas_made> no_memory =
        make_texture_atlas(access(), desc_of(16, 4, 2), k_armed);
    GLINTFX_CHECK(refused_as(no_memory, gltfx_err_code::out_of_memory, "atlas_open"));
    GLINTFX_CHECK(armed.blocks == blocks_before && card.gl_calls == 0);
}

GLINTFX_TEST(texture_atlas_add_image_writes_the_extruded_cell_at_the_receipts_place) {
    reset();
    open_atlas a(desc_of(32, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> first = add(a, image_of(2, 2, 7));
    GLINTFX_CHECK(first.has_value() && rect_is(first.value(), {0, 0, 4, 4}));
    GLINTFX_CHECK(card.sub_uploads.size() == 1U);
    const sub_upload &sent = card.sub_uploads[0];
    GLINTFX_CHECK(sent.level == 0 && sent.x == 0 && sent.y == 0 && sent.width == 4 &&
                  sent.height == 4);
    // Row 0 of the cell is the image's row 0, extruded: (0,0) (0,0) (1,0) (1,0); row 3 is row 1.
    std::vector<unsigned char> row0;
    for (const std::uint32_t x : {0U, 0U, 1U, 1U}) {
        const std::vector<unsigned char> t = texel(x, 0, 7);
        row0.insert(row0.end(), t.begin(), t.end());
    }
    GLINTFX_CHECK(std::vector<unsigned char>(sent.bytes.begin(), sent.bytes.begin() + 16) == row0);
    const std::vector<unsigned char> corner = texel(1, 1, 7);
    GLINTFX_CHECK(std::vector<unsigned char>(sent.bytes.end() - 4, sent.bytes.end()) == corner);
    // A 3 x 1 image: cell 5 x 3, on the shelf of height 4 (at least 3, at most 1.5 x 3), at x 4.
    const gltfx_rslt<gltfx_atlas_slot> second = add(a, image_of(3, 1, 8));
    GLINTFX_CHECK(second.has_value() && rect_is(second.value(), {4, 0, 5, 3}));
    GLINTFX_CHECK(region_is(region_in_atlas(a.atlas, first.value()), {{1.0, 1.0}, {2.0, 2.0}}));
    GLINTFX_CHECK(region_is(region_in_atlas(a.atlas, second.value()), {{5.0, 1.0}, {3.0, 1.0}}));
}

GLINTFX_TEST(texture_atlas_add_image_refusals_by_name_in_order_and_nothing_taken) {
    reset();
    open_atlas a(desc_of(32, 1, 0));
    const owned_image short_rows = image_of(2, 2, 1);
    gltfx_image_view bad_rows = view_of(short_rows);
    bad_rows.bytes_per_row = 4;
    GLINTFX_CHECK(refused_as(add_image_to_atlas(nullptr, a.texture, view_of(short_rows)),
                             gltfx_err_code::invalid_argument, "atlas"));
    GLINTFX_CHECK(refused_as(add(a, image_of(0, 2, 1)), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(
        refused_as(add(a, image_of(2, 0, 1)), gltfx_err_code::invalid_argument, "height"));
    GLINTFX_CHECK(refused_as(add_image_to_atlas(a.atlas, a.texture, bad_rows),
                             gltfx_err_code::invalid_argument, "bytes_per_row"));
    // 31 + 2 margins = 33, wider than 32; then 1 x 31, taller.
    GLINTFX_CHECK(
        refused_as(add(a, image_of(31, 1, 1)), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(
        refused_as(add(a, image_of(1, 31, 1)), gltfx_err_code::invalid_argument, "height"));
    GLINTFX_CHECK(card.sub_uploads.empty());
    const gltfx_rslt<gltfx_atlas_slot> after = add(a, image_of(2, 2, 1));
    GLINTFX_CHECK(after.has_value() && rect_is(after.value(), {0, 0, 4, 4}));
    GLINTFX_CHECK(after.value().id == 0U && after.value().generation == 1U);
}

GLINTFX_TEST(texture_atlas_full_now_is_the_allocators_no_free_space) {
    reset();
    open_atlas a(desc_of(8, 1, 0));
    GLINTFX_CHECK(add(a, image_of(6, 6, 1)).has_value()); // the whole 8 x 8
    GLINTFX_CHECK(
        refused_as(add(a, image_of(1, 1, 1)), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(card.sub_uploads.size() == 1U);
}

GLINTFX_TEST(texture_atlas_the_librarys_memory_failing_at_each_request_takes_no_cell) {
    reset();
    const int blocks_before = armed.blocks;
    const int requests_of_one_add = requests_of_an_add();
    int refused = 0;
    for (int n = 1; n <= requests_of_one_add; ++n) {
        refused += refused_with_no_cell_taken(n) ? 1 : 0;
    }
    std::printf(
        "texture_atlas_test: %d pedido(s) de memoria num add, %d recusa(s) sem celula tomada\n",
        requests_of_one_add, refused);
    GLINTFX_CHECK(requests_of_one_add >= 3);
    GLINTFX_CHECK(refused == requests_of_one_add);
    GLINTFX_CHECK(armed.blocks == blocks_before);
}

// THE COST of texture_atlas.hpp, the block of a cell and the table of image sizes, in bytes. Every
// number is worked out by hand from the header: a cell of an image w x h with margin 1 is
// (w + 2) x (h + 2) texels of 4 bytes. An add asks for its table and its block before anything
// else, so they are the first requests of the add; a later one may be update_texture()'s, which
// can ask for the same number of bytes, and THE COST puts that one apart.
GLINTFX_TEST(texture_atlas_the_block_of_a_cell_costs_what_the_header_says) {
    reset();
    open_atlas a(desc_of(64, 1, 0));
    arm(0);
    GLINTFX_CHECK(add(a, image_of(1, 1, 1)).has_value());
    GLINTFX_CHECK(armed.asked[1] == 36U); // 3 x 3: exactly its 36 bytes, after the table
    arm(0);
    GLINTFX_CHECK(add(a, image_of(4, 4, 1)).has_value());
    GLINTFX_CHECK(armed.asked[0] == 144U); // 6 x 6: 144, more than twice 36, so exactly that
    arm(0);
    GLINTFX_CHECK(add(a, image_of(5, 5, 1)).has_value());
    GLINTFX_CHECK(armed.asked[0] == 288U); // 7 x 7: 196, so 144 doubled, and 288 < 392
    arm(0);
    GLINTFX_CHECK(add(a, image_of(2, 2, 1)).has_value());
    GLINTFX_CHECK(armed.asked[0] != 576U); // 4 x 4: 64 fits, so the block stays
}

GLINTFX_TEST(texture_atlas_the_block_grows_for_a_cell_the_allocator_refuses) {
    reset();
    open_atlas a(desc_of(16, 1, 0));
    GLINTFX_CHECK(add(a, image_of(14, 1, 1)).has_value()); // 16 x 3, 192 bytes, the largest written
    const std::size_t uploads = card.sub_uploads.size();
    arm(0);
    // 16 x 16 fits the atlas, so it reaches the allocator, which has no such place now.
    GLINTFX_CHECK(
        refused_as(add(a, image_of(14, 14, 1)), gltfx_err_code::no_free_space, "atlas_full"));
    GLINTFX_CHECK(armed.asked[0] == 1024U); // 16 x 16 x 4: no such cell was ever written
    GLINTFX_CHECK(card.sub_uploads.size() == uploads);
    // A refusal before the allocator asks for nothing.
    arm(0);
    GLINTFX_CHECK(
        refused_as(add(a, image_of(15, 1, 1)), gltfx_err_code::invalid_argument, "width"));
    GLINTFX_CHECK(armed.requests == 0);
}

namespace {

// `count` adds of a 1 x 1 image: how many asked for 256 bytes first, or failed.
[[nodiscard]] int adds_with_a_larger_table(open_atlas &a, int count) {
    int larger = 0;
    for (int n = 0; n < count; ++n) {
        arm(0);
        const bool added = add(a, image_of(1, 1, 1)).has_value();
        larger += (!added || armed.asked[0] == 256U) ? 1 : 0;
    }
    return larger;
}

} // namespace

GLINTFX_TEST(texture_atlas_the_table_of_image_sizes_costs_what_the_header_says) {
    reset();
    open_atlas a(desc_of(64, 1, 0));
    arm(0);
    GLINTFX_CHECK(add(a, image_of(1, 1, 1)).has_value());
    GLINTFX_CHECK(armed.asked[0] == 128U); // one number now, and 128 bytes: never less
    GLINTFX_CHECK(adds_with_a_larger_table(a, 15) == 0); // up to 16 numbers: the same 128 bytes
    arm(0);
    GLINTFX_CHECK(add(a, image_of(1, 1, 1)).has_value());
    GLINTFX_CHECK(armed.asked[0] == 256U); // the 17th number: 136 needed, 256 < 272
}

GLINTFX_TEST(texture_atlas_a_write_the_card_refuses_gives_the_cell_back) {
    reset();
    open_atlas a(desc_of(32, 1, 0));
    card.error_after_sub_upload = k_out_of_memory;
    const gltfx_rslt<gltfx_atlas_slot> refused = add(a, image_of(2, 2, 1));
    GLINTFX_CHECK(refused_as(refused, gltfx_err_code::out_of_memory, "texture_update"));
    card.error_after_sub_upload = 0;
    const gltfx_rslt<gltfx_atlas_slot> next = add(a, image_of(2, 2, 1));
    // The same place; the number the refused call drew comes back one generation later.
    GLINTFX_CHECK(next.has_value() && rect_is(next.value(), {0, 0, 4, 4}));
    GLINTFX_CHECK(next.value().id == 0U && next.value().generation == 2U);
}

GLINTFX_TEST(texture_atlas_a_smaller_image_in_a_released_place_reads_only_itself) {
    reset();
    open_atlas a(desc_of(32, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> big = add(a, image_of(4, 4, 1));  // cell 6 x 6 at x 0
    const gltfx_rslt<gltfx_atlas_slot> kept = add(a, image_of(2, 2, 2)); // cell 4 x 4 at x 6
    GLINTFX_CHECK(big.has_value() && kept.has_value() && rect_is(kept.value(), {6, 0, 4, 4}));
    GLINTFX_CHECK(release_from_texture_atlas(a.atlas, big.value()).has_value());
    GLINTFX_CHECK(refused_as(region_in_atlas(a.atlas, big.value()),
                             gltfx_err_code::invalid_argument, "stale_slot"));
    GLINTFX_CHECK(refused_as(release_from_texture_atlas(a.atlas, big.value()),
                             gltfx_err_code::invalid_argument, "stale_slot"));
    // Step 1 of the placement: the shelf of 6 rows holds the kept cell, 6 <= 1.5 x 4, free at x 0.
    const gltfx_rslt<gltfx_atlas_slot> small = add(a, image_of(2, 2, 3));
    GLINTFX_CHECK(small.has_value() && rect_is(small.value(), {0, 0, 4, 4}));
    GLINTFX_CHECK(region_is(region_in_atlas(a.atlas, small.value()), {{1.0, 1.0}, {2.0, 2.0}}));
    GLINTFX_CHECK(region_is(region_in_atlas(a.atlas, kept.value()), {{7.0, 1.0}, {2.0, 2.0}}));
}

GLINTFX_TEST(texture_atlas_a_receipt_of_another_atlas_or_a_changed_one_is_refused_by_name) {
    reset();
    open_atlas first(desc_of(32, 1, 0));
    const open_atlas second(desc_of(32, 1, 0));
    const gltfx_rslt<gltfx_atlas_slot> slot = add(first, image_of(2, 2, 1));
    GLINTFX_CHECK(slot.has_value());
    GLINTFX_CHECK(refused_as(region_in_atlas(second.atlas, slot.value()),
                             gltfx_err_code::invalid_argument, "slot_other_allocator"));
    GLINTFX_CHECK(refused_as(release_from_texture_atlas(second.atlas, slot.value()),
                             gltfx_err_code::invalid_argument, "slot_other_allocator"));
    GLINTFX_CHECK(refused_as(region_in_atlas(first.atlas, gltfx_atlas_slot{}),
                             gltfx_err_code::invalid_argument, "stale_slot"));
    gltfx_atlas_slot changed = slot.value();
    changed.rect.width += 1U;
    GLINTFX_CHECK(refused_as(region_in_atlas(first.atlas, changed),
                             gltfx_err_code::invalid_argument, "stale_slot"));
    // The owner is read first: an owner no allocator handed out is not this atlas's, so the receipt
    // is another atlas's; an owner of 0 is a receipt never handed out.
    changed = slot.value();
    changed.owner = k_owner_never_handed_out;
    GLINTFX_CHECK(refused_as(region_in_atlas(first.atlas, changed),
                             gltfx_err_code::invalid_argument, "slot_other_allocator"));
    changed.owner = 0U;
    GLINTFX_CHECK(refused_as(region_in_atlas(first.atlas, changed),
                             gltfx_err_code::invalid_argument, "stale_slot"));
    GLINTFX_CHECK(refused_as(region_in_atlas(nullptr, slot.value()),
                             gltfx_err_code::invalid_argument, "atlas"));
    GLINTFX_CHECK(refused_as(release_from_texture_atlas(nullptr, slot.value()),
                             gltfx_err_code::invalid_argument, "atlas"));
}

GLINTFX_TEST(texture_atlas_every_cell_of_a_long_sequence_starts_on_a_multiple_of_two_to_the_k) {
    // The property the atlas leans on (texture_atlas_impl.hpp, THE ALIGNMENT IS THE ALLOCATOR'S
    // PLACEMENT): 400 adds of sides 1 to 13 and 1 to 11, every third call releasing the oldest kept
    // cell, on an atlas with two reduced levels; every receipt must start and end on a multiple
    // of 4.
    reset();
    open_atlas a(desc_of(256, 4, 2));
    std::vector<gltfx_atlas_slot> kept;
    int added = 0;
    int misaligned = 0;
    for (std::uint32_t i = 0; i < 400U; ++i) {
        const gltfx_rslt<gltfx_atlas_slot> slot =
            add(a, image_of(1U + i * 7U % 13U, 1U + i * 5U % 11U, 1));
        if (slot.has_value()) {
            const glintfx::gltfx_atlas_rect r = slot.value().rect;
            misaligned += (r.x % 4U | r.y % 4U | r.width % 4U | r.height % 4U) != 0U ? 1 : 0;
            ++added;
            kept.push_back(slot.value());
        }
        if (i % 3U == 2U && !kept.empty()) {
            misaligned += release_from_texture_atlas(a.atlas, kept.front()).has_value() ? 0 : 1000;
            kept.erase(kept.begin());
        }
    }
    std::printf("texture_atlas_test: %d celula(s) postas, %d fora de multiplo de 4\n", added,
                misaligned);
    GLINTFX_CHECK(added >= 200);
    GLINTFX_CHECK(misaligned == 0);
}
