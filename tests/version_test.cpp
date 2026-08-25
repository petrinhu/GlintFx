// SPDX-License-Identifier: AGPL-3.0-or-later
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
// struct carries a fourth field, tweak_version, with no padding added
// by the compiler - four uint32_t lay out to exactly 16 bytes on every
// target this library ships for. That is proven here, not assumed.
static_assert(sizeof(glintfx::version) == 16,
              "glintfx::version must stay four packed uint32_t fields, GODS_LAWS.md L-26");

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
