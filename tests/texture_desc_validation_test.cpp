// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/texture_request_check.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_desc_validation_test.cpp - R2D-TEXTURE, fatia T2a: the pure check of a request to make a
// texture (draw2d/texture_request_check.hpp), every refusal by name and in the order the public
// text of gltfx_renderer_2d::create_texture() lists. No GL and no renderer: the size limit of the
// graphics card is texture_upload_test's.

using glintfx::gltfx_all_mipmap_levels;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_pixel_format;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d_desc;
using glintfx::gltfx_texture_filter;
using glintfx::draw2d::check_texture_request;
using glintfx::draw2d::reduced_level_limit;

namespace {

// Enough bytes for every view below (8 x 8 texels, four bytes each).
const std::vector<std::byte> k_bytes(256, std::byte{0x40});

[[nodiscard]] gltfx_image_view view(std::uint32_t width, std::uint32_t height) {
    return gltfx_image_view{.pixels = std::span<const std::byte>(k_bytes),
                            .width = width,
                            .height = height,
                            .bytes_per_row = static_cast<std::size_t>(width) * 4,
                            .pixel_format = gltfx_pixel_format::rgba8_srgb_straight};
}

// A prvalue, never a named local: MSVC /O2 /fsanitize=address copied the padding of a named local
// after the end of its scope, a stack-use-after-scope that no line of this file commits (T2a-win).
[[nodiscard]] gltfx_texture_2d_desc levels(std::uint32_t count) {
    return gltfx_texture_2d_desc{.mipmap_levels = count};
}

// A value of each enumeration that this build does not know: the cast is the case (R4).
[[nodiscard]] gltfx_pixel_format unknown_pixel_format() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_pixel_format>(9);
}

[[nodiscard]] gltfx_texture_filter unknown_filter() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_texture_filter>(7);
}

// The request is refused with invalid_argument and exactly this field.
[[nodiscard]] bool refused_as(const gltfx_rslt<std::uint32_t> &result, std::string_view field) {
    return result.has_error() && result.err().code() == gltfx_err_code::invalid_argument &&
           result.err().rejected_value() == field;
}

// The request is accepted with exactly this many reduced levels.
[[nodiscard]] bool accepted_with(const gltfx_rslt<std::uint32_t> &result, std::uint32_t count) {
    return result.has_value() && result.value() == count;
}

} // namespace

GLINTFX_TEST(a_plain_request_is_accepted_without_mipmaps) {
    GLINTFX_CHECK(accepted_with(check_texture_request(view(2, 2), {}), 0));
}

GLINTFX_TEST(zero_width_is_refused_by_name) {
    GLINTFX_CHECK(refused_as(check_texture_request(view(0, 3), {}), "width"));
}

GLINTFX_TEST(zero_height_is_refused_by_name) {
    GLINTFX_CHECK(refused_as(check_texture_request(view(3, 0), {}), "height"));
}

GLINTFX_TEST(unspecified_pixel_format_is_refused_by_name) {
    gltfx_image_view image = view(2, 2);
    image.pixel_format = gltfx_pixel_format::unspecified;
    GLINTFX_CHECK(refused_as(check_texture_request(image, {}), "pixel_format"));
}

GLINTFX_TEST(a_pixel_format_this_build_does_not_know_is_refused_by_name) {
    gltfx_image_view image = view(2, 2);
    image.pixel_format = unknown_pixel_format();
    GLINTFX_CHECK(refused_as(check_texture_request(image, {}), "pixel_format"));
}

GLINTFX_TEST(a_row_shorter_than_the_image_is_refused_by_name) {
    gltfx_image_view image = view(2, 2);
    image.bytes_per_row = 7;
    GLINTFX_CHECK(refused_as(check_texture_request(image, {}), "bytes_per_row"));
}

GLINTFX_TEST(pixels_shorter_than_the_image_are_refused_by_name) {
    gltfx_image_view image = view(2, 2);
    image.pixels = std::span<const std::byte>(k_bytes).first(15);
    GLINTFX_CHECK(refused_as(check_texture_request(image, {}), "pixels"));
}

