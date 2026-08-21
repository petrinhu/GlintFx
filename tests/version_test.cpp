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

GLINTFX_TEST(runtime_version_matches_macros) {
    const glintfx::version v = glintfx::runtime_version();
    GLINTFX_CHECK_EQ(v.major, static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR));
    GLINTFX_CHECK_EQ(v.minor, static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR));
    GLINTFX_CHECK_EQ(v.patch, static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH));
}

GLINTFX_TEST(version_string_matches_macro_and_format) {
    const std::string from_macro = GLINTFX_VERSION_STRING;
    const std::string formatted_from_parts =
        std::to_string(GLINTFX_VERSION_MAJOR) + "." +
        std::to_string(GLINTFX_VERSION_MINOR) + "." +
        std::to_string(GLINTFX_VERSION_PATCH);

    const std::string runtime = std::string(glintfx::version_string());
    GLINTFX_CHECK_EQ(runtime, from_macro);
    GLINTFX_CHECK_EQ(runtime, formatted_from_parts);
}
