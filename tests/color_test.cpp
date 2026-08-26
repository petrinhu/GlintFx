// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>
#include <type_traits>

#include <glintfx/core/color.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// color_test.cpp - CORE-COLOR (TODO.md, GODS_LAWS.md L-19/L-20/L-26):
// proves the frozen layout of glintfx::gltfx_rgba, the round-trip
// conversion with the display's 8-bit format (glintfx::gltfx_rgba8),
// and the premultiply helper.
//
// LAYOUT SLICE (this section): sizeof()/alignof() alone cannot see a
// field reorder or a narrowed field type (version_test.cpp's own
// comment on offsetof() explains why - two mutants against version's
// four uint32_t fields survived a sizeof()-only check on 24/08/2026).
// The per-field offsetof() checks below are that same mutation-
// resistant technique, applied here to gltfx_rgba's four floats.

static_assert(sizeof(glintfx::gltfx_rgba) == 16,
              "glintfx::gltfx_rgba total size must stay 16 bytes, GODS_LAWS.md L-26");

static_assert(offsetof(glintfx::gltfx_rgba, r) == 0,
              "r must stay the first field, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(sizeof(glintfx::gltfx_rgba::r) == sizeof(float),
              "r must stay a 32-bit float, GODS_LAWS.md L-26 (CORE-COLOR)");

static_assert(offsetof(glintfx::gltfx_rgba, g) == 4,
              "g must stay the second field, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(sizeof(glintfx::gltfx_rgba::g) == sizeof(float),
              "g must stay a 32-bit float, GODS_LAWS.md L-26 (CORE-COLOR)");

static_assert(offsetof(glintfx::gltfx_rgba, b) == 8,
              "b must stay the third field, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(sizeof(glintfx::gltfx_rgba::b) == sizeof(float),
              "b must stay a 32-bit float, GODS_LAWS.md L-26 (CORE-COLOR)");

// a is last: no field after it can reveal a gap via offsetof() the
// way the three checks above do for each other - only its own
// sizeof() catches a narrowed type here, exactly the tail-padding
// blind spot version_test.cpp's own comment names for tweak_version.
static_assert(offsetof(glintfx::gltfx_rgba, a) == 12,
              "a must stay the fourth field, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(sizeof(glintfx::gltfx_rgba::a) == sizeof(float),
              "a must stay a 32-bit float, GODS_LAWS.md L-26 (CORE-COLOR)");

static_assert(std::is_trivially_copyable_v<glintfx::gltfx_rgba>,
              "gltfx_rgba must stay trivially copyable, CORE-COLOR (no owned resource)");
static_assert(std::is_standard_layout_v<glintfx::gltfx_rgba>,
              "gltfx_rgba must stay standard layout so offsetof() above is well-defined");

GLINTFX_TEST(gltfx_rgba_aggregate_init_reads_back_the_same_four_fields) {
    constexpr glintfx::gltfx_rgba color{.r = 0.1F, .g = 0.2F, .b = 0.3F, .a = 0.4F};
    GLINTFX_CHECK_EQ(color.r, 0.1F);
    GLINTFX_CHECK_EQ(color.g, 0.2F);
    GLINTFX_CHECK_EQ(color.b, 0.3F);
    GLINTFX_CHECK_EQ(color.a, 0.4F);
}

// SRGB8 ROUND-TRIP SLICE: gltfx_rgba8's own layout, plus decision 6's
// first half (glintfx::gltfx_rgba_from_srgb8()/gltfx_rgba_to_srgb8()).

static_assert(sizeof(glintfx::gltfx_rgba8) == 4,
              "glintfx::gltfx_rgba8 total size must stay 4 bytes, GODS_LAWS.md L-26");

static_assert(offsetof(glintfx::gltfx_rgba8, r) == 0,
              "r must stay the first byte, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(offsetof(glintfx::gltfx_rgba8, g) == 1,
              "g must stay the second byte, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(offsetof(glintfx::gltfx_rgba8, b) == 2,
              "b must stay the third byte, GODS_LAWS.md L-26 (CORE-COLOR)");
static_assert(offsetof(glintfx::gltfx_rgba8, a) == 3,
              "a must stay the fourth byte, GODS_LAWS.md L-26 (CORE-COLOR)");

static_assert(std::is_trivially_copyable_v<glintfx::gltfx_rgba8>,
              "gltfx_rgba8 must stay trivially copyable, CORE-COLOR (no owned resource)");

