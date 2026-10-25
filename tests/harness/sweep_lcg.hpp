// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// harness/sweep_lcg.hpp - fatia CURVE-FLOAT-NARROW (GODS_LAWS.md L-33 global, the rule of three):
// the fixed integer generator of the sweeps of arc_parameters_test, stroke_line_pixels_test and
// ellipse_pixels_test, the same numbers on every system. A 64-bit linear congruential step (the
// multiplier and increment of Knuth's MMIX) and the top 53 bits as a double in [0, 1). Each sweep
// gives it a seed of its own.
namespace glintfx::test_support {

struct lcg {
    std::uint64_t state = 0U;
    double next() {
        state = (state * 6364136223846793005ULL) + 1442695040888963407ULL;
        return static_cast<double>(state >> 11U) * 0x1.0p-53;
    }
};

} // namespace glintfx::test_support
