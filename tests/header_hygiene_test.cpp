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

// WL-WINDOW-HANDLE: the real <windows.h> above only ever runs on the
// one Windows leg - this shim carries the SPECIFIC subset of its own
// hostile macros (near/far) onto the other four platforms too, so the
// SAME collision class is checked everywhere (GODS_LAWS.md L-04). A
// no-op on an actual Windows build (see the shim's own header comment
// for why) - never a second, possibly-drifted definition alongside the
// real <windows.h> above.
#include "hostile_win32_macros_shim.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/err_format.hpp>
#include <glintfx/core/fixed_step.hpp>
#include <glintfx/core/interpolate.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/mat3.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/time.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/core/version.hpp>
#include <glintfx/gfss/keyword.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>
#include <glintfx/gfss/value.hpp>
#include <glintfx/gfui/node_view.hpp>
#include <glintfx/platform/asset/file.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>
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

// core/time.hpp (CORE-TIME) survives the same hostile include order -
// the two aggregate fields have no call-shaped name for a hostile
// function-like macro to pattern-match on (the same "no live target
// today" honesty color_header_survives_hostile_system_headers above
// already states for gltfx_rgba/gltfx_rgba8), but all three
// gltfx_duration_* free functions ARE call-shaped, so this case
// exercises every one of them as a real call expression, the same
// CE-8 discipline core_error_use_sites_survive_hostile_system_headers
// below already applies to gltfx_err/gltfx_rslt.
GLINTFX_TEST(time_header_survives_hostile_system_headers) {
    constexpr glintfx::gltfx_time_point earlier{.ticks = 5};
    constexpr glintfx::gltfx_time_point later{.ticks = 20};
    GLINTFX_CHECK_EQ(earlier.ticks, static_cast<std::int64_t>(5));
    GLINTFX_CHECK_EQ(later.ticks, static_cast<std::int64_t>(20));

    const glintfx::gltfx_duration elapsed = glintfx::gltfx_duration_between(earlier, later);
    GLINTFX_CHECK_EQ(elapsed.nanoseconds, static_cast<std::int64_t>(15));

    const double seconds = glintfx::gltfx_duration_to_seconds(elapsed);
    GLINTFX_CHECK(seconds >= 0.0);

    const glintfx::gltfx_duration round_tripped = glintfx::gltfx_duration_from_seconds(seconds);
    GLINTFX_CHECK_EQ(round_tripped.nanoseconds, elapsed.nanoseconds);
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

// gfss/value.hpp (GFSS-VALUE) survives the same hostile include order -
// the aggregate fields (gltfx_gfss_value/gltfx_gfss_length) have no
// call-shaped name for a function-like macro to pattern-match on, the
// SAME "no live target today" honesty this file's own header comment
// already states for gltfx_rgba/gltfx_gfss_token; gltfx_gfss_value_
// kind_name()/gltfx_gfss_length_unit_name() ARE call-shaped, so this
// case exercises both as real call expressions, the SAME CE-8
// discipline the two cases above already apply.
GLINTFX_TEST(gfss_value_header_survives_hostile_system_headers) {
    const std::string_view kind_name =
        glintfx::style::gltfx_gfss_value_kind_name(glintfx::style::gltfx_gfss_value_kind::length);
    GLINTFX_CHECK(kind_name == std::string_view{"length"});

    const std::string_view unit_name =
        glintfx::style::gltfx_gfss_length_unit_name(glintfx::style::gltfx_gfss_length_unit::rem);
    GLINTFX_CHECK(unit_name == std::string_view{"rem"});

    constexpr glintfx::style::gltfx_gfss_value value{
        .kind = glintfx::style::gltfx_gfss_value_kind::length,
        .keyword_text = {},
        .number = 0.0,
        .integer_value = 0,
        .length = {.magnitude = 16.0, .unit = glintfx::style::gltfx_gfss_length_unit::px},
        .percentage = 0.0,
    };
    GLINTFX_CHECK(value.kind == glintfx::style::gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(value.length.magnitude, 16.0);
}

// gfss/keyword.hpp (GFSS-PROP-REGISTRY) survives the same hostile
// include order - gltfx_gfss_keyword_name() is the ONE call-shaped
// entry point this header declares, exercised here as a real call
// expression the SAME CE-8 discipline the cases above already apply;
// the enum's own 56 enumerators are already exercised at their
// DECLARATION site, inside keyword.hpp's own enum class body, by the
// mere #include above under the hostile order (the SAME declaration-
// site guarantee this file's own header comment states for
// version.hpp) - so the two enumerators that needed the E2 reserved-
// word suffix (auto_keyword, static_keyword) are enough here to also
// exercise the name lookup as a real call expression.
GLINTFX_TEST(gfss_keyword_header_survives_hostile_system_headers) {
    const std::string_view auto_name =
        glintfx::style::gltfx_gfss_keyword_name(glintfx::style::gltfx_gfss_keyword::auto_keyword);
    GLINTFX_CHECK(auto_name == std::string_view{"auto"});

    const std::string_view static_name =
        glintfx::style::gltfx_gfss_keyword_name(glintfx::style::gltfx_gfss_keyword::static_keyword);
    GLINTFX_CHECK(static_name == std::string_view{"static"});
}

// gfss/property.hpp (GFSS-PROP-REGISTRY) survives the same hostile
// include order - gltfx_gfss_property_name(), gltfx_gfss_property_is_
// inherited() and gltfx_gfss_property_initial() are the three
// call-shaped entry points this header declares, exercised here as
// real call expressions the SAME CE-8 discipline the cases above
// already apply; the enum's own 104 enumerators are already exercised
// at their DECLARATION site, inside property.hpp's own enum class
// body, by the mere #include above under the hostile order, so one
// representative enumerator per accessor is enough here.
GLINTFX_TEST(gfss_property_header_survives_hostile_system_headers) {
    const std::string_view name =
        glintfx::style::gltfx_gfss_property_name(glintfx::style::gltfx_gfss_property::display);
    GLINTFX_CHECK(name == std::string_view{"display"});

    const bool inherited = glintfx::style::gltfx_gfss_property_is_inherited(
        glintfx::style::gltfx_gfss_property::color);
    GLINTFX_CHECK(inherited);

    const glintfx::style::gltfx_gfss_value initial =
        glintfx::style::gltfx_gfss_property_initial(glintfx::style::gltfx_gfss_property::opacity);
    GLINTFX_CHECK(initial.kind == glintfx::style::gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(initial.number, 1.0);
}

// gfui/node_view.hpp (GFSS-NODE-VIEW) survives the same hostile
// include order - gltfx_node_state_name() is the ONE call-shaped,
// ABI-crossing entry point this header declares (every ten forwarders
// of the table live in the internal node_query.hpp, not here), so it
// is exercised as a real call expression the SAME CE-8 discipline the
// cases above already apply; gltfx_node_state_has() and
// gltfx_node_facts_first_missing() are inline (nothing to hide behind
// the .so boundary for either), exercised here too since they are
// call-shaped as well.
GLINTFX_TEST(gfui_node_view_header_survives_hostile_system_headers) {
    const std::string_view bit_name =
        glintfx::gfui::gltfx_node_state_name(glintfx::gfui::gltfx_node_state::focus);
    GLINTFX_CHECK(bit_name == std::string_view{"focus"});

    GLINTFX_CHECK(glintfx::gfui::gltfx_node_state_has(glintfx::gfui::gltfx_node_state::hover,
                                                      glintfx::gfui::gltfx_node_state::hover));

    constexpr glintfx::gfui::gltfx_node_facts empty_facts{};
    GLINTFX_CHECK(glintfx::gfui::gltfx_node_facts_first_missing(empty_facts) ==
                  std::string_view{"tag_name"});

    constexpr glintfx::gfui::gltfx_node_view view{};
    GLINTFX_CHECK(view.node == nullptr);
}

// asset_file_header_survives_hostile_system_headers - ASSET-LOAD:
// glintfx::asset::gltfx_load_file_bytes() is entirely inline (docs/
// api-conventions.md R5(b): a container-by-value return has to stay
// header-only, the same reason glintfx::style::gltfx_gfss_tokenize()
// above does), so its call-shaped USE SITE is exercised at its own
// declaration, under the SAME hostile include order - a real temp
// file, not a stub path, so both the success arm (bytes read back) and
// the header's own compilability are proven together.
GLINTFX_TEST(asset_file_header_survives_hostile_system_headers) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "glintfx_hygiene_asset_load_probe";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file_path = dir / "probe.bin";
    {
        std::ofstream out(file_path, std::ios::binary);
        out.put(static_cast<char>(0x2A));
    }

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes(file_path.string());

    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result.value().size() == 1);
    GLINTFX_CHECK(result.value().at(0) == std::byte{0x2A});
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
    // two members (name, value). GLINTFX_CHECK is CASE-FATAL since
    // QA-HARNESS-ABORT (27/08/2026, harness/check.hpp's own header
    // comment): a failed !fields.empty() check now unwinds this case
    // immediately, so fields[0] below is never reached with an empty
    // fields in practice. The explicit `if (!fields.empty())` guard is
    // kept anyway, as belt-and-suspenders against cppcheck's own
    // containerOutOfBounds finding (which reads this function's source
    // alone and does not know GLINTFX_CHECK throws) and as
    // documentation that fields[0] below has a real precondition, not
    // an implicit one.
    const auto fields = glintfx::gltfx_err_fields(copied);
    GLINTFX_CHECK(!fields.empty());
    if (!fields.empty()) {
        GLINTFX_CHECK(fields[0].name == std::string_view{"code"});
        GLINTFX_CHECK(fields[0].value == "parse_failure");
    }
}

// window_header_survives_hostile_system_headers - W-D' (docs/plano-
// w6a-janela.md fatia 6): glintfx/platform/window/window.hpp's own
// plain-data vocabulary (gltfx_window_size, gltfx_window_desc,
// gltfx_window_state_bit) survives the same hostile include order -
// every field/enumerator is exercised as a real construction/read
// expression, the same CE-8 discipline the cases above already apply
// to aggregates with no call-shaped member (gltfx_rgba, gltfx_time_
// point above): none of these three names is call-shaped, so there is
// no USE-SITE macro collision to additionally exercise here, only the
// DECLARATION-SITE one this file's own header comment already states
// is what an aggregate's hostile-header check actually proves.
GLINTFX_TEST(window_header_survives_hostile_system_headers) {
    constexpr glintfx::gltfx_window_size logical{.width = 800, .height = 600};
    GLINTFX_CHECK_EQ(logical.width, static_cast<std::uint32_t>(800));
    GLINTFX_CHECK_EQ(logical.height, static_cast<std::uint32_t>(600));

    constexpr glintfx::gltfx_window_desc desc{
        .title = "janela",
        .application_id = "com.glintfx.probe",
        .logical_size = logical,
    };
    GLINTFX_CHECK(desc.title == std::string_view{"janela"});
    GLINTFX_CHECK(desc.logical_size.width == static_cast<std::uint32_t>(800));

    GLINTFX_CHECK(glintfx::gltfx_window_state_bit::active !=
                  glintfx::gltfx_window_state_bit::maximized);
    GLINTFX_CHECK(glintfx::gltfx_window_state_bit::fullscreen !=
                  glintfx::gltfx_window_state_bit::maximized);
}

// display_header_declaration_survives_hostile_system_headers - W-D'
// (docs/plano-w6a-janela.md fatia 6): glintfx/platform/window/
// display.hpp is only exercised at DECLARATION site here, never by an
// actual call to gltfx_display::open()/pump_events() - GODS_LAWS.md
// L-09 forbids opening any real display connection from this ctest
// (it runs in the plain unit-test job, never inside the isolated
// compositor container this project's window-opening tests require).
// The declaration-site guarantee this file's own header comment
// already states for version.hpp/gltfx_rgba applies unchanged here:
// gltfx_display's own name and every one of its method names already
// got parsed, under the SAME hostile include order set up above, by
// the mere #include line - if `open`, `is_open` or `pump_events`
// collided with a hostile macro at declaration, this translation unit
// would already have failed to compile before this test case even
// runs. A REAL call-expression proof of gltfx_display's own behavior
// lives in `window_parity_test` (tests/parity/window_parity_test.cpp)
// - ABSORBED (achado do time-lead, 06/09/2026, docs/plano-w6a-janela.md
// fatia 6's own "Par" column): the plan's own row 6 named a separate
// `public_display_test` fixture (open a public gltfx_display, pump,
// close), but window_parity_test.cpp already opens that SAME public
// gltfx_display before opening its own gltfx_window, pumps it, and
// closes it in reverse order, on both systems, under the identical
// P-0 shared name - a strict SUPERSET of what a standalone `public_
// display_test` would have proven, never a narrower substitute. A
// second fixture proving a strict subset of an existing one would be
// fragmentation, not coverage (GODS_LAWS.md L-17), so none was ever
// written under that name.
GLINTFX_TEST(display_header_declaration_survives_hostile_system_headers) { GLINTFX_CHECK(true); }

// window_handle_header_declaration_survives_hostile_system_headers -
// WL-WINDOW-HANDLE (docs/plano-w6a-janela.md fatias 6/9/11, absorbed
// into one fatia): the exact sibling of the gltfx_display case above,
// for gltfx_window (this fatia's own class, added to window.hpp) -
// same declaration-site-only guarantee, same GODS_LAWS.md L-09 reason
// a real open() cannot run from this ctest. A REAL call-expression
// proof of gltfx_window's own behavior lives in tests/parity/window_
// parity_test.cpp (this SAME fatia), staged and run as a container
// fixture on Linux and as a normal ctest on Windows - the "perna do
// shim hostil" this test case's own file-level comment (top of this
// file) names is the near/far collision check, proven separately by
// sabotaging a COPY of window.hpp with a field literally named `near`
// and confirming THAT copy fails to compile under the shim, never by
// a case committed here (there is no way to assert "this fails to
// compile" inside a runtime test harness).
GLINTFX_TEST(window_handle_header_declaration_survives_hostile_system_headers) {
    GLINTFX_CHECK(true);
}

// gfx_option_header_survives_hostile_system_headers - C-OPT (docs/
// plano-w6b-placa-e-laco.md fatia 2a): glintfx/platform/gl/gfx_option.
// hpp's four call-shaped entry points are exercised here as real call
// expressions, the SAME CE-8 discipline the cases above already apply;
// the enum's own eight enumerators and the three plain-data structs'
// own fields are already exercised at their DECLARATION site by the
// mere #include above under the hostile order.
GLINTFX_TEST(gfx_option_header_survives_hostile_system_headers) {
    const std::size_t count = glintfx::gltfx_gfx_option_count();
    GLINTFX_CHECK(count > 0);

    const glintfx::gltfx_gfx_option_info first = glintfx::gltfx_gfx_option_at(0);
    GLINTFX_CHECK(first.id == glintfx::gltfx_gfx_option::vsync);

    const glintfx::gltfx_gfx_option_info described =
        glintfx::gltfx_gfx_option_describe(glintfx::gltfx_gfx_option::vsync);
    GLINTFX_CHECK(described.name == std::string_view{"vsync"});

    const glintfx::gltfx_rslt<glintfx::gltfx_gfx_option> found =
        glintfx::gltfx_gfx_option_by_name("vsync");
    GLINTFX_CHECK(found.has_value());
    GLINTFX_CHECK(found.value() == glintfx::gltfx_gfx_option::vsync);

    constexpr glintfx::gltfx_gfx_option_entry entry{.id = glintfx::gltfx_gfx_option::vsync,
                                                    .value = 1};
    GLINTFX_CHECK(entry.value == 1);
}
