// SPDX-License-Identifier: AGPL-3.0-or-later
//
// header_hygiene_test.cpp - proves glintfx/core/version.hpp still
// compiles and behaves correctly after aggressive, real-world system
// headers are included first (HDR-HYGIENE-FIX, reproving HDR-HYGIENE,
// which promised this guard and never delivered it).
//
// WHAT THIS TU ACTUALLY GUARANTEES (measured, HDR-HYGIENE-FIX-2):
// compilability AT THE HEADER'S DECLARATION SITE, of the whole public
// header, after the kind of hostile system header a real consumer's
// translation unit is free to include first. That property is real
// and durable: it is what would catch, for example, a future method
// literally named min()/major() on a public type colliding with
// <windows.h> or <sys/sysmacros.h> the moment the header is parsed
// here - before any real consumer ever hits it.
//
// WHAT IT DOES NOT COVER, stated honestly because a mutant proved it
// (adversarial review, 24/08/2026, mutant M3): the hostile include
// order below is preventive shielding with NO LIVE TARGET TODAY.
// Nothing in version.hpp calls `major`/`minor` in the call-like form
// that would actually collide with sys/sysmacros.h's function-like
// macros (`#define major(dev) gnu_dev_major (dev)`, only expands when
// immediately followed by `(`) - the struct's fields were renamed to
// major_version/minor_version/patch_version specifically to avoid
// that collision (HDR-HYGIENE, GODS_LAWS.md L-19 PMU), so removing
// <sys/types.h>/<sys/sysmacros.h> from above changes NOTHING here
// today. This TU does not prove collision-at-USE-SITE (a future
// method actually named major()/min() being called), and it does not
// prove that a NEW symbol added inside a header this TU already
// covers gets exercised (mutant M4a). Both are the responsibility of
// that feature's own TDD cycle when it lands (GODS_LAWS.md L-20), not
// of this guard.
//
// COVERAGE CONTRACT, now MECHANICAL, not text: every *.hpp committed
// under include/glintfx/ has to appear as a literal `#include` line
// below. This is enforced by
// tests/tools/check_hygiene_coverage.sh (ctest case
// hygiene_coverage_test), not by a human reading this comment - the
// previous version of this comment claimed to be a checklist for
// future reviews and nothing ever applied it (HDR-HYGIENE-FIX-2
// finding). Generated headers (export.hpp, version_macros.hpp - see
// cmake/GlintfxLibrary.cmake) are NOT under include/glintfx/ in the
// source tree, at configure/build time they are written under the
// build directory's generated include path instead, so that portal
// does not and cannot enumerate them; they are covered by
// transitivity (glintfx never ships without generating them, and
// version_macros.hpp is already included below).
//
// DELIBERATE DUPLICATION: the four checks in the test case below
// mirror assertions already made in tests/version_test.cpp (the three
// field checks come from runtime_version_matches_macros, the string
// check from version_string_matches_macro_and_format). That is
// intentional, not an oversight - a prior report claimed this TU
// never replicated those assertions; this comment exists so the next
// reviewer does not reopen that question by reading the diff instead
// of the code. The point of the duplication is RE-VERIFICATION UNDER
// HOSTILE CONTEXT: the same values have to still be correct after the
// hostile headers above, not just when version.hpp is compiled clean.

#ifdef __linux__
#include <sys/sysmacros.h>
#include <sys/types.h>
#endif

#ifdef _WIN32
// Deliberately NOT defining NOMINMAX or WIN32_LEAN_AND_MEAN first:
// <windows.h>'s min/max are function-like macros, the same class of
// collision as major/minor on Linux (GODS_LAWS.md L-04, Windows leg).
#include <windows.h>
#endif

#include <cstdint>
#include <string>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/err_format.hpp>
#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// See "DELIBERATE DUPLICATION" above: these four checks intentionally
// re-run tests/version_test.cpp's assertions under the hostile include
// order set up above.
GLINTFX_TEST(version_header_survives_hostile_system_headers) {
    const glintfx::version v = glintfx::runtime_version();
    GLINTFX_CHECK_EQ(v.major_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR));
    GLINTFX_CHECK_EQ(v.minor_version, static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR));
    GLINTFX_CHECK_EQ(v.patch_version, static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH));
    GLINTFX_CHECK_EQ(v.tweak_version, static_cast<std::uint32_t>(GLINTFX_VERSION_TWEAK));

    const std::string runtime = std::string(glintfx::version_string());
    GLINTFX_CHECK_EQ(runtime, std::string(GLINTFX_VERSION_STRING));
}
