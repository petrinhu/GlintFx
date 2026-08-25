// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "core_error_cost_functions.hpp"

// core_error_cost.cpp - CE-7 of CORE-ERROR (TODO.md): measures, not
// infers, the two costs the leader named:
//
//   (1) constructing a code-only gltfx_err vs. one with context
//       attached;
//   (2) returning a raw value directly vs. returning it wrapped in
//       gltfx_rslt<T>, on the SUCCESS PATH ONLY - the CTO's inference
//       under test ("o envelope com destrutor nao trivial forca
//       retorno por memoria em vez de registrador, inclusive no
//       caminho de sucesso").
//
// The four functions being timed are declared in
// core_error_cost_functions.hpp and DEFINED in the separate TU
// core_error_cost_functions.cpp - see that header's own comment for
// why a single-file version of this benchmark measured the optimizer,
// not the ABI (GCC's interprocedural constant propagation folded a
// trivial same-TU function's return value into a compile-time
// constant despite [[gnu::noinline]], timing 0.000 ns/op for a real
// function call - caught by inspecting the printed number, not
// assumed correct because the program ran and produced output).
//
// PART A - THE DETERMINISTIC ABI FACT, not an inference: the Itanium
// C++ ABI (section 3.1.1, "non-trivial for the purposes of calls")
// mandates that a class with a non-trivial destructor, copy
// constructor or move constructor is passed and returned via HIDDEN
// REFERENCE (memory), never in registers, regardless of its size or
// of what the template parameter is. gltfx_rslt<T> stores a
// std::variant<T, gltfx_err>; gltfx_err has a user-declared,
// out-of-line (non-trivial) destructor (err.hpp) - so
// std::variant<T, gltfx_err>, and therefore gltfx_rslt<T> itself, can
// NEVER be trivially copyable, for ANY T, including plain `int`. The
// static_assert below is not a benchmark result; it is a compile-time,
// permanent, always-checked proof of the ABI classification itself -
// if it ever failed to compile, the CTO's inference would already be
// WRONG before a single nanosecond was measured.
static_assert(!std::is_trivially_copyable_v<glintfx::gltfx_rslt<int>>,
              "gltfx_rslt<int> is intentionally NOT trivially copyable (it owns a gltfx_err, "
              "which has a non-trivial destructor) - by the Itanium C++ ABI's own definition "
              "(3.1.1), this means it is ALWAYS returned via hidden reference/memory, never in "
              "registers, on every platform whose ABI follows that rule (GCC/Clang everywhere, "
              "and MSVC's x64 ABI applies the equivalent classification). This assertion is the "
              "permanent, compiler-checked half of CE-7's measurement; the timing below is the "
              "empirical half.");

namespace {

using clock_type = std::chrono::steady_clock;

constexpr int k_iterations = 2'000'000;

double ns_per_op(clock_type::duration total, int iterations) {
    return std::chrono::duration<double, std::nano>(total).count() /
           static_cast<double>(iterations);
}

} // namespace

int main() {
    using namespace glintfx::bench;

    // Comparison 1 (the leader's first pair): code-only vs.
    // context-attached construction. Both loops accumulate the
    // constructed error's code into a sink so the compiler cannot
    // prove the calls are dead and elide them.
    std::uint32_t sink1 = 0;
    const auto t1_start = clock_type::now();
    for (int i = 0; i < k_iterations; ++i) {
        sink1 += static_cast<std::uint32_t>(make_code_only().code());
    }
    const auto t1_end = clock_type::now();

    std::uint32_t sink2 = 0;
    const auto t2_start = clock_type::now();
    for (int i = 0; i < k_iterations; ++i) {
        sink2 += static_cast<std::uint32_t>(make_with_context().code());
    }
    const auto t2_end = clock_type::now();

    // Comparison 2 (the leader's second pair, "esta e a que
    // interessa"): direct value return vs. gltfx_rslt<T> envelope
    // return, SUCCESS PATH ONLY - never touching the error branch, so
    // any delta measured here is the ENVELOPE's own cost, not the
    // cost of carrying an error.
    long long sink3 = 0;
    const auto t3_start = clock_type::now();
    for (int i = 0; i < k_iterations; ++i) {
        sink3 += direct_value();
    }
    const auto t3_end = clock_type::now();

    long long sink4 = 0;
    const auto t4_start = clock_type::now();
    for (int i = 0; i < k_iterations; ++i) {
        sink4 += wrapped_value().value();
    }
    const auto t4_end = clock_type::now();

    const double code_only_ns = ns_per_op(t1_end - t1_start, k_iterations);
    const double with_context_ns = ns_per_op(t2_end - t2_start, k_iterations);
    const double direct_ns = ns_per_op(t3_end - t3_start, k_iterations);
    const double wrapped_ns = ns_per_op(t4_end - t4_start, k_iterations);

    std::printf("iterations=%d\n", k_iterations);
    std::printf("construct_code_only_ns_per_op=%.3f\n", code_only_ns);
    std::printf("construct_with_context_ns_per_op=%.3f\n", with_context_ns);
    std::printf("construct_context_minus_code_only_ns=%.3f\n", with_context_ns - code_only_ns);
    std::printf("return_direct_value_ns_per_op=%.3f\n", direct_ns);
    std::printf("return_wrapped_rslt_ns_per_op=%.3f\n", wrapped_ns);
    std::printf("return_wrapped_minus_direct_ns=%.3f\n", wrapped_ns - direct_ns);

    // Sinks printed so the optimizer cannot prove any loop above is
    // provably-dead code and remove it - the printed numbers would
    // change if it did, so keeping them alive is a correctness
    // requirement of the measurement, not decoration.
    std::printf("sink1=%u sink2=%u sink3=%lld sink4=%lld\n", sink1, sink2, sink3, sink4);
    return 0;
}
