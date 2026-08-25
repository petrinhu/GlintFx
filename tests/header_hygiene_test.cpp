// SPDX-License-Identifier: AGPL-3.0-or-later
//
// header_hygiene_test.cpp - proves glintfx/core/version.hpp still
// compiles and behaves correctly after aggressive, real-world system
// headers are included first (HDR-HYGIENE-FIX, reproving HDR-HYGIENE,
// which promised this guard and never delivered it).
//
// CONTRACT: every new public header enters this translation unit,
// included after the platform block below. This is a checklist item
// for future public API reviews (GODS_LAWS.md L-19).
//
// This TU does NOT assert against the two sysmacros.h macros by name.
// `major`/`minor` on Linux are function-like (`#define major(dev) ...`)
// and only expand when the identifier is immediately followed by `(`;
// a struct member never produces that shape, so a test built around
// those two macros specifically would never turn red (an adversarial
// review reproved the original HDR-HYGIENE item for exactly this).
// What this TU guards is broader and durable: the whole public header
// still compiles and works after the kind of hostile system header a
// real consumer's translation unit is free to include first.

#ifdef __linux__
#include <sys/types.h>
#include <sys/sysmacros.h>
#endif

#ifdef _WIN32
// Deliberately NOT defining NOMINMAX or WIN32_LEAN_AND_MEAN first:
// <windows.h>'s min/max are function-like macros, the same class of
// collision as major/minor on Linux (GODS_LAWS.md L-04, Windows leg).
#include <windows.h>
#endif

#include <cstdint>
#include <string>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

GLINTFX_TEST(version_header_survives_hostile_system_headers) {
    const glintfx::version v = glintfx::runtime_version();
    GLINTFX_CHECK_EQ(v.major_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR));
    GLINTFX_CHECK_EQ(v.minor_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR));
    GLINTFX_CHECK_EQ(v.patch_version, static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH));

    const std::string runtime = std::string(glintfx::version_string());
    GLINTFX_CHECK_EQ(runtime, std::string(GLINTFX_VERSION_STRING));
}
