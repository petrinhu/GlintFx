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

namespace glintfx::bench {

namespace {

// Never actually called (the emission this file measures is always
// filtered before build_fields runs) - a builder that WOULD do real
// work if it ran, so an optimizer cannot use its emptiness as a
// reason to fold the call away.
[[gnu::noinline]] std::span<const glintfx::gltfx_log_field> unreachable_builder(void *) noexcept {
    static const glintfx::gltfx_log_field fields[] = {
        glintfx::gltfx_log_field{"unreachable", glintfx::gltfx_log_value::make_boolean(true)},
    };
    return std::span<const glintfx::gltfx_log_field>{fields};
}

} // namespace

[[gnu::noinline]] void suppressed_emit() noexcept {
    // No sink registered in this whole process (the benchmark never
    // calls gltfx_log_set_sink) - every call here takes the "no sink"
    // branch of log_emit()'s own filter.
    glintfx::log_emit(glintfx::gltfx_log_severity::info, "bench", "suppressed",
                      &unreachable_builder, nullptr);
}

} // namespace glintfx::bench
