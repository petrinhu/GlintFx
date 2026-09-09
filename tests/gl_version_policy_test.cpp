// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <print>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/gl/gl_version_policy.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gl_version_policy_test.cpp - GL-CONTEXT (docs/plano-w6b-placa-e-
// laco.md fatia 2b, D-W6b-4, GODS_LAWS.md L-20/L-40): the TDD red/
// green witness for glintfx::platform::validate_gl_context_version()
// (src/platform/gl/gl_version_policy.hpp) - the seven named cases plus
// the CLOSED 12-cell enumeration {major<3, ==3&minor<3, ==3&minor>=3,
// >3} x {core, compat, none} the plan's own fatia 2b row names.
//
// RED, SEEN: before gl_version_policy.{hpp,cpp} existed, this file's
// own #include line failed to compile.

using glintfx::platform::k_gl_context_compatibility_profile_bit;
using glintfx::platform::k_gl_context_core_profile_bit;
using glintfx::platform::validate_gl_context_version;

GLINTFX_TEST(validate_gl_context_version_accepts_3_3_core) {
    const glintfx::gltfx_rslt<void> result =
        validate_gl_context_version(3, 3, k_gl_context_core_profile_bit);
    GLINTFX_CHECK(result.has_value());
}

GLINTFX_TEST(validate_gl_context_version_accepts_4_6_core) {
    const glintfx::gltfx_rslt<void> result =
        validate_gl_context_version(4, 6, k_gl_context_core_profile_bit);
    GLINTFX_CHECK(result.has_value());
}

GLINTFX_TEST(validate_gl_context_version_refuses_3_3_compatibility) {
    const glintfx::gltfx_rslt<void> result =
        validate_gl_context_version(3, 3, k_gl_context_compatibility_profile_bit);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "gl_version");
}

GLINTFX_TEST(validate_gl_context_version_refuses_3_2_core) {
    const glintfx::gltfx_rslt<void> result =
        validate_gl_context_version(3, 2, k_gl_context_core_profile_bit);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "gl_version");
}

GLINTFX_TEST(validate_gl_context_version_refuses_2_1) {
    const glintfx::gltfx_rslt<void> result =
        validate_gl_context_version(2, 1, k_gl_context_core_profile_bit);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::platform_failure);
}

GLINTFX_TEST(validate_gl_context_version_refuses_3_3_with_no_profile_bit_at_all) {
    const glintfx::gltfx_rslt<void> result = validate_gl_context_version(3, 3, 0);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::platform_failure);
    GLINTFX_CHECK(result.err().rejected_value() == "gl_version");
}

namespace {

struct version_cell {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t profile_mask = 0;
    bool expect_accept = false;
};

} // namespace

// The closed 12-cell enumeration: {major<3, ==3&minor<3, ==3&minor>=3,
// >3} x {core, compat, none}. Only (>=3.3, core) accepts - the same
// two buckets the seven named cases above already exercise
// individually, here checked as ONE closed table with a printed count
// (GODS_LAWS.md L-40: a non-empty scan, never zero).
GLINTFX_TEST(validate_gl_context_version_closed_twelve_cell_enumeration) {
    const std::array<version_cell, 12> cells{{
        // major < 3
        {2, 1, k_gl_context_core_profile_bit, false},
        {2, 1, k_gl_context_compatibility_profile_bit, false},
        {2, 1, 0, false},
        // major == 3, minor < 3
        {3, 2, k_gl_context_core_profile_bit, false},
        {3, 2, k_gl_context_compatibility_profile_bit, false},
        {3, 2, 0, false},
        // major == 3, minor >= 3
        {3, 3, k_gl_context_core_profile_bit, true},
        {3, 3, k_gl_context_compatibility_profile_bit, false},
        {3, 3, 0, false},
        // major > 3
        {4, 6, k_gl_context_core_profile_bit, true},
        {4, 6, k_gl_context_compatibility_profile_bit, false},
        {4, 6, 0, false},
    }};

    int analyzed = 0;
    for (const version_cell &cell : cells) {
        const glintfx::gltfx_rslt<void> result =
            validate_gl_context_version(cell.major, cell.minor, cell.profile_mask);
        GLINTFX_CHECK(result.has_value() == cell.expect_accept);
        ++analyzed;
    }
    GLINTFX_CHECK_EQ(analyzed, static_cast<int>(cells.size()));
    std::println("validate_gl_context_version_closed_twelve_cell_enumeration: {} cells analyzed, "
                 "{} found",
                 analyzed, cells.size());
}
