// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/nul_terminated_name.hpp"

// proc_name_buffer_test.cpp - NOEXCEPT-ALLOC-B8 fatia F1
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F1"): unit
// coverage of the shared atom itself, on all five systems (pure
// template, no OS header - unguarded, the same shape check_layers.py's
// own comment gives for cross-platform gates). This file alone does
// NOT prove B8 fixed (src/platform/wayland/egl_context_adapter.cpp:851
// still needs to CALL this atom - that is gl_proc_address_oom_test.cpp,
// a separate file, on purpose: "o teste do atomo e honesto e verde; o
// sitio continua com std::string" is form 1 of the plan's own sec. 10
// table, and this file's own existence never claims to close it).

using glintfx::platform::copy_nul_terminated;
using glintfx::platform::k_max_proc_name_chars;

namespace {
constexpr std::size_t k_n = 8;
} // namespace

GLINTFX_TEST(k_max_proc_name_chars_is_256_paridade_with_win32) {
    // D-2 of the plan: 256, byte-for-byte parity with the number
    // src/platform/win32/wgl_proc_address.cpp already used before this
    // atom replaced its own private k_max_name_chars.
    GLINTFX_CHECK_EQ(k_max_proc_name_chars, static_cast<std::size_t>(256));
}

GLINTFX_TEST(name_that_comfortably_fits_is_copied_and_terminated) {
    std::array<char, k_n> buffer{};
    const bool ok = copy_nul_terminated(buffer, std::string_view("gl"));
    GLINTFX_CHECK(ok);
    GLINTFX_CHECK_EQ(buffer[0], 'g');
    GLINTFX_CHECK_EQ(buffer[1], 'l');
    GLINTFX_CHECK_EQ(buffer[2], '\0');
}

GLINTFX_TEST(name_that_fits_exactly_in_the_last_position_is_copied) {
    // N - 1 characters leave EXACTLY one byte for the terminator - the
    // largest name this buffer can ever legitimately hold.
    std::array<char, k_n> buffer{};
    const std::string_view exact_fit("glClear"); // 7 == k_n - 1
    GLINTFX_CHECK_EQ(exact_fit.size(), k_n - 1);
    const bool ok = copy_nul_terminated(buffer, exact_fit);
    GLINTFX_CHECK(ok);
    for (std::size_t i = 0; i < exact_fit.size(); ++i) {
        GLINTFX_CHECK_EQ(buffer[i], exact_fit[i]);
    }
    GLINTFX_CHECK_EQ(buffer[exact_fit.size()], '\0');
}

// THE CASE THAT CATCHES MUTATION m2 (the plan's own F1 section): a
// name exactly buffer.size() long has NO room left for the terminator
// and MUST be refused - never truncated, never accepted with an
// out-of-bounds write at buffer[buffer.size()]. Built from a raw
// std::array rather than a string literal so its length is exactly
// k_n, independent of how k_n is spelled above.
GLINTFX_TEST(name_that_overflows_by_exactly_one_byte_is_refused) {
    std::array<char, k_n> buffer{};
    const std::array<char, k_n> raw{'g', 'l', 'C', 'l', 'e', 'a', 'r', 'X'};
    const std::string_view name(raw.data(), raw.size());
    GLINTFX_CHECK_EQ(name.size(), k_n);
    const bool ok = copy_nul_terminated(buffer, name);
    GLINTFX_CHECK(!ok);
}

GLINTFX_TEST(one_past_the_boundary_is_also_refused) {
    // The plan's own L-43 requirement: prove a step BEYOND the border,
    // never only the exact border.
    std::array<char, k_n> buffer{};
    const std::array<char, k_n + 1> raw{'g', 'l', 'C', 'l', 'e', 'a', 'r', 'X', 'Y'};
    const std::string_view name(raw.data(), raw.size());
    const bool ok = copy_nul_terminated(buffer, name);
    GLINTFX_CHECK(!ok);
}

GLINTFX_TEST(empty_name_is_copied_as_an_empty_terminated_string) {
    std::array<char, k_n> buffer{};
    const bool ok = copy_nul_terminated(buffer, std::string_view());
    GLINTFX_CHECK(ok);
    GLINTFX_CHECK_EQ(buffer[0], '\0');
}

GLINTFX_TEST(embedded_nul_byte_is_copied_byte_for_byte_not_stopped_at) {
    // string_view::copy() is a raw byte copy - it does not treat an
    // embedded '\0' as a terminator. This atom does not either: it
    // only APPENDS its own terminator at name.size(), and this case
    // proves that contract explicitly rather than leaving it implicit.
    std::array<char, k_n> buffer{};
    const std::array<char, 4> raw{'g', '\0', 'l', 'X'};
    const std::string_view name(raw.data(), raw.size());
    const bool ok = copy_nul_terminated(buffer, name);
    GLINTFX_CHECK(ok);
    GLINTFX_CHECK_EQ(buffer[0], 'g');
    GLINTFX_CHECK_EQ(buffer[1], '\0');
    GLINTFX_CHECK_EQ(buffer[2], 'l');
    GLINTFX_CHECK_EQ(buffer[3], 'X');
    GLINTFX_CHECK_EQ(buffer[4], '\0'); // the atom's own terminator, at name.size()
}
