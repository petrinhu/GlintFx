// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <new>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/value.hpp>

#include "harness/check.hpp"
#include "harness/compiler_noinline.hpp"
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

namespace {

// GLINTFX_TEST_NOINLINE (compiler_noinline.hpp) AND a type-erased
// pointer+size (never std::array directly) - measured necessary, not
// decoration: with the poisoning, the placement-new and the byte
// check all visible in ONE function, GCC's own -O3 optimizer proves
// the SAME uninitialized bytes this case exists to catch AT COMPILE
// TIME (elements 20-23 of the 24-byte object, named individually in
// the diagnostic) and -Werror refuses to build at all - correct, but
// it would collapse this into the SAME kind of proof as Vermelho 1 (a
// compile error), when the point of THIS case is a genuinely RUNTIME
// check that only fails when the bug is actually present today and
// passes cleanly once the fix lands. Crossing an opaque, noinline
// function boundary with a raw pointer is what keeps the read from
// being provably-uninitialized to the compiler, the same "the
// optimizer must not see through this" need CE-7's own benchmark
// functions document (tools/bench/core_log_cost_functions.cpp) - the
// per-compiler spelling of "noinline" itself lives in ONE place
// (compiler_noinline.hpp), not re-derived here, after CORE-LOG-CI
// proved [[gnu::noinline]] alone is GCC/Clang-only and fails MSVC.
GLINTFX_TEST_NOINLINE int count_zero_bytes_from(const std::byte *data, std::size_t offset,
                                                std::size_t size) {
    int checked = 0;
    for (std::size_t i = offset; i < size; ++i) {
        GLINTFX_CHECK(data[i] == std::byte{0});
        ++checked;
    }
    return checked;
}

} // namespace

// CORE-LOG-CI, DEFEITO 2 (run 34329543846, job "Sanitizer (Fedora -
// ASan/UBSan)"): the default member initializer used to live on
// `as_unsigned_integer` (8 bytes), but the union's own storage is 16
// bytes wide (`text_view`, the largest member) - a default-constructed
// gltfx_log_value left the SECOND 8 bytes (as_text.size) genuinely
// indeterminate. GCC caught it for real, under sanitizer instrumentation,
// inlined into text()'s own read of as_text.size - not a false positive
// (gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html's own warning
// about sanitizers and -Wmaybe-uninitialized does not apply here).
//
// This case poisons the raw storage FIRST (0xAB, never zero, so a
// pass can only mean the constructor actually wrote every byte, not
// that the buffer happened to start zeroed), then default-constructs
// IN PLACE over it and checks every byte from offsetof(as_text) to the
// end of the object is zero. Deliberately does NOT check the 4 bytes
// of padding between `kind` and the union (offsetof(as_text) is
// exactly where that padding ends) - nobody promises padding, only the
// union's own storage.
GLINTFX_TEST(log_value_default_construction_defines_every_union_byte) {
    alignas(gltfx_log_value) std::array<std::byte, sizeof(gltfx_log_value)> storage;
    storage.fill(std::byte{0xAB});

    new (storage.data()) gltfx_log_value;

    const std::size_t union_offset = offsetof(gltfx_log_value, as_text);
    const int checked = count_zero_bytes_from(storage.data(), union_offset, storage.size());
    std::println("log_value_default_construction_defines_every_union_byte: {} byte(s) of the "
                 "union checked, offset {} to {}",
                 checked, union_offset, storage.size());
    GLINTFX_CHECK(checked > 0);
}

// Guardian, not proof (never seen failing on its own - the Vermelho 1
// compile error above is the real proof, GODS_LAWS.md L-40): if a
// future change ever adds a union member LARGER than `text_view`
// without moving the default member initializer to it, this fails to
// compile-time-flag that the initializer no longer covers the whole
// union. Holds today because `as_text` (text_view: pointer + size_t)
// already IS the union's largest, and therefore only, member whose
// size equals the union's own footprint from its own offset to the
// end of gltfx_log_value.
GLINTFX_TEST(log_value_default_initializer_lives_on_the_largest_union_member) {
    static_assert(sizeof(gltfx_log_value) - offsetof(gltfx_log_value, as_text) ==
                  sizeof(gltfx_log_value::text_view));
    GLINTFX_CHECK(true);
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
