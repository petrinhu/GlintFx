// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>

#include <glintfx/draw2d/texture_2d_desc.hpp>

#include "draw2d/texture_sampling.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// texture_sampling_test.cpp - R2D-TEXTURE, fatia T2b: the translation of a texture's filter and
// number of reduced levels into the four GL texture parameters (draw2d/texture_sampling.hpp), as a
// CLOSED table: every filter value against "no reduced level", "one" and "many". No GL, no window.
// The expected enumerators are literals of third_party/khronos/gl.xml, never the constants of the
// code under test (the T2b order, item 3).

using glintfx::gltfx_texture_filter;
using glintfx::draw2d::sampling_parameters_of;
using glintfx::draw2d::texture_sampling_parameters;
using glintfx::render::GLint;

namespace {

// gl.xml
constexpr GLint k_nearest = 0x2600;
constexpr GLint k_linear = 0x2601;
constexpr GLint k_nearest_mipmap_nearest = 0x2700;
constexpr GLint k_linear_mipmap_linear = 0x2703;
constexpr GLint k_clamp_to_edge = 0x812F;

struct row {
    const char *name = nullptr;
    gltfx_texture_filter filter = gltfx_texture_filter::nearest;
    std::uint32_t reduced_levels = 0U;
    GLint magnification = 0;
    GLint minification = 0;
};

[[nodiscard]] gltfx_texture_filter unknown_filter() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    return static_cast<gltfx_texture_filter>(7);
}

[[nodiscard]] bool same(const std::optional<texture_sampling_parameters> &got, const row &want) {
    return got.has_value() && got->magnification == want.magnification &&
           got->minification == want.minification && got->wrap_s == k_clamp_to_edge &&
           got->wrap_t == k_clamp_to_edge;
}

} // namespace

GLINTFX_TEST(texture_sampling_closed_table_of_filter_and_reduced_levels) {
    // 12 is the most reduced levels a side of 4096 allows (floor(log2(4096))).
    const std::array<row, 6> rows{{
        {"nearest, 0 niveis reduzidos", gltfx_texture_filter::nearest, 0U, k_nearest, k_nearest},
        {"linear, 0 niveis reduzidos", gltfx_texture_filter::linear, 0U, k_linear, k_linear},
        {"nearest, 1 nivel reduzido", gltfx_texture_filter::nearest, 1U, k_nearest,
         k_nearest_mipmap_nearest},
        {"linear, 1 nivel reduzido", gltfx_texture_filter::linear, 1U, k_linear,
         k_linear_mipmap_linear},
        {"nearest, 12 niveis reduzidos", gltfx_texture_filter::nearest, 12U, k_nearest,
         k_nearest_mipmap_nearest},
        {"linear, 12 niveis reduzidos", gltfx_texture_filter::linear, 12U, k_linear,
         k_linear_mipmap_linear},
    }};
    std::size_t counted = 0;
    std::size_t failed = 0;
    for (const row &want : rows) {
        ++counted;
        if (!same(sampling_parameters_of(want.filter, want.reduced_levels), want)) {
            ++failed;
            std::println("texture_sampling_test: FALHOU o caso {}", want.name);
        }
    }
    std::println("texture_sampling_test: casos={} falhas={}", counted, failed);
    GLINTFX_CHECK_EQ(counted, std::size_t{6});
    GLINTFX_CHECK_EQ(failed, std::size_t{0});
}

GLINTFX_TEST(texture_sampling_an_unknown_filter_answers_nothing) {
    GLINTFX_CHECK(!sampling_parameters_of(unknown_filter(), 0U).has_value());
    GLINTFX_CHECK(!sampling_parameters_of(unknown_filter(), 3U).has_value());
}
