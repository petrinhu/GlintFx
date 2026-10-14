// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_region_uv.hpp"

#include <array>
#include <optional>
#include <utility>

// draw2d/texture_region_uv.cpp - R2D-TEXTURE, fatia T4b-A: the coordinates of
// texture_region_uv.hpp.
namespace glintfx::draw2d {

namespace {

struct mirror_flags {
    bool left_right = false;
    bool top_bottom = false;
};

// A switch over every value, with std::nullopt after it: a value a newer glintfx adds is never
// taken for `none` (docs/api-conventions.md R4).
[[nodiscard]] std::optional<mirror_flags> flags_of(glintfx::gltfx_texture_mirror mirror) noexcept {
    switch (mirror) {
    case glintfx::gltfx_texture_mirror::none:
        return mirror_flags{false, false};
    case glintfx::gltfx_texture_mirror::flip_left_right:
        return mirror_flags{true, false};
    case glintfx::gltfx_texture_mirror::flip_top_bottom:
        return mirror_flags{false, true};
    case glintfx::gltfx_texture_mirror::flip_left_right_and_top_bottom:
        return mirror_flags{true, true};
    }
    return std::nullopt;
}

} // namespace

std::optional<std::array<texture_coordinate, 4>>
region_corner_coordinates(const glintfx::gltfx_texture_region &region,
                          texture_extent extent) noexcept {
    const std::optional<mirror_flags> flags = flags_of(region.mirror);
    if (!flags.has_value() || extent.width == 0U || extent.height == 0U) {
        return std::nullopt;
    }
    const double width = static_cast<double>(extent.width);
    const double height = static_cast<double>(extent.height);
    double left = region.source.corner.x / width;
    double right = (region.source.corner.x + region.source.size.x) / width;
    double top = region.source.corner.y / height;
    double bottom = (region.source.corner.y + region.source.size.y) / height;
    if (flags->left_right) {
        std::swap(left, right);
    }
    if (flags->top_bottom) {
        std::swap(top, bottom);
    }
    return std::array<texture_coordinate, 4>{
        texture_coordinate{left, top}, texture_coordinate{right, top},
        texture_coordinate{right, bottom}, texture_coordinate{left, bottom}};
}

} // namespace glintfx::draw2d