namespace {

// Distinct formulas per channel, not the same byte repeated four
// times (GODS_LAWS.md L-40: a round trip that feeds every channel the
// SAME value cannot tell a channel-swap mutant - r<->g reordered -
// from a correct implementation; both would round-trip clean). r/g
// move in opposite directions as b climbs, and blue/alpha are offset
// by different constants, so at almost every b at least three of the
// four channels disagree with each other.
glintfx::gltfx_rgba8 pattern_for(int b) {
    return glintfx::gltfx_rgba8{
        .r = static_cast<std::uint8_t>(b),
        .g = static_cast<std::uint8_t>(255 - b),
        .b = static_cast<std::uint8_t>((b + 85) % 256),
        .a = static_cast<std::uint8_t>((b + 170) % 256),
    };
}

} // namespace

GLINTFX_TEST(gltfx_rgba_srgb8_round_trip_covers_every_byte_value) {
    // ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27,
    // "enumere o espaco pequeno quando ele for fechado"): 256 is every
    // possible std::uint8_t, not a hand-picked subset.
    int checked = 0;
    for (int b = 0; b <= 255; ++b) {
        const glintfx::gltfx_rgba8 original = pattern_for(b);
        const glintfx::gltfx_rgba linear = glintfx::gltfx_rgba_from_srgb8(original);
        const glintfx::gltfx_rgba8 back = glintfx::gltfx_rgba_to_srgb8(linear);
        GLINTFX_CHECK_EQ(back.r, original.r);
        GLINTFX_CHECK_EQ(back.g, original.g);
        GLINTFX_CHECK_EQ(back.b, original.b);
        GLINTFX_CHECK_EQ(back.a, original.a);
        ++checked;
    }
    // L-40: the count checked is printed even when everything passes.
    std::println("gltfx_rgba_srgb8_round_trip_covers_every_byte_value: {} byte value(s) checked",
                 checked);
}

// THE decision this whole PMU exists to protect (see color.hpp's own
// header comment, decision 3): #808080 must NOT become the linear
// number 0.5. This is the test that would catch the "quebra sem
// quebrar compilacao" swap from linear to display-encoded, or vice
// versa - GODS_LAWS.md L-19/L-26's table names exactly this class of
// mistake as the one a signature change cannot catch. Reference value
// computed independently in Python at float32 precision (see the
// service order's own worked numbers), not derived from this same
// C++ formula, so this check is not tautological with color.cpp's
// implementation.
GLINTFX_TEST(gltfx_rgba_from_srgb8_is_linear_light_not_display_encoded) {
    const glintfx::gltfx_rgba mid_gray =
        glintfx::gltfx_rgba_from_srgb8(glintfx::gltfx_rgba8{.r = 128, .g = 128, .b = 128, .a = 255});

    constexpr float k_expected_linear = 0.2158605F;
    constexpr float k_naive_identity = 128.0F / 255.0F; // what a WRONG, non-gamma implementation
                                                          // would produce instead
    GLINTFX_CHECK(std::abs(mid_gray.r - k_expected_linear) < 1e-3F);
    GLINTFX_CHECK(std::abs(mid_gray.g - k_expected_linear) < 1e-3F);
    GLINTFX_CHECK(std::abs(mid_gray.b - k_expected_linear) < 1e-3F);
    // Failing this line means the transfer function was silently
    // dropped, not merely imprecise - the two candidates are 0.29
    // apart, far outside any float rounding budget.
    GLINTFX_CHECK(std::abs(mid_gray.r - k_naive_identity) > 0.1F);

    // Alpha bypasses the transfer function (decision 6's own text: it
    // is a coverage fraction, not light) - straight division, exact.
    GLINTFX_CHECK_EQ(mid_gray.a, 1.0F);
}

GLINTFX_TEST(gltfx_rgba_srgb8_round_trip_endpoints_are_exact) {
    const glintfx::gltfx_rgba black =
        glintfx::gltfx_rgba_from_srgb8(glintfx::gltfx_rgba8{.r = 0, .g = 0, .b = 0, .a = 0});
    GLINTFX_CHECK_EQ(black.r, 0.0F);
    GLINTFX_CHECK_EQ(black.g, 0.0F);
    GLINTFX_CHECK_EQ(black.b, 0.0F);
    GLINTFX_CHECK_EQ(black.a, 0.0F);

    const glintfx::gltfx_rgba white = glintfx::gltfx_rgba_from_srgb8(
        glintfx::gltfx_rgba8{.r = 255, .g = 255, .b = 255, .a = 255});
    GLINTFX_CHECK(std::abs(white.r - 1.0F) < 1e-5F);
    GLINTFX_CHECK(std::abs(white.g - 1.0F) < 1e-5F);
    GLINTFX_CHECK(std::abs(white.b - 1.0F) < 1e-5F);
    GLINTFX_CHECK_EQ(white.a, 1.0F);
}

