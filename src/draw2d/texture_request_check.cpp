// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_request_check.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/pixel_premultiply.hpp"

namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_all_mipmap_levels;
using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_image_view;
using glintfx::gltfx_rslt;
using glintfx::gltfx_texture_2d_desc;
using glintfx::gltfx_texture_filter;

// The one refusal of this file: invalid_argument, naming the field that was refused.
[[nodiscard]] gltfx_err refusal(std::string_view field) noexcept {
    return gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field);
}

// "width", then "height": a zero side is refused by name before anything else is read.
[[nodiscard]] gltfx_rslt<void> check_sides(const gltfx_image_view &image) noexcept {
    if (image.width == 0U) {
        return gltfx_rslt<void>::err(refusal("width"));
    }
    if (image.height == 0U) {
        return gltfx_rslt<void>::err(refusal("height"));
    }
    return gltfx_rslt<void>::ok();
}

[[nodiscard]] bool is_known_filter(gltfx_texture_filter filter) noexcept {
    switch (filter) {
    case gltfx_texture_filter::nearest:
    case gltfx_texture_filter::linear:
        return true;
    }
    return false;
}

// gltfx_all_mipmap_levels asks for the whole chain, so it is resolved to the limit and never
// compared with it; any other count above the limit is refused by name.
[[nodiscard]] gltfx_rslt<std::uint32_t>
resolve_mipmap_levels(const gltfx_image_view &image, const gltfx_texture_2d_desc &desc) noexcept {
    const std::uint32_t limit = reduced_level_limit(image.width, image.height);
    if (desc.mipmap_levels == gltfx_all_mipmap_levels) {
        return gltfx_rslt<std::uint32_t>::ok(limit);
    }
    if (desc.mipmap_levels > limit) {
        return gltfx_rslt<std::uint32_t>::err(refusal("mipmap_levels"));
    }
    return gltfx_rslt<std::uint32_t>::ok(desc.mipmap_levels);
}

} // namespace

std::uint32_t reduced_level_limit(std::uint32_t width, std::uint32_t height) noexcept {
    const std::uint32_t side = std::max(width, height);
    const auto bits = static_cast<std::uint32_t>(std::bit_width(side));
    return bits == 0U ? 0U : bits - 1U;
}

gltfx_rslt<std::uint32_t> check_texture_request(const gltfx_image_view &image,
                                                const gltfx_texture_2d_desc &desc) noexcept {
    const gltfx_rslt<void> sides = check_sides(image);
    if (sides.has_error()) {
        return gltfx_rslt<std::uint32_t>::err(sides.err());
    }
    const gltfx_rslt<void> view = check_image_view(image);
    if (view.has_error()) {
        return gltfx_rslt<std::uint32_t>::err(view.err());
    }
    if (!is_known_filter(desc.filter)) {
        return gltfx_rslt<std::uint32_t>::err(refusal("filter"));
    }
    return resolve_mipmap_levels(image, desc);
}

} // namespace glintfx::draw2d
