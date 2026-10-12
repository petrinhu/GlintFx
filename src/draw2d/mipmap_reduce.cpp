// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/mipmap_reduce.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>

#include "core/srgb_transfer.hpp"
#include "draw2d/pixel_premultiply.hpp"

namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_rgba8;

constexpr float k_byte_max = 255.0F;
constexpr std::uint32_t k_shift_bits = 32U;

// The length of a level in each axis.
struct plane {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

// One reduction: the level above (from, with its texels) and the level made from it (to).
struct reduction {
    std::span<const gltfx_rgba8> source;
    plane from;
    plane to;
};

// The premultiplied linear sums of one destination texel, before they are stored as bytes.
struct texel_sum {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    float alpha = 0.0F;
};

[[nodiscard]] float linear_of_stored(std::uint8_t byte) noexcept {
    return glintfx::srgb_channel_to_linear(static_cast<float>(byte) / k_byte_max);
}

[[nodiscard]] std::size_t texel_index(plane level, std::uint32_t x, std::uint32_t y) noexcept {
    return static_cast<std::size_t>(y) * level.width + x;
}

// The share of source texel `from` that destination texel `target` takes along one axis: the
// overlap of the two scaled intervals over the source length, so the shares of one destination
// texel add up to one. A side of odd length gives partial shares, as the design requires.
[[nodiscard]] float axis_weight(std::uint32_t from, std::uint32_t target,
                                std::uint32_t source_length, std::uint32_t target_length) noexcept {
    const auto n = static_cast<std::uint64_t>(source_length);
    const auto m = static_cast<std::uint64_t>(target_length);
    const auto s = static_cast<std::uint64_t>(from);
    const auto t = static_cast<std::uint64_t>(target);
    const std::uint64_t begin = std::max(s * m, t * n);
    const std::uint64_t end = std::min((s + 1U) * m, (t + 1U) * n);
    const std::uint64_t overlap = end > begin ? end - begin : 0U;
    return static_cast<float>(overlap) / static_cast<float>(n);
}

void add_weighted(texel_sum &sum, gltfx_rgba8 texel, float weight) noexcept {
    sum.red += weight * linear_of_stored(texel.red);
    sum.green += weight * linear_of_stored(texel.green);
    sum.blue += weight * linear_of_stored(texel.blue);
    sum.alpha += weight * (static_cast<float>(texel.alpha) / k_byte_max);
}

// Alpha is coverage, not light: it is stored as a plain byte, rounded, and never encoded.
[[nodiscard]] std::uint8_t alpha_byte(float fraction) noexcept {
    return static_cast<std::uint8_t>(
        std::round(std::clamp(fraction * k_byte_max, 0.0F, k_byte_max)));
}

[[nodiscard]] gltfx_rgba8 stored_texel(const texel_sum &sum) noexcept {
    return gltfx_rgba8{
        .red = stored_channel_byte(sum.red),
        .green = stored_channel_byte(sum.green),
        .blue = stored_channel_byte(sum.blue),
        .alpha = alpha_byte(sum.alpha),
    };
}

// The texel of the level below at (x, y): the weighted sum of the source texels it covers.
[[nodiscard]] gltfx_rgba8 average_block(const reduction &job, std::uint32_t x,
                                        std::uint32_t y) noexcept {
    const covered_range columns = covered_texels(x, job.from.width, job.to.width);
    const covered_range rows = covered_texels(y, job.from.height, job.to.height);
    texel_sum sum;
    for (std::uint32_t row = rows.first; row <= rows.last; ++row) {
        const float row_weight = axis_weight(row, y, job.from.height, job.to.height);
        for (std::uint32_t column = columns.first; column <= columns.last; ++column) {
            const float weight = row_weight * axis_weight(column, x, job.from.width, job.to.width);
            add_weighted(sum, job.source[texel_index(job.from, column, row)], weight);
        }
    }
    return stored_texel(sum);
}

// Both sides must be non-empty, and the storage must hold exactly the texels of each level.
[[nodiscard]] bool sizes_agree(std::span<const gltfx_rgba8> source,
                               std::span<gltfx_rgba8> destination, plane from, plane to) noexcept {
    if (from.width == 0 || from.height == 0) {
        return false;
    }
    const auto source_texels = static_cast<std::uint64_t>(from.width) * from.height;
    const auto target_texels = static_cast<std::uint64_t>(to.width) * to.height;
    return source.size() == source_texels && destination.size() == target_texels;
}

} // namespace

std::uint32_t reduced_side(std::uint32_t side, std::uint32_t level) noexcept {
    if (level >= k_shift_bits) {
        return 1U;
    }
    return std::max(1U, side >> level);
}

bool reduce_to_next_level(std::span<const gltfx_rgba8> source, std::uint32_t width,
                          std::uint32_t height, std::span<gltfx_rgba8> destination) noexcept {
    const plane from{.width = width, .height = height};
    const plane to{.width = reduced_side(width, 1U), .height = reduced_side(height, 1U)};
    if (!sizes_agree(source, destination, from, to)) {
        return false;
    }
    const reduction job{.source = source, .from = from, .to = to};
    for (std::uint32_t y = 0; y < to.height; ++y) {
        for (std::uint32_t x = 0; x < to.width; ++x) {
            destination[texel_index(to, x, y)] = average_block(job, x, y);
        }
    }
    return true;
}

// The source texels that destination texel `target` touches, in integers. With n the source length
// and m the target length, source texel s spans [s * m, (s + 1) * m) in scaled units, and
// destination texel t spans [t * n, (t + 1) * n). The two overlap exactly when
// s * m < (t + 1) * n and (s + 1) * m > t * n, which gives the first and last index below.
covered_range covered_texels(std::uint32_t target, std::uint32_t source_length,
                             std::uint32_t target_length) noexcept {
    const auto n = static_cast<std::uint64_t>(source_length);
    const auto m = static_cast<std::uint64_t>(target_length);
    const auto t = static_cast<std::uint64_t>(target);
    const std::uint64_t first = t * n / m;
    const std::uint64_t last = ((t + 1U) * n - 1U) / m;
    return covered_range{.first = static_cast<std::uint32_t>(first),
                         .last = static_cast<std::uint32_t>(last)};
}

} // namespace glintfx::draw2d
