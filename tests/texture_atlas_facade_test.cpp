// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>

#include "draw2d/renderer_2d_impl.hpp"
#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_atlas_impl.hpp"
#include "draw2d/texture_release.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_atlas_facade_test.cpp - R2D-TEXTURE, fatia T6-B1-fix1 (GODS_LAWS.md L-09 rule 6, L-20,
// L-22). The public handle gltfx_texture_atlas over a LINK SEAM: texture_atlas_facade.cpp and
// texture_2d_facade.cpp are recompiled into this test (tests/CMakeLists.txt), and everything they
// forward to is a fake below, which keeps what it was given. So what texture_atlas.hpp, LIFETIME,
// says of a move is counted without a window: which interior and which texture record each call
// passes on, and how many times each is given back. The memory an interior holds cannot be
// counted through the handle, because open() always uses the library's own; giving the interior
// back is what this test counts, and draw2d::destroy_texture_atlas() itself is texture_atlas_test.
//
// A DECLARED DOWNGRADE (L-09 rule 6): this is the seam of the decision, not the library end to end.
// The refusals by name for a moved-from atlas are texture_atlas_test (the interior called with
// nullptr) and, through the real library on a real context, cell AT5 of draw2d_texture_parity_test.

using glintfx::gltfx_atlas_slot;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_rect_texel;
using glintfx::gltfx_renderer_2d;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d;
using glintfx::gltfx_texture_atlas;
using glintfx::gltfx_texture_atlas_desc;
using glintfx::texture_2d_impl;
using glintfx::texture_atlas_impl;

namespace {

// The layout of texture_atlas.hpp (D-W8-1204, a one-way door): the descriptor by field, and the
// handle by size, its two members being private. Five fields of 4 bytes but the filter's 1, which
// the alignment of the field after it pads to 4, the same under every data model the library
// targets. docs/auditoria-api-texture.md, section 9, says what else was checked.
static_assert(offsetof(gltfx_texture_atlas_desc, width) == 0);
static_assert(offsetof(gltfx_texture_atlas_desc, height) == 4);
static_assert(offsetof(gltfx_texture_atlas_desc, margin_pixels) == 8);
static_assert(offsetof(gltfx_texture_atlas_desc, filter) == 12);
static_assert(sizeof(gltfx_texture_atlas_desc::filter) == 1);
static_assert(offsetof(gltfx_texture_atlas_desc, mipmap_levels) == 16);
static_assert(sizeof(gltfx_texture_atlas_desc) == 20);
static_assert(std::is_standard_layout_v<gltfx_texture_atlas_desc>);
static_assert(std::is_trivially_copyable_v<gltfx_texture_atlas_desc>);
// The receipt goes by value in two signatures and comes back in a third: seven 32-bit integers.
static_assert(sizeof(gltfx_atlas_slot) == 28);
static_assert(offsetof(gltfx_atlas_slot, rect) == 0);
static_assert(offsetof(gltfx_atlas_slot, id) == 16);
static_assert(offsetof(gltfx_atlas_slot, generation) == 20);
static_assert(offsetof(gltfx_atlas_slot, owner) == 24);
static_assert(sizeof(gltfx_texture_atlas) == 2 * sizeof(void *));
static_assert(alignof(gltfx_texture_atlas) == alignof(void *));
static_assert(!std::is_copy_constructible_v<gltfx_texture_atlas>);
static_assert(!std::is_copy_assignable_v<gltfx_texture_atlas>);
static_assert(std::is_nothrow_move_constructible_v<gltfx_texture_atlas>);
static_assert(std::is_nothrow_move_assignable_v<gltfx_texture_atlas>);
static_assert(std::is_nothrow_destructible_v<gltfx_texture_atlas>);

// open() takes the renderer only to hand it to renderer_2d_internal_access::get(), and that hands
// the interior only to its texture_access(): both are fakes here, and neither reads a member, so
// neither object is ever built.
union never_built {
    never_built() noexcept {}
    ~never_built() {}
    gltfx_renderer_2d renderer;
    glintfx::renderer_2d_impl interior;
};
never_built nothing;

// What the fakes hand out, and the log of what the handle passed on.
constexpr int k_interiors = 3;
texture_atlas_impl interiors[k_interiors];
texture_2d_impl texture_records[k_interiors];
int opened = 0;
bool refuse_open = false;

struct passed_on {
    const texture_atlas_impl *atlas = nullptr;
    const texture_2d_impl *texture = nullptr;
    int calls = 0;
};
passed_on added;
passed_on read;
passed_on released;
// In the order they happened; nullptr is never logged, because giving back nothing gives nothing.
std::vector<const void *> given_back;

void reset() {
    opened = 0;
    refuse_open = false;
    added = passed_on{};
    read = passed_on{};
    released = passed_on{};
    given_back.clear();
    for (int i = 0; i < k_interiors; ++i) {
        texture_records[i].width = 32U + static_cast<std::uint32_t>(i);
    }
}

[[nodiscard]] gltfx_texture_atlas open_one() {
    const gltfx_texture_atlas_desc desc{.width = 32, .height = 32};
    gltfx_rslt<gltfx_texture_atlas> made = gltfx_texture_atlas::open(nothing.renderer, desc);
    return std::move(made.value());
}

[[nodiscard]] bool given_back_is(const std::vector<const void *> &want) {
    return given_back == want;
}

[[nodiscard]] bool passes_nothing_on(gltfx_texture_atlas &atlas) {
    added = passed_on{};
    read = passed_on{};
    released = passed_on{};
    const gltfx_rslt<gltfx_atlas_slot> add_result = atlas.add_image(gltfx_image_view{});
    const gltfx_rslt<gltfx_rect_texel> region = atlas.region_of(gltfx_atlas_slot{});
    const gltfx_rslt<void> release = atlas.release_slot(gltfx_atlas_slot{});
    return added.calls == 1 && read.calls == 1 && released.calls == 1 && added.atlas == nullptr &&
           added.texture == nullptr && read.atlas == nullptr && released.atlas == nullptr &&
           add_result.has_error() && region.has_error() && release.has_error();
}

[[nodiscard]] bool passes_on(gltfx_texture_atlas &atlas, int index) {
    added = passed_on{};
    read = passed_on{};
    released = passed_on{};
    static_cast<void>(atlas.add_image(gltfx_image_view{}));
    static_cast<void>(atlas.region_of(gltfx_atlas_slot{}));
    static_cast<void>(atlas.release_slot(gltfx_atlas_slot{}));
    return added.atlas == &interiors[index] && added.texture == &texture_records[index] &&
           read.atlas == &interiors[index] && released.atlas == &interiors[index];
}

} // namespace

