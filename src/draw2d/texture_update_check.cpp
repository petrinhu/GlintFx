// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_update_check.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "draw2d/pixel_premultiply.hpp"

// draw2d/texture_update_check.cpp - R2D-TEXTURE, fatia T3: the check of texture_update_check.hpp,
// in the order its header lists. Each step is one atom: the record, the view, the rectangle.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_err;
using glintfx::gltfx_err_code;
using glintfx::gltfx_rslt;

using rect_answer = std::optional<texel_rect>;

// The one refusal of this file: invalid_argument, naming the field that was refused.
[[nodiscard]] gltfx_err refusal(std::string_view field) noexcept {
    return gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field);
}

// "texture", then "updatable": the record must exist, belong to the renderer, and be updatable.
[[nodiscard]] gltfx_rslt<void> check_record(const texture_update_request &request) noexcept {
    if (request.texture == nullptr || request.texture->access.owner != request.renderer) {
        return gltfx_rslt<void>::err(refusal("texture"));
    }
    if (!request.texture->updatable) {
        return gltfx_rslt<void>::err(refusal("updatable"));
    }
    return gltfx_rslt<void>::ok();
}

// A span that starts at `corner` and covers `length` texels reaches past a side of `side` texels.
// The sum is made in 64 bits, so no wrap of 32 bits lets it pass.
[[nodiscard]] bool reaches_past(std::uint32_t corner, std::uint32_t length,
                                std::uint32_t side) noexcept {
    return static_cast<std::uint64_t>(corner) + length > side;
}

// "rect": the image, put at (x, y), must fit in the texture. A rectangle with no width may start
// at x equal to the texture's width (and likewise for y).
[[nodiscard]] gltfx_rslt<void> check_rect(const texture_update_request &request) noexcept {
    if (reaches_past(request.x, request.image.width, request.texture->width) ||
        reaches_past(request.y, request.image.height, request.texture->height)) {
        return gltfx_rslt<void>::err(refusal("rect"));
    }
    return gltfx_rslt<void>::ok();
}

// The texels the image covers, both ends included. Only called with a non-empty image.
[[nodiscard]] texel_rect rect_of(const texture_update_request &request) noexcept {
    return texel_rect{
        .columns = covered_range{.first = request.x, .last = request.x + request.image.width - 1U},
        .rows = covered_range{.first = request.y, .last = request.y + request.image.height - 1U},
    };
}

} // namespace

gltfx_rslt<std::optional<texel_rect>>
check_texture_update(const texture_update_request &request) noexcept {
    const gltfx_rslt<void> record = check_record(request);
    if (record.has_error()) {
        return gltfx_rslt<rect_answer>::err(record.err());
    }
    const gltfx_rslt<void> view = check_image_view(request.image);
    if (view.has_error()) {
        return gltfx_rslt<rect_answer>::err(view.err());
    }
    const gltfx_rslt<void> rect = check_rect(request);
    if (rect.has_error()) {
        return gltfx_rslt<rect_answer>::err(rect.err());
    }
    if (request.image.width == 0U || request.image.height == 0U) {
        return gltfx_rslt<rect_answer>::ok(std::nullopt); // nothing to write, and no GL call
    }
    return gltfx_rslt<rect_answer>::ok(rect_of(request));
}

} // namespace glintfx::draw2d
