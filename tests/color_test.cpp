// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
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