// The fakes of what the two recompiled facade files forward to.
namespace glintfx {

renderer_2d_impl *renderer_2d_internal_access::get(gltfx_renderer_2d &) noexcept {
    return &nothing.interior;
}

draw2d::texture_gl_access renderer_2d_impl::texture_access() noexcept { return {}; }

namespace draw2d {

gltfx_rslt<texture_atlas_made> make_texture_atlas(const texture_gl_access &,
                                                  const gltfx_texture_atlas_desc &,
                                                  const batch_allocator &) noexcept {
    if (refuse_open) {
        return gltfx_rslt<texture_atlas_made>::err(
            gltfx_err(gltfx_err_code::out_of_memory).with_rejected_value("atlas_open"));
    }
    const int index = opened++;
    return gltfx_rslt<texture_atlas_made>::ok(
        texture_atlas_made{&interiors[index], &texture_records[index]});
}

void destroy_texture_atlas(texture_atlas_impl *atlas) noexcept {
    if (atlas != nullptr) {
        given_back.push_back(atlas);
    }
}

void release_texture_object(texture_2d_impl *texture) noexcept {
    if (texture != nullptr) {
        given_back.push_back(texture);
    }
}

gltfx_rslt<gltfx_atlas_slot> add_image_to_atlas(texture_atlas_impl *atlas, texture_2d_impl *texture,
                                                const gltfx_image_view &) noexcept {
    added = passed_on{atlas, texture, added.calls + 1};
    return gltfx_rslt<gltfx_atlas_slot>::err(gltfx_err(gltfx_err_code::invalid_argument));
}

gltfx_rslt<gltfx_rect_texel> region_in_atlas(const texture_atlas_impl *atlas,
                                             const gltfx_atlas_slot &) noexcept {
    read = passed_on{atlas, nullptr, read.calls + 1};
    return gltfx_rslt<gltfx_rect_texel>::err(gltfx_err(gltfx_err_code::invalid_argument));
}

gltfx_rslt<void> release_from_texture_atlas(texture_atlas_impl *atlas,
                                            const gltfx_atlas_slot &) noexcept {
    released = passed_on{atlas, nullptr, released.calls + 1};
    return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::invalid_argument));
}

} // namespace draw2d

} // namespace glintfx

