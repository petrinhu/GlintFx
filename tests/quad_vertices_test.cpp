// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <limits>
#include <print>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/quad_vertices.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// quad_vertices_test.cpp - R2D-BATCH, fatia B2a (docs/plano-w7d.md sec. 4.3/4.4, /var/tmp/cto-w7d/
// PLANO-errata.md sec. 3 B2a, docs/auditoria-api-draw2d.md B0-C1): the pure atom that turns the
// four corners of a piece from WORLD (double) into PIXEL (float) through ONE transform.
//
// THE DECISION THIS TEST EXISTS TO PROVE (the leader's precision decision): the transform is
// applied to world positions that were NEVER narrowed, in DOUBLE, and only the finished pixel
// positions become single precision. Two ways to break it, and one cell each:
//   - narrowing the corners BEFORE transforming (a piece at x = 16 777 217 cannot be held by a
//     float, so it lands on the wrong pixel);
//   - narrowing the CAMERA (the transform's translation) before subtracting it (B0-C1: a camera at
//     x = 16 777 217 cannot be held by a float either, and the earlier cell would not have seen
//     it, because -16 777 216 = -2^24 IS representable).
// A "camera at X" here is the transform whose translation is -X: pixel = world + translation.
//
// RED, SEEN: before quad_vertices.{hpp,cpp} existed, this file's own #include line failed to
// compile; then, against a body that answered zeros, every cell below failed.

using glintfx::gltfx_angle;
using glintfx::gltfx_transform;
using glintfx::gltfx_vec2_world;
using glintfx::draw2d::pixel_affine_from_transform;
using glintfx::draw2d::quad_corners_pixel;
using glintfx::draw2d::quad_corners_world;
using glintfx::draw2d::quad_vertices;

namespace {
constexpr double k_pi = 3.14159265358979323846;

[[nodiscard]] gltfx_transform make(double tx, double ty, double radians, double sx, double sy) {
    return gltfx_transform{
        .translation = {tx, ty}, .rotation = gltfx_angle{radians}, .scale = {sx, sy}};
}
constexpr gltfx_transform k_identity = {
    .translation = {0.0, 0.0}, .rotation = {0.0}, .scale = {1.0, 1.0}};

// A quad whose four corners are all the same point (so each cell asks one question).
[[nodiscard]] quad_corners_world at(double x, double y) {
    const gltfx_vec2_world p{x, y};
    return quad_corners_world{p, p, p, p};
}

[[nodiscard]] quad_corners_pixel through(const quad_corners_world &corners,
                                         const gltfx_transform &transform) {
    return quad_vertices(corners, pixel_affine_from_transform(transform));
}

[[nodiscard]] bool near(float value, float expected, float tolerance = 1e-5F) {
    return std::fabs(value - expected) <= tolerance;
}
} // namespace

