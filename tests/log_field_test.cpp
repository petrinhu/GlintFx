// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/value.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_field_test.cpp - CL-2 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-19/L-20/L-26/L-40): gltfx_log_value/gltfx_log_field, plano/
// core-log.md Q3's tagged, non-allocating, five-kind field type.
//
// NO std::variant, NO std::string (Q3): a std::variant's layout is
// the standard library's own to define, not this project's frozen ABI
// (the same reasoning err.hpp's own header comment gives for why
// gltfx_err is NOT a std::expected); std::string allocates and is not
// trivially copyable, which would also break the alloc-free promise
// CL-5 proves. GLTFX_LOG_VALUE_HEADER_SOURCE/GLTFX_LOG_FIELD_HEADER_
// SOURCE below are the two headers' own source text, injected as
// compile definitions by tests/CMakeLists.txt - the "grep no proprio
// teste" plano/core-log.md CL-2 calls for, done in C++ against the
// REAL committed header text rather than trusted from a type trait
// alone (a static_assert on standard_layout/trivially_copyable would
// already reprove a std::variant/std::string member indirectly, but
// this reads the actual source, the same discipline check_public_name_
// collision.py's own header comment prefers - "real varredura, nao
// lista curada").

using glintfx::gltfx_log_field;
using glintfx::gltfx_log_value;
using glintfx::gltfx_log_value_kind;

namespace {

std::string read_header(std::string_view compile_time_path) {
    const std::ifstream in{std::string(compile_time_path)};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

GLINTFX_TEST(log_value_kind_underlying_type_is_uint32) {
    GLINTFX_CHECK((std::is_same_v<std::underlying_type_t<gltfx_log_value_kind>, std::uint32_t>));
}

GLINTFX_TEST(log_value_default_is_unknown_and_reads_as_empty_or_zero) {
    const gltfx_log_value v;
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::unknown);
    // R4: unknown/mismatched kind never crashes, never reads
    // uninitialized union storage - degrades to empty/zero.
    GLINTFX_CHECK(v.text().empty());
    GLINTFX_CHECK(v.signed_integer() == 0);
    GLINTFX_CHECK(v.unsigned_integer() == 0);
    GLINTFX_CHECK(v.floating() == 0.0);
    GLINTFX_CHECK(v.boolean() == false);
}

GLINTFX_TEST(log_value_text_round_trips) {
    const gltfx_log_value v = gltfx_log_value::make_text("dedicated");
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::text);
    GLINTFX_CHECK(v.text() == std::string_view{"dedicated"});
    GLINTFX_CHECK(v.signed_integer() == 0); // mismatched kind: R4 default, not UB
}

GLINTFX_TEST(log_value_signed_integer_round_trips) {
    const gltfx_log_value v = gltfx_log_value::make_signed_integer(-42);
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::signed_integer);
    GLINTFX_CHECK(v.signed_integer() == -42);
    GLINTFX_CHECK(v.text().empty());
}

GLINTFX_TEST(log_value_unsigned_integer_round_trips) {
    const gltfx_log_value v = gltfx_log_value::make_unsigned_integer(7);
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::unsigned_integer);
    GLINTFX_CHECK(v.unsigned_integer() == 7);
}

GLINTFX_TEST(log_value_floating_round_trips) {
    const gltfx_log_value v = gltfx_log_value::make_floating(1.5);
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::floating);
    GLINTFX_CHECK(v.floating() == 1.5);
}

GLINTFX_TEST(log_value_boolean_round_trips) {
    const gltfx_log_value v = gltfx_log_value::make_boolean(true);
    GLINTFX_CHECK(v.kind == gltfx_log_value_kind::boolean);
    GLINTFX_CHECK(v.boolean() == true);
}

// Frozen layout: measured, not guessed - same discipline gltfx_err's
// own `2 * sizeof(void*)` static_assert follows (err.hpp).
GLINTFX_TEST(log_value_layout_is_frozen) {
    GLINTFX_CHECK(std::is_standard_layout_v<gltfx_log_value>);
    GLINTFX_CHECK(std::is_trivially_copyable_v<gltfx_log_value>);
    GLINTFX_CHECK(sizeof(gltfx_log_value) == 3 * sizeof(void *));
    GLINTFX_CHECK(offsetof(gltfx_log_value, kind) == 0);
}

GLINTFX_TEST(log_field_round_trips_and_is_frozen_layout) {
    const gltfx_log_field f{"kind", gltfx_log_value::make_text("dedicated")};
    GLINTFX_CHECK(f.name == std::string_view{"kind"});
    GLINTFX_CHECK(f.value.text() == std::string_view{"dedicated"});
    GLINTFX_CHECK(std::is_standard_layout_v<gltfx_log_field>);
    GLINTFX_CHECK(std::is_trivially_copyable_v<gltfx_log_field>);
}

GLINTFX_TEST(log_value_header_has_no_variant_or_string) {
    const std::string source = read_header(GLTFX_LOG_VALUE_HEADER_SOURCE);
    GLINTFX_CHECK(!source.empty());
    GLINTFX_CHECK(source.find("std::variant") == std::string::npos);
    GLINTFX_CHECK(source.find("std::string ") == std::string::npos);
    GLINTFX_CHECK(source.find("std::string>") == std::string::npos);
    GLINTFX_CHECK(source.find("<string>") == std::string::npos);
}

GLINTFX_TEST(log_field_header_has_no_variant_or_string) {
    const std::string source = read_header(GLTFX_LOG_FIELD_HEADER_SOURCE);
    GLINTFX_CHECK(!source.empty());
    GLINTFX_CHECK(source.find("std::variant") == std::string::npos);
    GLINTFX_CHECK(source.find("std::string ") == std::string::npos);
    GLINTFX_CHECK(source.find("std::string>") == std::string::npos);
    GLINTFX_CHECK(source.find("<string>") == std::string::npos);
}
