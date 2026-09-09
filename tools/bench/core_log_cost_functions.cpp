// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core_log_cost_functions.hpp"

#include <span>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/value.hpp>

#include "core/log/emit.hpp"

// core_log_cost_functions.cpp - CL-5 of CORE-LOG (TODO.md, molde
// CE-7): bodies, in their OWN translation unit - see the header's own
// comment for why that split is a measurement-correctness
// requirement, not style.
//
// CORE-LOG-CI (run 34350043551, GODS_LAWS.md L-17): the twin of the
// [[gnu::noinline]] MSVC-C2220 regression fixed in tests/log_field_
// test.cpp (see tests/harness/compiler_noinline.hpp for the full
// measured story). Measured here, not assumed: neither this file nor
// any target under tools/bench/ is registered in ANY CMakeLists.txt
// (only tools/gl_registry_codegen is add_subdirectory()'d from the
// top-level one) or referenced by any .github/workflows/*.yml step -
// grep for "tools/bench" and "core_log_cost" across both finds
// nothing. tools/bench_core_error.sh, the one committed manual-build
// script in tools/, only ever compiles core_error_cost(_functions).cpp
// - core_log_cost(_functions).cpp has no equivalent script. So this
// [[gnu::noinline]] is not compiled by Windows CI TODAY and did not
// contribute to the regression - but it is the SAME bomb with a
// longer fuse, and the fix costs nothing to apply now rather than
// wait for someone to wire this file into a build. GLINTFX_BENCH_
// NOINLINE below is a SEPARATE definition from GLINTFX_TEST_NOINLINE
// (tests/harness/compiler_noinline.hpp), not a shared header: tools/
// bench/ has no existing include-path or build coupling to tests/ (no
// file in the repo does a parent-relative "../" include across a
// top-level directory boundary, confirmed by grep), and manufacturing
// one now, for a mechanism this small, would be a new dependency the
// architecture doesn't otherwise have - GODS_LAWS.md L-33's "regra de
// 3" keeps two independent, equally-named, per-compiler definitions
// WET rather than force a cross-directory abstraction for its second
// occurrence.
#if defined(_MSC_VER) && !defined(__clang__)
#define GLINTFX_BENCH_NOINLINE __declspec(noinline)
#else
#define GLINTFX_BENCH_NOINLINE [[gnu::noinline]]
#endif

namespace glintfx::bench {

namespace {

// Never actually called (the emission this file measures is always
// filtered before build_fields runs) - a builder that WOULD do real
// work if it ran, so an optimizer cannot use its emptiness as a
// reason to fold the call away.
GLINTFX_BENCH_NOINLINE std::span<const glintfx::gltfx_log_field>
unreachable_builder(void *) noexcept {
    static const glintfx::gltfx_log_field fields[] = {
        glintfx::gltfx_log_field{"unreachable", glintfx::gltfx_log_value::make_boolean(true)},
    };
    return std::span<const glintfx::gltfx_log_field>{fields};
}

} // namespace

GLINTFX_BENCH_NOINLINE void suppressed_emit() noexcept {
    // No sink registered in this whole process (the benchmark never
    // calls gltfx_log_set_sink) - every call here takes the "no sink"
    // branch of log_emit()'s own filter.
    glintfx::log_emit(glintfx::gltfx_log_severity::info, "bench", "suppressed",
                      &unreachable_builder, nullptr);
}

} // namespace glintfx::bench
