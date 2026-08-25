// SPDX-License-Identifier: AGPL-3.0-or-later
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
    const bool major_ok = v.major_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR);
    const bool minor_ok = v.minor_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR);
    const bool patch_ok = v.patch_version == static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH);
    // VER-4C: this line is a TOOTH added alongside the field it checks,
    // not a fix for something that broke - it compares against this
    // same build's own GLINTFX_VERSION_TWEAK macro, so both sides of
    // the comparison always move together. Scope of what this tooth
    // proves, stated precisely (adversarial review, 24/08/2026): it
    // proves PROPAGATION and PACKAGING - that a fourth component
    // exists here at all, reaches this consumer's translation unit,
    // and links. It does NOT prove the number itself is correct; that
    // is tests/version_test.cpp's job (offsetof()/sizeof() per field,
    // catching the two mutants a struct-wide sizeof() alone missed).
    const bool tweak_ok = v.tweak_version == static_cast<std::uint32_t>(GLINTFX_VERSION_TWEAK);
    const bool string_ok = glintfx::version_string() == std::string_view{GLINTFX_VERSION_STRING};

    std::println("glintfx {}", glintfx::version_string());

    if (!major_ok || !minor_ok || !patch_ok || !tweak_ok || !string_ok) {
        std::println(stderr, "consumer: runtime version does not match build macros");
        return 1;
    }
    return 0;
}
