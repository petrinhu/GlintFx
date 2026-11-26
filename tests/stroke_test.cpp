// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <type_traits>

#include <glintfx/draw2d/stroke.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// stroke_test.cpp - R2D-SHAPES, S1-fix1 (docs/auditoria-api-shapes.md, section 2, the fence of
// draw2d/stroke.hpp; GODS_LAWS.md L-17, L-19, L-20, L-26). Proves the PUBLIC DEFAULTS and the
// ENUMERATOR VALUES of the stroke header, as the parecer fixes them. Every expected value below
// is written from the parecer's fence, never read back from the header under test.
//
// Why this file exists: the enumerator values are append-only (stroke.hpp says so), and the
// defaults are a promise to every consumer that writes a stroke without naming its unit, cap or
// alignment. Neither was tested in S1, so a mutant that changed a default from world to pixel, or
// from flat to square, survived. This file is the test that kills those mutants.
//
// Defaults fixed by the parecer: thickness 1.0 world units (gltfx_stroke); unit world; cap flat
// (gltfx_line_style); alignment centered (gltfx_rect_outline_style).
// Enumerator values fixed by the parecer, all of underlying type std::uint8_t:
//   gltfx_stroke_unit:  world = 0, pixel = 1
//   gltfx_line_cap:     flat = 0, square = 1, rounded = 2
//   gltfx_stroke_align: centered = 0, inside = 1, outside = 2

static_assert(std::is_same_v<std::underlying_type_t<glintfx::gltfx_stroke_unit>, std::uint8_t>,
              "gltfx_stroke_unit must stay a std::uint8_t enum (parecer, stroke.hpp fence)");
static_assert(std::is_same_v<std::underlying_type_t<glintfx::gltfx_line_cap>, std::uint8_t>,
              "gltfx_line_cap must stay a std::uint8_t enum (parecer, stroke.hpp fence)");
static_assert(std::is_same_v<std::underlying_type_t<glintfx::gltfx_stroke_align>, std::uint8_t>,
              "gltfx_stroke_align must stay a std::uint8_t enum (parecer, stroke.hpp fence)");

static_assert(std::is_trivially_copyable_v<glintfx::gltfx_stroke>,
              "gltfx_stroke must stay trivially copyable: a value, no owned resource");
static_assert(std::is_trivially_copyable_v<glintfx::gltfx_line_style>,
              "gltfx_line_style must stay trivially copyable: a value, no owned resource");
static_assert(std::is_trivially_copyable_v<glintfx::gltfx_rect_outline_style>,
              "gltfx_rect_outline_style must stay trivially copyable: a value, no owned resource");

GLINTFX_TEST(stroke_defaults_are_one_world_unit_thick_in_world_units) {
    // Value-initialised with {}: the defaults come from the struct's own initialisers, the
    // parecer's thickness 1.0 and unit world.
    const glintfx::gltfx_stroke stroke{};

    GLINTFX_CHECK_EQ(stroke.thickness, 1.0);
    GLINTFX_CHECK(stroke.unit == glintfx::gltfx_stroke_unit::world);
}

GLINTFX_TEST(line_style_defaults_are_the_stroke_default_with_a_flat_cap) {
    const glintfx::gltfx_line_style line{};

    GLINTFX_CHECK_EQ(line.stroke.thickness, 1.0);
    GLINTFX_CHECK(line.stroke.unit == glintfx::gltfx_stroke_unit::world);
    GLINTFX_CHECK(line.cap == glintfx::gltfx_line_cap::flat);
}

GLINTFX_TEST(rect_outline_style_defaults_are_the_stroke_default_with_a_centered_alignment) {
    const glintfx::gltfx_rect_outline_style outline{};

    GLINTFX_CHECK_EQ(outline.stroke.thickness, 1.0);
    GLINTFX_CHECK(outline.stroke.unit == glintfx::gltfx_stroke_unit::world);
    GLINTFX_CHECK(outline.align == glintfx::gltfx_stroke_align::centered);
}

GLINTFX_TEST(stroke_unit_enumerator_values_match_the_parecer) {
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_stroke_unit::world),
                     static_cast<std::uint8_t>(0));
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_stroke_unit::pixel),
                     static_cast<std::uint8_t>(1));
}

GLINTFX_TEST(line_cap_enumerator_values_match_the_parecer) {
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_line_cap::flat),
                     static_cast<std::uint8_t>(0));
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_line_cap::square),
                     static_cast<std::uint8_t>(1));
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_line_cap::rounded),
                     static_cast<std::uint8_t>(2));
}

GLINTFX_TEST(stroke_align_enumerator_values_match_the_parecer) {
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_stroke_align::centered),
                     static_cast<std::uint8_t>(0));
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_stroke_align::inside),
                     static_cast<std::uint8_t>(1));
    GLINTFX_CHECK_EQ(static_cast<std::uint8_t>(glintfx::gltfx_stroke_align::outside),
                     static_cast<std::uint8_t>(2));
}
