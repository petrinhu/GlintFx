#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

// main.cpp - minimal consumer that proves the installed glintfx
// package: find_package resolves, the link works, and the runtime
// version matches the build macros that generated the installed
// package.

int main() {
    const glintfx::version v = glintfx::runtime_version();
    const bool major_ok = v.major == static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR);
    const bool minor_ok = v.minor == static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR);
    const bool patch_ok = v.patch == static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH);
    const bool string_ok =
        glintfx::version_string() == std::string_view{GLINTFX_VERSION_STRING};

    std::println("glintfx {}", glintfx::version_string());

    if (!major_ok || !minor_ok || !patch_ok || !string_ok) {
        std::println(stderr, "consumer: runtime version does not match build macros");
        return 1;
    }
    return 0;
}