GLINTFX_TEST(a_filter_outside_the_enumeration_is_refused_by_name) {
    gltfx_texture_2d_desc desc;
    desc.filter = unknown_filter();
    GLINTFX_CHECK(refused_as(check_texture_request(view(2, 2), desc), "filter"));
}

// 8 x 2 allows three reduced levels (4 x 1, 2 x 1, 1 x 1): three is accepted, four is not.
GLINTFX_TEST(mipmap_levels_above_the_chain_are_refused_at_the_boundary) {
    GLINTFX_CHECK(accepted_with(check_texture_request(view(8, 2), levels(3)), 3));
    GLINTFX_CHECK(refused_as(check_texture_request(view(8, 2), levels(4)), "mipmap_levels"));
}

// Zero means no mipmap, so a 1 x 1 texture has no reduced level to give: one is refused.
GLINTFX_TEST(a_one_texel_texture_has_no_reduced_level) {
    GLINTFX_CHECK(accepted_with(check_texture_request(view(1, 1), levels(0)), 0));
    GLINTFX_CHECK(refused_as(check_texture_request(view(1, 1), levels(1)), "mipmap_levels"));
}

// The largest uint32 is the request for every level, never a count to compare.
GLINTFX_TEST(all_mipmap_levels_asks_for_the_whole_chain) {
    GLINTFX_CHECK(
        accepted_with(check_texture_request(view(8, 2), levels(gltfx_all_mipmap_levels)), 3));
    GLINTFX_CHECK(
        accepted_with(check_texture_request(view(1, 1), levels(gltfx_all_mipmap_levels)), 0));
}

// bytes_per_row comes before pixels (the published order): a view whose row is short AND whose
// pixels are short is refused as "bytes_per_row".
GLINTFX_TEST(a_short_row_is_named_before_short_pixels) {
    gltfx_image_view image = view(2, 2);
    image.bytes_per_row = 7;
    image.pixels = std::span<const std::byte>(k_bytes).first(3);
    GLINTFX_CHECK(refused_as(check_texture_request(image, {}), "bytes_per_row"));
}

// The order of the public text: size, then the view, then the filter, then the levels.
GLINTFX_TEST(the_first_refusal_in_the_published_order_wins) {
    gltfx_image_view no_width = view(0, 0);
    no_width.pixel_format = gltfx_pixel_format::unspecified;
    GLINTFX_CHECK(refused_as(check_texture_request(no_width, {}), "width"));

    gltfx_texture_2d_desc bad_filter = levels(9);
    bad_filter.filter = unknown_filter();
    gltfx_image_view no_format = view(2, 2);
    no_format.pixel_format = gltfx_pixel_format::unspecified;
    GLINTFX_CHECK(refused_as(check_texture_request(no_format, bad_filter), "pixel_format"));
    GLINTFX_CHECK(refused_as(check_texture_request(view(2, 2), bad_filter), "filter"));
}

// floor(log2(max(width, height))).
GLINTFX_TEST(reduced_level_limit_is_the_length_of_the_chain) {
    GLINTFX_CHECK_EQ(reduced_level_limit(1, 1), 0U);
    GLINTFX_CHECK_EQ(reduced_level_limit(2, 1), 1U);
    GLINTFX_CHECK_EQ(reduced_level_limit(1, 2), 1U);
    GLINTFX_CHECK_EQ(reduced_level_limit(3, 1), 1U);
    GLINTFX_CHECK_EQ(reduced_level_limit(4, 1), 2U);
    GLINTFX_CHECK_EQ(reduced_level_limit(5, 3), 2U);
    GLINTFX_CHECK_EQ(reduced_level_limit(255, 1), 7U);
    GLINTFX_CHECK_EQ(reduced_level_limit(256, 1), 8U);
    GLINTFX_CHECK_EQ(reduced_level_limit(1, 256), 8U);
    GLINTFX_CHECK_EQ(reduced_level_limit(0xFFFFFFFFU, 1), 31U);
}