// Decision 1's own reason for existing: light above white and a
// negative (out-of-gamut) component both have to survive
// CONSTRUCTION (nothing amputates them until this ONE lossy step,
// which decision 6 accepts explicitly - see color.hpp's header
// comment). This proves to_srgb8() is where that amputation actually
// happens, clamping instead of wrapping, overflowing, or asserting.
GLINTFX_TEST(gltfx_rgba_to_srgb8_clamps_light_outside_the_displayable_range) {
    constexpr glintfx::gltfx_rgba hdr{.r = 2.0F, .g = -0.5F, .b = 0.5F, .a = 1.5F};
    const glintfx::gltfx_rgba8 encoded = glintfx::gltfx_rgba_to_srgb8(hdr);

    GLINTFX_CHECK_EQ(encoded.r, static_cast<std::uint8_t>(255)); // brighter than white clamps up
    GLINTFX_CHECK_EQ(encoded.g, static_cast<std::uint8_t>(0));   // negative clamps down
    // Reference value computed independently in Python at float32
    // precision: linear 0.5 encodes to byte 188, not the naive 128
    // (0.5 * 255) a non-gamma implementation would produce.
    GLINTFX_CHECK_EQ(encoded.b, static_cast<std::uint8_t>(188));
    GLINTFX_CHECK_EQ(encoded.a, static_cast<std::uint8_t>(255)); // alpha clamps too, no transfer fn
}

// PREMULTIPLY SLICE: decision 6's second half, the helper decision 5's
// own text names. gltfx_rgba_premultiplied() multiplies r/g/b by a and
// leaves a itself unchanged - a straight-line computation (no
// transfer function, no clamp), so exact equality is the right check,
// not a tolerance.

GLINTFX_TEST(gltfx_rgba_premultiplied_scales_color_channels_by_alpha_and_keeps_alpha) {
    constexpr glintfx::gltfx_rgba straight{.r = 0.8F, .g = 0.4F, .b = 0.2F, .a = 0.5F};
    const glintfx::gltfx_rgba premultiplied = glintfx::gltfx_rgba_premultiplied(straight);

    GLINTFX_CHECK_EQ(premultiplied.r, 0.4F);
    GLINTFX_CHECK_EQ(premultiplied.g, 0.2F);
    GLINTFX_CHECK_EQ(premultiplied.b, 0.1F);
    // Straight, decision 5: the helper is a TRANSIENT conversion, not
    // a second stored form - alpha itself never changes.
    GLINTFX_CHECK_EQ(premultiplied.a, 0.5F);
}

GLINTFX_TEST(gltfx_rgba_premultiplied_at_zero_alpha_zeroes_color_channels) {
    constexpr glintfx::gltfx_rgba fully_transparent{.r = 0.9F, .g = 0.6F, .b = 0.3F, .a = 0.0F};
    const glintfx::gltfx_rgba premultiplied = glintfx::gltfx_rgba_premultiplied(fully_transparent);

    GLINTFX_CHECK_EQ(premultiplied.r, 0.0F);
    GLINTFX_CHECK_EQ(premultiplied.g, 0.0F);
    GLINTFX_CHECK_EQ(premultiplied.b, 0.0F);
    GLINTFX_CHECK_EQ(premultiplied.a, 0.0F);
}

GLINTFX_TEST(gltfx_rgba_premultiplied_at_full_alpha_is_identity) {
    constexpr glintfx::gltfx_rgba opaque{.r = 0.7F, .g = 0.3F, .b = 0.9F, .a = 1.0F};
    const glintfx::gltfx_rgba premultiplied = glintfx::gltfx_rgba_premultiplied(opaque);

    GLINTFX_CHECK_EQ(premultiplied.r, opaque.r);
    GLINTFX_CHECK_EQ(premultiplied.g, opaque.g);
    GLINTFX_CHECK_EQ(premultiplied.b, opaque.b);
    GLINTFX_CHECK_EQ(premultiplied.a, opaque.a);
}
