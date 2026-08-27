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
// today for version.hpp - it is a plain data struct with no methods,
// so there is no call-like USE SITE of its field names to exercise
// this way at all.
//
// CLOSED FOR CORE-ERROR (CE-8, 25/08/2026): "that feature's own TDD
// cycle when it lands" from the paragraph above - CORE-ERROR is
// exactly that feature. core_error_use_sites_survive_hostile_system_
// headers() below calls EVERY frozen CORE-ERROR public identifier
// (every gltfx_err accessor/attach method, gltfx_err_code_name(),
// both gltfx_rslt<T> forms, gltfx_err_fields()) as an ACTUAL CALL
// EXPRESSION, not just a declaration the compiler parses - the shape
// that would actually collide with a hostile function-like macro
// (`identifier(` immediately followed by an open paren is what the
// preprocessor pattern-matches on). Same honest caveat as the
// declaration-site case above: this is preventive shielding, checked
// live in this session against the REAL sys/sysmacros.h on Linux (no
// collision found - see the CE-8 commit for the full enumerated
// audit); the Windows leg of CI exercises the same use-site calls
// against the REAL windows.h automatically, since #ifdef _WIN32
// selects it the same way it already does for the declaration-site
// case, but that leg was not run in THIS sandbox (no Windows
// toolchain here).
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
#include <string_view>
#include <utility>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/err_format.hpp>
#include <glintfx/core/version.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>
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

// gltfx_rgba/gltfx_rgba8 are plain aggregates - no call-shaped field
// name for a hostile function-like macro to pattern-match on (the
// same "no live target today" honesty the header comment above
// already states for version.hpp's own plain fields). The two
// conversion functions ARE call-shaped, so this case exercises them
// as real call expressions too, the same CE-8 discipline
// core_error_use_sites_survive_hostile_system_headers below already
// applies to gltfx_err/gltfx_rslt.
GLINTFX_TEST(color_header_survives_hostile_system_headers) {
    constexpr glintfx::gltfx_rgba color{.red = 0.1F, .green = 0.2F, .blue = 0.3F, .alpha = 0.4F};
    GLINTFX_CHECK_EQ(color.red, 0.1F);
    GLINTFX_CHECK_EQ(color.green, 0.2F);
    GLINTFX_CHECK_EQ(color.blue, 0.3F);
    GLINTFX_CHECK_EQ(color.alpha, 0.4F);

    const glintfx::gltfx_rgba8 encoded = glintfx::gltfx_rgba_to_srgb8(color);
    const glintfx::gltfx_rgba decoded = glintfx::gltfx_rgba_from_srgb8(encoded);
    GLINTFX_CHECK(decoded.alpha == color.alpha);
}

// gfss/token.hpp (GFSS-TOKEN) survives the same hostile include order -
// the aggregate fields have no call-shaped name for a function-like
// macro to pattern-match on (the same "no live target today" honesty
// color_header_survives_hostile_system_headers above already states
// for gltfx_rgba/gltfx_rgba8), but gltfx_gfss_token_kind_name() IS
// call-shaped, so this case exercises it as a real call expression,
// the same CE-8 discipline the case below already applies to
// gltfx_err/gltfx_rslt.
GLINTFX_TEST(gfss_token_header_survives_hostile_system_headers) {
    const std::string_view kind_name =
        glintfx::style::gltfx_gfss_token_kind_name(glintfx::style::gltfx_gfss_token_kind::ident);
    GLINTFX_CHECK(kind_name == std::string_view{"ident"});

    constexpr glintfx::style::gltfx_gfss_token token{
        .kind = glintfx::style::gltfx_gfss_token_kind::hash,
        .lexeme = "#a1",
        .line = 1,
        .column = 1,
        .diagnostic = {},
    };
    GLINTFX_CHECK(token.kind == glintfx::style::gltfx_gfss_token_kind::hash);
    GLINTFX_CHECK(token.diagnostic.expected.empty());
}

