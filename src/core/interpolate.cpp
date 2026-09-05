// SPDX-License-Identifier: AGPL-3.0-or-later
//
// interpolate.cpp - linear interpolation. The measured failure of the
// obvious expression `a + t * (b - a)` at the far end, the measured
// answers std::lerp gives for non-finite input, and the source for the
// form used here all live in include/glintfx/core/interpolate.hpp.

#include <cmath>
#include <limits>

#include <glintfx/core/interpolate.hpp>

namespace glintfx {

double gltfx_lerp(double a, double b, double t) noexcept {
    // The guard runs BEFORE any call into the standard library, never
    // after - the same order-of-operations fix core/time.hpp documents
    // for its own std::llround call. Measured on this toolchain,
    // std::lerp answers a non-finite input with a plausible FINITE
    // number (see the header): deciding it here is what keeps the
    // answer identical on all five targets (GODS_LAWS.md L-04).
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(t)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Every finite input goes to std::lerp untouched: the standard
    // requires exactness at both ends and monotonicity near the far
    // end, which is precisely what the naive expression loses. It is
    // standard library, so nothing here costs a dependency
    // (GODS_LAWS.md L-07).
    return std::lerp(a, b, t);
}

} // namespace glintfx
