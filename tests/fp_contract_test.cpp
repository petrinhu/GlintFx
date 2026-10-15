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
// The three values go through `volatile`, a defence against the compiler folding the expression at
// compile time (a constant folded with contraction would hide the runtime code, llvm-project
// #128545). It is a defence, not a measured need: with GCC 16 and Clang 22 the test fails without
// the rule and passes with it whether or not the values are volatile (the S5-A-fix3 review, M6
// and M6B).
// A DECLARED LIMIT: on a target without the instruction the compiler cannot fuse even without the
// rule, so there the test passes either way. That is x86-64 below v3 under GCC and Clang (the
// default of this project's Linux CI) and the default x64 of MSVC (no /arch, the project's
// Windows CI). It bites on x86-64-v3 builds: measured by the S5-A-fix3 pre-execution, which fails
// without the rule under -march=x86-64-v3. On arm64, which has the instruction too, it is expected
// to bite, but that was NOT measured: there is no arm64 machine here (S5-A-fix4 review, C-7).

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
