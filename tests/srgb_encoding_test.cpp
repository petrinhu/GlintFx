// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>
#include <print>

#include <glintfx/core/color.hpp>

#include "draw2d/srgb_encoding.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// srgb_encoding_test.cpp - R2D-BATCH, fatia B4: the sRGB encoding of the clear color. The reference
// points are the ones of the standard curve (IEC 61966-2-1), written as literals here, never
// through the code's own constants: 0 -> 0, 1 -> 1, the toe at 0.0031308 -> 0.040449936, mid grey
// 0.5 -> 0.735356983, and the value that encodes to exactly half, 0.21404114 -> 0.5.

using glintfx::gltfx_rgba;
using glintfx::draw2d::srgb_encode;

namespace {
[[nodiscard]] bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 1.0e-4F;
}
} // namespace

GLINTFX_TEST(srgb_encode_matches_the_reference_points_of_the_standard_curve) {
    GLINTFX_CHECK(near(srgb_encode(0.0F), 0.0F));
    GLINTFX_CHECK(near(srgb_encode(1.0F), 1.0F));
    GLINTFX_CHECK(near(srgb_encode(0.0031308F), 0.040449936F)); // the toe: both branches meet
    GLINTFX_CHECK(near(srgb_encode(0.5F), 0.735356983F));
    GLINTFX_CHECK(near(srgb_encode(0.21404114F), 0.5F));
    GLINTFX_CHECK(near(srgb_encode(0.001F), 0.01292F)); // below the toe: 12.92 * x
}

GLINTFX_TEST(srgb_encode_is_monotonic_and_handles_the_edges) {
    float previous = -1.0F;
    for (int i = 0; i <= 100; ++i) {
        const float encoded = srgb_encode(static_cast<float>(i) / 100.0F);
        GLINTFX_CHECK(encoded >= previous);
        previous = encoded;
    }
    GLINTFX_CHECK(srgb_encode(-0.5F) == 0.0F); // below zero encodes as zero
    GLINTFX_CHECK(srgb_encode(2.0F) > 1.0F);   // above one stays above one: the surface clips it
}

GLINTFX_TEST(srgb_encode_of_a_color_leaves_the_alpha_alone) {
    const gltfx_rgba encoded = srgb_encode(gltfx_rgba{0.5F, 0.0F, 1.0F, 0.25F});
    GLINTFX_CHECK(near(encoded.red, 0.735356983F));
    GLINTFX_CHECK(near(encoded.green, 0.0F));
    GLINTFX_CHECK(near(encoded.blue, 1.0F));
    GLINTFX_CHECK(encoded.alpha == 0.25F); // coverage, not light
    std::println("srgb_encoding_test: pontos de referencia da curva conferidos");
}