// gfss/tokenizer.hpp (GFSS-TOKEN) survives the same hostile include
// order - gltfx_gfss_next_token() is the call-shaped, ABI-crossing
// primitive this header declares (see that header's own comment for
// why it is the exported entry point and gltfx_gfss_tokenize() stays
// inline), exercised here the same CE-8 way as gfss_token_header_
// survives_hostile_system_headers above exercises token.hpp's own
// call-shaped name.
GLINTFX_TEST(gfss_tokenizer_header_survives_hostile_system_headers) {
    glintfx::style::gltfx_gfss_cursor cursor{.source = "foo"};
    glintfx::style::gltfx_gfss_token token;
    const bool more = glintfx::style::gltfx_gfss_next_token(cursor, token);
    GLINTFX_CHECK(more);
    GLINTFX_CHECK(token.kind == glintfx::style::gltfx_gfss_token_kind::ident);
    GLINTFX_CHECK(token.diagnostic.expected.empty());
}

// core_error_use_sites_survive_hostile_system_headers - CE-8 finding:
// closes the documented "no live target for USE-SITE collision" gap
// above, for CORE-ERROR specifically. Every frozen public identifier
// from err_code.hpp/err.hpp/err_format.hpp is called here, as an
// actual expression, under the SAME hostile include order the
// declaration-site case above already sets up - if any of them
// collided with a hostile macro (major/minor-style, function-like,
// pattern-matching on `identifier(`), this translation unit would fail
// to COMPILE, not merely produce a wrong runtime value.
GLINTFX_TEST(core_error_use_sites_survive_hostile_system_headers) {
    // gltfx_err_code_name() - CE-1's free function.
    const std::string_view code_name =
        glintfx::gltfx_err_code_name(glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(code_name == std::string_view{"parse_failure"});

    // gltfx_err - CE-2/CE-3's type: construction, every accessor, every
    // with_*() attach method (chained), copy and move.
    glintfx::gltfx_err err(glintfx::gltfx_err_code::parse_failure);
    err.with_path("scene.rcss")
        .with_position(1, 2)
        .with_byte_offset(3)
        .with_rejected_value("x")
        .with_os_error_code(4);
    GLINTFX_CHECK(err.code() == glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(err.path() == std::string_view{"scene.rcss"});
    GLINTFX_CHECK(err.line() == 1);
    GLINTFX_CHECK(err.column() == 2);
    GLINTFX_CHECK(err.byte_offset() == 3);
    GLINTFX_CHECK(err.rejected_value() == std::string_view{"x"});
    GLINTFX_CHECK(err.os_error_code() == 4);

    const glintfx::gltfx_err copied(err);
    const glintfx::gltfx_err moved(std::move(err));
    GLINTFX_CHECK(copied.code() == glintfx::gltfx_err_code::parse_failure);
    GLINTFX_CHECK(moved.code() == glintfx::gltfx_err_code::parse_failure);

    // gltfx_rslt<T> - CE-4's envelope: both the primary template and
    // the T = void specialization, both factory forms.
    const glintfx::gltfx_rslt<int> ok_result = glintfx::gltfx_rslt<int>::ok(7);
    GLINTFX_CHECK(ok_result.has_value());
    GLINTFX_CHECK(ok_result.value() == 7);

    const glintfx::gltfx_rslt<int> err_result =
        glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::not_found));
    GLINTFX_CHECK(err_result.has_error());
    GLINTFX_CHECK(err_result.error().code() == glintfx::gltfx_err_code::not_found);

    const glintfx::gltfx_rslt<void> ok_void = glintfx::gltfx_rslt<void>::ok();
    GLINTFX_CHECK(ok_void.has_value());

    const glintfx::gltfx_rslt<void> err_void =
        glintfx::gltfx_rslt<void>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::unsupported));
    GLINTFX_CHECK(err_void.has_error());

    // gltfx_err_fields() - CE-5's formatter, plus gltfx_err_field's own
    // two members (name, value). GLINTFX_CHECK is NON-FATAL (see
    // harness/check.hpp: it records the failure and the case keeps
    // running) - cppcheck's containerOutOfBounds caught the real
    // consequence: indexing fields[0] unconditionally right after a
    // failed !fields.empty() check would still execute, out of
    // bounds. Guarding the index behind the same condition, not
    // suppressing the finding, is the fix.
    const auto fields = glintfx::gltfx_err_fields(copied);
    GLINTFX_CHECK(!fields.empty());
    if (!fields.empty()) {
        GLINTFX_CHECK(fields[0].name == std::string_view{"code"});
        GLINTFX_CHECK(fields[0].value == "parse_failure");
    }
}
