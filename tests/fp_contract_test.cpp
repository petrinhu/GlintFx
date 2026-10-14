// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cmath>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// fp_contract_test.cpp - S5-A-fix3 (D-W8-441, D-W8-443; GODS_LAWS.md L-04): every glintfx target
// rounds each floating-point operation as written; the compiler never fuses a * b + c into one
// multiply-add (cmake/GlintfxCompileOptions.cmake: -ffp-contract=off under GCC and Clang; MSVC's
// /fp:precise does not contract). The values discriminate by construction, before the code:
// a = b = 1 + 2^-30, so a * b = 1 + 2^-29 + 2^-60, which rounds to 1 + 2^-29 (the 2^-60 is below
// the last place); with c = -(1 + 2^-29), the separate operations give exactly 0 and the fused one
// gives 2^-60. std::fma gives the fused answer on purpose, as the control: if it did not, the
// values would not tell the two apart and the test would prove nothing.
//
// The three values go through `volatile`, so the compiler cannot fold the expression at compile
// time (a constant folded with contraction would hide the runtime code, llvm-project #128545).
// A DECLARED LIMIT: on a target without the instruction (x86-64 below v3, the default of this
// project's CI) the compiler cannot fuse even without the flag, so there the test passes either
// way; it bites on arm64 and on x86-64-v3 builds, which is where the rule matters (the CTO's
// pre-execution proves it there, with -march=x86-64-v3, failing without the flag).

namespace {

[[nodiscard]] double read_volatile(const volatile double &value) { return value; }

} // namespace

GLINTFX_TEST(fp_contract_a_times_b_plus_c_rounds_twice) {
    const volatile double a = 1.0 + 0x1.0p-30;
    const volatile double b = 1.0 + 0x1.0p-30;
    const volatile double c = -(1.0 + 0x1.0p-29);
    const double x = read_volatile(a);
    const double y = read_volatile(b);
    const double z = read_volatile(c);
    GLINTFX_CHECK_EQ(std::fma(x, y, z), 0x1.0p-60);
    const double separate = (x * y) + z;
    GLINTFX_CHECK_EQ(separate, 0.0);
}
