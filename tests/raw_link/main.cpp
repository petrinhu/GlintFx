// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

// main.cpp - minimal consumer with NO CMakeLists.txt of its own
// (FIX-CONSUMO-3, achado QA-2-2): compiled and linked directly by
// check_output_name.sh with a bare `-lglintfx`, no find_package, no
// CMake target at all on the consumer side. This is the scenario
// pkg-config, a hand-written Makefile or a distro's %files/.spec/
// PKGBUILD entry actually exercises, and none of the CMake-target-based
// gates (consume_test, embed_test) touch it: they all resolve the
// library through the glintfx::glintfx imported target, never through
// the raw linker flag.

int main() {
    const glintfx::version v = glintfx::runtime_version();
    const bool major_ok = v.major_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR);
    const bool minor_ok = v.minor_version == static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR);
    const bool patch_ok = v.patch_version == static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH);
    // VER-4C: this line is a TOOTH added alongside the field it checks,
    // not a fix for something that broke - it compares against this
    // same build's own GLINTFX_VERSION_TWEAK macro, so both sides of
    // the comparison always move together.
    const bool tweak_ok = v.tweak_version == static_cast<std::uint32_t>(GLINTFX_VERSION_TWEAK);
    const bool string_ok = glintfx::version_string() == std::string_view{GLINTFX_VERSION_STRING};

    std::println("glintfx {} (raw -lglintfx link)", glintfx::version_string());

    if (!major_ok || !minor_ok || !patch_ok || !tweak_ok || !string_ok) {
        std::println(stderr, "raw_link consumer: runtime version does not match build macros");
        return 1;
    }
    return 0;
}
