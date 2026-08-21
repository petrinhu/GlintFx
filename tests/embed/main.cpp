// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

// main.cpp - minimal consumer that proves glintfx embedded via
// add_subdirectory (FIX-CONSUMO, achado A7): configure resolves, the
// link works, and the runtime version matches the build macros that
// were generated for this exact embedded build.

int main() {
    const glintfx::version v = glintfx::runtime_version();
    const bool major_ok = v.major_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR);
    const bool minor_ok = v.minor_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR);
    const bool patch_ok = v.patch_version == static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH);
    const bool string_ok =
        glintfx::version_string() == std::string_view{GLINTFX_VERSION_STRING};

    std::println("glintfx {} (embedded)", glintfx::version_string());

    if (!major_ok || !minor_ok || !patch_ok || !string_ok) {
        std::println(stderr, "embed_consumer: runtime version does not match build macros");
        return 1;
    }
    return 0;
}
