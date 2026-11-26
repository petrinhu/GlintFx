// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/stroke_rect_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// draw2d/stroke_rect_geometry.cpp - R2D-SHAPES, fatia S4: the two polygons, the points and the
// triangles of stroke_rect_geometry.hpp.
namespace glintfx::draw2d {

namespace {

// How far the outer and the inner polygon lie from the rectangle's edge, in the unit of the
// thickness: `outward` out of the rectangle, `inward` into it.
struct edge_reach {
    double outward = 0.0;
    double inward = 0.0;
};

// The outward sign of one corner, in the order of core/quad.hpp.
struct corner_sign {
    double x = 0.0;
    double y = 0.0;
};

// A direction on the screen, of unit length.
struct screen_direction {
    double x = 0.0;
    double y = 0.0;
};

// The directions of the two sides of the rectangle on the screen: its width and its height.
struct screen_axes {
    screen_direction width{};
    screen_direction height{};
};

constexpr std::array<corner_sign, 4> k_corner_signs{{
    {-1.0, -1.0},
    {1.0, -1.0},
    {1.0, 1.0},
    {-1.0, 1.0},
}};

[[nodiscard]] edge_reach reach_of(glintfx::gltfx_stroke_align align, double thickness) noexcept {
    switch (align) {
    case glintfx::gltfx_stroke_align::inside:
        return edge_reach{0.0, thickness};
    case glintfx::gltfx_stroke_align::outside:
        return edge_reach{thickness, 0.0};
    case glintfx::gltfx_stroke_align::centered:
        break;
    }
    return edge_reach{0.5 * thickness, 0.5 * thickness};
}

// The inner polygon has area when its side, measured in the unit of the thickness, is wider than
// the two reaches together. The halves are compared, so no difference of two large numbers is
// taken.
[[nodiscard]] bool keeps_inner(double length, double inward) noexcept {
    return (0.5 * length) - inward > 0.0;
}

// The four corners of the rectangle as the consumer wrote it, in the order of core/quad.hpp.
[[nodiscard]] quad_corners_world world_corners_of(const glintfx::gltfx_rect_world &rect) noexcept {
    const double right = rect.corner.x + rect.size.x;
    const double bottom = rect.corner.y + rect.size.y;
    return quad_corners_world{
        glintfx::gltfx_vec2_world{rect.corner.x, rect.corner.y},
        glintfx::gltfx_vec2_world{right, rect.corner.y},
        glintfx::gltfx_vec2_world{right, bottom},
        glintfx::gltfx_vec2_world{rect.corner.x, bottom},
    };
}

// The unit direction of (x, y), or nothing for the zero vector. Divides by the larger coordinate
// first, so the length neither overflows nor vanishes.
[[nodiscard]] std::optional<screen_direction> unit_of(double x, double y) noexcept {
    const double larger = std::max(std::fabs(x), std::fabs(y));
    if (larger == 0.0) {
        return std::nullopt;
    }
    const double scaled_x = x / larger;
    const double scaled_y = y / larger;
    const double length = std::hypot(scaled_x, scaled_y);
    return screen_direction{scaled_x / length, scaled_y / length};
}

// The screen directions of the two sides. A side the transform flattens takes the other side
// turned a quarter of a turn; with both flattened, +x and +y of the screen.
[[nodiscard]] screen_axes screen_axes_of(const pixel_affine &affine) noexcept {
    screen_axes axes{screen_direction{1.0, 0.0}, screen_direction{0.0, 1.0}};
    const std::optional<screen_direction> width = unit_of(affine.column0_x, affine.column0_y);
    const std::optional<screen_direction> height = unit_of(affine.column1_x, affine.column1_y);
    if (width) {
        axes.width = *width;
    }
    if (height) {
        axes.height = *height;
    }
    if (!width && height) {
        axes.width = screen_direction{axes.height.y, -axes.height.x};
    }
    if (width && !height) {
        axes.height = screen_direction{-axes.width.y, axes.width.x};
    }
    return axes;
}

// A corner moved by `distance` along its outward sign, in the world.
[[nodiscard]] glintfx::gltfx_vec2_world
offset_in_world(const glintfx::gltfx_vec2_world &from, double distance, corner_sign sign) noexcept {
    return glintfx::gltfx_vec2_world{from.x + (sign.x * distance), from.y + (sign.y * distance)};
}

// A pixel corner moved by `distance` along the two sides of the screen, toward its sign.
[[nodiscard]] pixel_position_wide offset_on_screen(const pixel_position_wide &from, double distance,
                                                   corner_sign sign,
                                                   const screen_axes &axes) noexcept {
    const double dx = (sign.x * axes.width.x) + (sign.y * axes.height.x);
    const double dy = (sign.x * axes.width.y) + (sign.y * axes.height.y);
    return pixel_position_wide{from.x + (distance * dx), from.y + (distance * dy)};
}

// Unit WORLD: the polygons grow and shrink in the world, then the transform carries them.
[[nodiscard]] rect_outline world_outline(const glintfx::gltfx_rect_world &rect,
                                         const edge_reach &reach,
                                         const pixel_affine &affine) noexcept {
    rect_outline outline{};
    outline.has_hole =
        keeps_inner(rect.size.x, reach.inward) && keeps_inner(rect.size.y, reach.inward);
    const quad_corners_world corners = world_corners_of(rect);
    for (std::size_t k = 0; k < 4U; ++k) {
        outline.outer[k] = pixel_position_of(
            offset_in_world(corners[k], reach.outward, k_corner_signs[k]), affine);
        outline.inner[k] = pixel_position_of(
            offset_in_world(corners[k], -reach.inward, k_corner_signs[k]), affine);
    }
    return outline;
}

// Unit PIXEL: the corners are carried first, then the polygons grow on the screen.
[[nodiscard]] rect_outline pixel_outline(const glintfx::gltfx_rect_world &rect,
                                         const edge_reach &reach,
                                         const pixel_affine &affine) noexcept {
    rect_outline outline{};
    const screen_axes axes = screen_axes_of(affine);
    const double width_on_screen = std::hypot(affine.column0_x, affine.column0_y) * rect.size.x;
    const double height_on_screen = std::hypot(affine.column1_x, affine.column1_y) * rect.size.y;
    outline.has_hole =
        keeps_inner(width_on_screen, reach.inward) && keeps_inner(height_on_screen, reach.inward);
    const quad_corners_world corners = world_corners_of(rect);
    for (std::size_t k = 0; k < 4U; ++k) {
        const pixel_position_wide corner = pixel_position_of(corners[k], affine);
        outline.outer[k] = offset_on_screen(corner, reach.outward, k_corner_signs[k], axes);
        outline.inner[k] = offset_on_screen(corner, -reach.inward, k_corner_signs[k], axes);
    }
    return outline;
}

} // namespace

rect_outline rect_outline_of(const glintfx::gltfx_rect_world &rect,
                             const glintfx::gltfx_rect_outline_style &style,
                             const pixel_affine &affine) noexcept {
    const edge_reach reach = reach_of(style.align, style.stroke.thickness);
    if (style.stroke.unit == glintfx::gltfx_stroke_unit::pixel) {
        return pixel_outline(rect, reach, affine);
    }
    return world_outline(rect, reach, affine);
}

std::size_t write_rect_outline_points(const rect_outline &outline,
                                      std::span<pixel_position_wide> points) noexcept {
    if (points.size() < k_rect_outline_max_points) {
        return 0U;
    }
    for (std::size_t k = 0; k < 4U; ++k) {
        points[k] = outline.outer[k];
    }
    if (!outline.has_hole) {
        return 4U;
    }
    for (std::size_t k = 0; k < 4U; ++k) {
        points[4U + k] = outline.inner[k];
    }
    return k_rect_outline_max_points;
}

std::size_t write_rect_outline_indices(bool has_hole, std::span<std::uint32_t> indices) noexcept {
    if (indices.size() < k_rect_outline_max_indices) {
        return 0U;
    }
    if (!has_hole) {
        indices[0] = 0U;
        indices[1] = 1U;
        indices[2] = 2U;
        indices[3] = 0U;
        indices[4] = 2U;
        indices[5] = 3U;
        return 6U;
    }
    for (std::uint32_t side = 0U; side < 4U; ++side) {
        const std::uint32_t next = (side + 1U) % 4U;
        indices[(6U * side) + 0U] = side;
        indices[(6U * side) + 1U] = next;
        indices[(6U * side) + 2U] = 4U + next;
        indices[(6U * side) + 3U] = side;
        indices[(6U * side) + 4U] = 4U + next;
        indices[(6U * side) + 5U] = 4U + side;
    }
    return k_rect_outline_max_indices;
}

} // namespace glintfx::draw2d
