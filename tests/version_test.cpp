// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <string>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// version_test.cpp - proves that linking against glintfx::glintfx works
// in both modes (shared and static) and that the GLINTFX_API macro
// export does not break the symbol (FUND-1, item 7 of the service
// order).
//
// Four-component version (VER-4C, GODS_LAWS.md L-26): the `version`
// struct carries a fourth field, tweak_version, appended after the
// three that already existed, without reordering them.
//
// CORRECTION (adversarial review of VER-4C, 24/08/2026): the original
// version of this comment claimed the layout was "packed, no padding".
// That was false, and the falseness is exactly what let two mutants
// survive against a sizeof()-only guard:
//   - mutant 1 (swap patch_version and tweak_version in declaration
//     order): the total byte count is unchanged, so sizeof() == 16
//     alone cannot see the swap.
//   - mutant 2 (narrow tweak_version from uint32_t to uint16_t): this
//     struct DOES carry 2 bytes of TAIL PADDING after a uint16_t last
//     member, measured with offsetof()/alignof() - the compiler closes
//     the gap to keep the struct's own alignment, so sizeof() stays 16
//     either way. sizeof() alone cannot see this either.
// Both survive a sizeof()-only check because sizeof() proves TOTAL
// SIZE, not LAYOUT. What actually pins order and per-field width is
// offsetof() for position plus sizeof() of the field itself for
// width, one pair per field, below.
static_assert(sizeof(glintfx::version) == 16,
              "glintfx::version total size must stay 16 bytes, GODS_LAWS.md L-26");

static_assert(offsetof(glintfx::version, major_version) == 0,
              "major_version must stay the first field, GODS_LAWS.md L-26");
static_assert(sizeof(glintfx::version::major_version) == sizeof(std::uint32_t),
              "major_version must stay a uint32_t, GODS_LAWS.md L-26");

static_assert(offsetof(glintfx::version, minor_version) == 4,
              "minor_version must stay the second field, GODS_LAWS.md L-26");
static_assert(sizeof(glintfx::version::minor_version) == sizeof(std::uint32_t),
              "minor_version must stay a uint32_t, GODS_LAWS.md L-26");

static_assert(offsetof(glintfx::version, patch_version) == 8,
              "patch_version must stay the third field, GODS_LAWS.md L-26");
static_assert(sizeof(glintfx::version::patch_version) == sizeof(std::uint32_t),
              "patch_version must stay a uint32_t, GODS_LAWS.md L-26");

// tweak_version is the field mutant 2 targeted: it is LAST, so no
// field after it can reveal a gap via offsetof() the way the three
// checks above do for each other - only this field's own sizeof()
// catches a narrowed type here, because the tail-padding trick that
// hid the mutation from the struct-wide sizeof() only works when
// nothing measures the field directly.
static_assert(offsetof(glintfx::version, tweak_version) == 12,
              "tweak_version must stay the fourth field, GODS_LAWS.md L-26");
static_assert(sizeof(glintfx::version::tweak_version) == sizeof(std::uint32_t),
              "tweak_version must stay a uint32_t, GODS_LAWS.md L-26");

GLINTFX_TEST(runtime_version_matches_macros) {
    const glintfx::version v = glintfx::runtime_version();
    GLINTFX_CHECK_EQ(v.major_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR));
    GLINTFX_CHECK_EQ(v.minor_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR));
    GLINTFX_CHECK_EQ(v.patch_version, static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH));
    GLINTFX_CHECK_EQ(v.tweak_version, static_cast<std::uint32_t>(GLINTFX_VERSION_TWEAK));
}

GLINTFX_TEST(version_string_matches_macro_and_format) {
    const std::string from_macro = GLINTFX_VERSION_STRING;
    const std::string formatted_from_parts =
        std::to_string(GLINTFX_VERSION_MAJOR) + "." + std::to_string(GLINTFX_VERSION_MINOR) + "." +
        std::to_string(GLINTFX_VERSION_PATCH) + "." + std::to_string(GLINTFX_VERSION_TWEAK);

    const std::string runtime = std::string(glintfx::version_string());
    GLINTFX_CHECK_EQ(runtime, from_macro);
    GLINTFX_CHECK_EQ(runtime, formatted_from_parts);
}
