// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/error_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// error_code_test.cpp - CE-1 of CORE-ERROR (TODO.md, GODS_LAWS.md L-20,
// L-40): proves the whole error_code table is internally consistent -
// every enumerator has a name, no two enumerators share a name, no two
// enumerators share a numeric value - and that a value outside the
// table degrades to "unknown" instead of undefined behavior.
//
// ENUMERATION, not directed search (GODS_LAWS.md L-17/L-27 lineage,
// "enumere o espaco pequeno"): k_all_codes below is the closed,
// hand-written list of every enumerator error_code.hpp declares. There
// is no enum reflection in C++23 to walk it automatically; this list
// IS the enumeration, meant to be edited in lockstep with the header -
// a code added there and forgotten here would silently escape this
// test, which is exactly why the count is PRINTED (L-40), not just
// asserted: a stale k_all_codes still passes silently unless a human
// reads the printed count against the header's own enumerator count.

namespace {

constexpr std::array<glintfx::error_code, 8> k_all_codes{
    glintfx::error_code::unknown,          glintfx::error_code::out_of_memory,
    glintfx::error_code::io_failure,       glintfx::error_code::not_found,
    glintfx::error_code::invalid_argument, glintfx::error_code::parse_failure,
    glintfx::error_code::unsupported,      glintfx::error_code::platform_failure,
};

bool any_value_repeated(const std::array<std::uint32_t, k_all_codes.size()> &values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) {
                return true;
            }
        }
    }
    return false;
}

bool any_name_repeated(const std::array<std::string_view, k_all_codes.size()> &names) {
    for (std::size_t i = 0; i < names.size(); ++i) {
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            if (names[i] == names[j]) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

GLINTFX_TEST(every_code_has_a_distinct_name_and_value) {
    std::array<std::string_view, k_all_codes.size()> names{};
    std::array<std::uint32_t, k_all_codes.size()> values{};

    for (std::size_t i = 0; i < k_all_codes.size(); ++i) {
        names[i] = glintfx::error_code_name(k_all_codes[i]);
        values[i] = static_cast<std::uint32_t>(k_all_codes[i]);
        GLINTFX_CHECK(!names[i].empty());
    }

    // error_code::unknown (index 0) is the only enumerator allowed to
    // be named "unknown"; any OTHER entry named "unknown" means the
    // implementation's lookup table is missing that entry and silently
    // fell back to the sentinel instead of naming it.
    for (std::size_t i = 1; i < names.size(); ++i) {
        GLINTFX_CHECK(names[i] != "unknown");
    }

    GLINTFX_CHECK(!any_name_repeated(names));
    GLINTFX_CHECK(!any_value_repeated(values));

    // L-40: the count of what was actually enumerated is printed even
    // when everything passes - a portal that scans nothing and prints
    // green is the defect this project's gates exist to never ship.
    std::println("error_code_test: {} enumerator(s) checked, all names and values distinct",
                 k_all_codes.size());
}

GLINTFX_TEST(value_outside_the_table_degrades_to_unknown) {
    // The out-of-range cast below is the point of this case, not a
    // mistake: it simulates a value a NEWER glintfx produced that THIS
    // build's table has never heard of, and proves error_code_name()
    // degrades to "unknown" instead of undefined behavior.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: see comment above
    const auto outside = static_cast<glintfx::error_code>(0xFFFFFFFFU);
    GLINTFX_CHECK(glintfx::error_code_name(outside) == std::string_view{"unknown"});
}