namespace {

// Each scope below is a function, so that what is given back when the handles die is read after it.
void an_open_atlas_passes_on_what_it_holds() {
    gltfx_texture_atlas atlas = open_one();
    GLINTFX_CHECK(atlas.is_open());
    GLINTFX_CHECK(atlas.texture().is_open() && atlas.texture().width() == 32U);
    GLINTFX_CHECK(passes_on(atlas, 0));
    GLINTFX_CHECK(given_back.empty());
}

void a_moved_atlas_is_read_on_both_sides() {
    gltfx_texture_atlas source = open_one();
    const gltfx_texture_2d &taken_before = source.texture();
    gltfx_texture_atlas target = std::move(source);
    GLINTFX_CHECK(!taken_before.is_open());
    // The moved-from handle is read on purpose: its state is the promise under test.
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: see above
    // cppcheck-suppress accessMoved ; reason: see above
    GLINTFX_CHECK(!source.is_open());
    // cppcheck-suppress accessMoved ; reason: see above
    GLINTFX_CHECK(!source.texture().is_open() && source.texture().width() == 0U);
    // cppcheck-suppress accessMoved ; reason: see above
    GLINTFX_CHECK(passes_nothing_on(source));
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: closes the block
    GLINTFX_CHECK(target.is_open() && target.texture().width() == 32U);
    GLINTFX_CHECK(passes_on(target, 0));
    GLINTFX_CHECK(given_back.empty());
}

void an_assigned_atlas_is_read_on_both_sides() {
    gltfx_texture_atlas receiver = open_one();
    gltfx_texture_atlas source = open_one();
    receiver = std::move(source);
    // Back at once and once each, before anything of the source is used.
    GLINTFX_CHECK(given_back_is({&interiors[0], &texture_records[0]}));
    // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: the promise
    // cppcheck-suppress accessMoved ; reason: the promise under test
    GLINTFX_CHECK(!source.is_open());
    // cppcheck-suppress accessMoved ; reason: the promise under test
    GLINTFX_CHECK(!source.texture().is_open());
    // cppcheck-suppress accessMoved ; reason: the promise under test
    GLINTFX_CHECK(passes_nothing_on(source));
    // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move) reason: closes the block
    GLINTFX_CHECK(receiver.is_open() && receiver.texture().width() == 33U);
    GLINTFX_CHECK(passes_on(receiver, 1));
}

void an_atlas_assigned_to_itself_is_read() {
    gltfx_texture_atlas atlas = open_one();
    gltfx_texture_atlas &same = atlas;
    atlas = std::move(same);
    GLINTFX_CHECK(atlas.is_open());
    GLINTFX_CHECK(passes_on(atlas, 0));
    GLINTFX_CHECK(given_back.empty());
}

} // namespace

GLINTFX_TEST(texture_atlas_facade_open_holds_what_was_made_and_a_refusal_comes_as_it_came) {
    reset();
    an_open_atlas_passes_on_what_it_holds();
    GLINTFX_CHECK(given_back_is({&interiors[0], &texture_records[0]}));

    reset();
    refuse_open = true;
    const gltfx_texture_atlas_desc desc{.width = 32, .height = 32};
    const gltfx_rslt<gltfx_texture_atlas> refused =
        gltfx_texture_atlas::open(nothing.renderer, desc);
    GLINTFX_CHECK(refused.has_error() && refused.err().code() == gltfx_err_code::out_of_memory &&
                  refused.err().rejected_value() == std::string_view("atlas_open"));
    GLINTFX_CHECK(given_back.empty());
}

GLINTFX_TEST(texture_atlas_facade_a_moved_from_atlas_holds_nothing_and_gives_nothing_back) {
    reset();
    a_moved_atlas_is_read_on_both_sides();
    GLINTFX_CHECK(given_back_is({&interiors[0], &texture_records[0]}));
}

GLINTFX_TEST(texture_atlas_facade_move_assignment_gives_its_own_back_first_and_takes_the_other) {
    reset();
    an_assigned_atlas_is_read_on_both_sides();
    GLINTFX_CHECK(
        given_back_is({&interiors[0], &texture_records[0], &interiors[1], &texture_records[1]}));
}

GLINTFX_TEST(texture_atlas_facade_assigning_an_atlas_to_itself_changes_nothing) {
    reset();
    an_atlas_assigned_to_itself_is_read();
    GLINTFX_CHECK(given_back_is({&interiors[0], &texture_records[0]}));
}
