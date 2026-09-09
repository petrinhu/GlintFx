// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdio>

#include "core_log_cost_functions.hpp"

// core_log_cost.cpp - CL-5 of CORE-LOG (TODO.md): measures the cost
// of a SUPPRESSED emission (plano/core-log.md Q4's promise: "sem
// recebedor o custo e uma carga atomica (acquire) mais uma
// comparacao"), in the CE-7 mold (two TUs, see core_log_cost_
// functions.hpp's own comment for why a single-TU version would
// measure the optimizer instead of the ABI). Not wired into CMake
// (same as core_error_cost.cpp) - a manual tool, run once per
// measurement, its number recorded in TODO.md, never in a header
// (numbers written into prose age; the command that reproduces them
// does not).

namespace {

using clock_type = std::chrono::steady_clock;

constexpr int k_iterations = 2'000'000;

double ns_per_op(clock_type::duration total, int iterations) {
    return std::chrono::duration<double, std::nano>(total).count() /
           static_cast<double>(iterations);
}

} // namespace

int main() {
    const auto start = clock_type::now();
    for (int i = 0; i < k_iterations; ++i) {
        glintfx::bench::suppressed_emit();
    }
    const auto end = clock_type::now();

    std::printf("suppressed_emit(): %.3f ns/op (%d iterations)\n",
                ns_per_op(end - start, k_iterations), k_iterations);
    return 0;
}