GLINTFX_TEST(quad_vertices_precision_and_transform_cells) {
    int analyzed = 0;

    // 1. THE DECISION: a piece at x = 16 777 217 (2^24 + 1, which a float cannot hold) with the
    //    camera at 16 777 216 (translation -16 777 216) lands on pixel 1.0 EXACTLY. Narrowing the
    //    piece before transforming gives 0.0.
    {
        const auto out = through(at(16777217.0, 0.0), make(-16777216.0, 0.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK_EQ(out[0].x, 1.0F);
        ++analyzed;
    }
    // 2. B0-C1: the CAMERA at 16 777 217 (a float cannot hold it) and the piece at 16 777 218:
    //    pixel 1.0. Narrowing the camera first gives 2.0.
    {
        const auto out = through(at(16777218.0, 0.0), make(-16777217.0, 0.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK_EQ(out[0].x, 1.0F);
        ++analyzed;
    }
    // 3. The control of the old cell: a camera a float CAN hold (-2^24) stays exact in both
    //    directions of the check.
    {
        const auto out = through(at(16777218.0, 0.0), make(-16777216.0, 0.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK(out[0].x == 2.0F);
        const auto zero = through(at(16777216.0, 0.0), make(-16777216.0, 0.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK(zero[0].x == 0.0F);
        ++analyzed;
    }
    // 4. Far from the origin with a fraction: 1e9 + 0.25 (a float's step there is 64) with the
    //    camera at 1e9 lands on 0.25 exactly, on both axes.
    {
        const auto out =
            through(at(1.0e9 + 0.25, -1.0e9 + 0.25), make(-1.0e9, 1.0e9, 0.0, 1.0, 1.0));
        GLINTFX_CHECK(out[0].x == 0.25F);
        GLINTFX_CHECK(out[0].y == 0.25F);
        ++analyzed;
    }
    // 5. No transform (the identity): the world position IS the pixel, only narrowed.
    {
        const auto out = through(at(10.5, 20.25), k_identity);
        GLINTFX_CHECK(out[0].x == 10.5F && out[0].y == 20.25F);
        ++analyzed;
    }
    // 6. Scale, per axis.
    {
        const auto out = through(at(1.0, 1.0), make(0.0, 0.0, 0.0, 2.0, 3.0));
        GLINTFX_CHECK(out[0].x == 2.0F && out[0].y == 3.0F);
        ++analyzed;
    }
    // 7. Rotation of a quarter turn takes (1, 0) to (0, 1) (the same convention as
    //    gltfx_mat3_from_transform: x' = cos*x - sin*y, y' = sin*x + cos*y).
    {
        const auto out = through(at(1.0, 0.0), make(0.0, 0.0, k_pi / 2.0, 1.0, 1.0));
        GLINTFX_CHECK(near(out[0].x, 0.0F) && near(out[0].y, 1.0F));
        ++analyzed;
    }
    // 8. THE ORDER, scale then rotation then translation (core/transform.hpp): (1, 0) scaled by
    //    (2, 1) is (2, 0), turned a quarter is (0, 2), moved by (10, 20) is (10, 22). Any other
    //    order gives another point.
    {
        const auto out = through(at(1.0, 0.0), make(10.0, 20.0, k_pi / 2.0, 2.0, 1.0));
        GLINTFX_CHECK(near(out[0].x, 10.0F) && near(out[0].y, 22.0F));
        ++analyzed;
    }
    // 9. The four corners keep their ROLES, in order: top_left, top_right, bottom_right,
    //    bottom_left. The unit square at the origin, moved by (5, 5).
    {
        const quad_corners_world square{gltfx_vec2_world{0.0, 0.0}, gltfx_vec2_world{1.0, 0.0},
                                        gltfx_vec2_world{1.0, 1.0}, gltfx_vec2_world{0.0, 1.0}};
        const auto out = through(square, make(5.0, 5.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK(out[0].x == 5.0F && out[0].y == 5.0F);
        GLINTFX_CHECK(out[1].x == 6.0F && out[1].y == 5.0F);
        GLINTFX_CHECK(out[2].x == 6.0F && out[2].y == 6.0F);
        GLINTFX_CHECK(out[3].x == 5.0F && out[3].y == 6.0F);
        ++analyzed;
    }
    // 10. A degenerate quad (zero area: all corners the same) is transformed like any other: four
    //     equal vertices, no refusal here (the refusal by value belongs to the frame report).
    {
        const auto out = through(at(3.0, 4.0), make(1.0, 1.0, 0.0, 1.0, 1.0));
        GLINTFX_CHECK(out[0].x == 4.0F && out[3].y == 5.0F);
        ++analyzed;
    }
    // 11. A value that is not finite passes THROUGH as not finite (the atom never hides it; the
    //     frame report is what counts and refuses it), and nothing else in the quad is spoiled.
    {
        const quad_corners_world quad{
            gltfx_vec2_world{std::numeric_limits<double>::quiet_NaN(), 0.0},
            gltfx_vec2_world{2.0, 0.0}, gltfx_vec2_world{2.0, 2.0}, gltfx_vec2_world{0.0, 2.0}};
        const auto out = through(quad, k_identity);
        GLINTFX_CHECK(std::isnan(out[0].x));
        GLINTFX_CHECK(out[1].x == 2.0F && out[2].y == 2.0F && out[3].y == 2.0F);
        ++analyzed;
    }
    // 12. The prepared affine of the identity is exactly the identity, and preparing it once and
    //     using it twice gives the same answer both times (a batch prepares it ONCE).
    {
        const auto affine = pixel_affine_from_transform(k_identity);
        GLINTFX_CHECK(affine.column0_x == 1.0 && affine.column0_y == 0.0);
        GLINTFX_CHECK(affine.column1_x == 0.0 && affine.column1_y == 1.0);
        GLINTFX_CHECK(affine.translation_x == 0.0 && affine.translation_y == 0.0);
        const auto first = quad_vertices(at(7.0, 8.0), affine);
        const auto second = quad_vertices(at(7.0, 8.0), affine);
        GLINTFX_CHECK(first[0].x == second[0].x && first[0].y == second[0].y);
        ++analyzed;
    }

    // 13. The y coordinate takes part in the rotation: (0, 1) turned a quarter goes to (-1, 0)
    //     (the second column of the matrix, -sin on x). A cell that only rotates (1, 0) never
    //     reads the second column.
    {
        const auto out = through(at(0.0, 1.0), make(0.0, 0.0, k_pi / 2.0, 1.0, 1.0));
        GLINTFX_CHECK(near(out[0].x, -1.0F) && near(out[0].y, 0.0F));
        ++analyzed;
    }
    // 14. The rotation itself is computed in DOUBLE. A point at distance 1e6 sitting exactly on
    //     the rotated axis (x = 1e6 * sin(0.5), y = 1e6 * cos(0.5)) turned by 0.5 rad lands at
    //     x = 0 and y = 1e6. With the cosine and sine narrowed to float (an error of about 3e-8 on
    //     each) the same point misses x = 0 by about 1e-2 pixels: this cell separates the two.
    {
        constexpr double k_radians = 0.5;
        const auto out = through(at(1.0e6 * std::sin(k_radians), 1.0e6 * std::cos(k_radians)),
                                 make(0.0, 0.0, k_radians, 1.0, 1.0));
        GLINTFX_CHECK(std::fabs(out[0].x) < 1.0e-6F);
        GLINTFX_CHECK(near(out[0].y, 1.0e6F, 1.0e-1F));
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 14);
    std::println("quad_vertices_test: {} celula(s) conferida(s)", analyzed);
}
