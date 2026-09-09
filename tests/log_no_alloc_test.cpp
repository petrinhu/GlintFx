// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <span>
#include <string_view>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/core/log/value.hpp>

#include "core/log/emit.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_no_alloc_test.cpp - CL-5 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-19/L-20/L-26/L-40): the hot-path cost promise, plano/core-log.md
// Q4 - harness technique borrowed directly from err_no_alloc_test.cpp
// (CE-2 of CORE-ERROR): replace the GLOBAL operator new/delete for
// this executable (glintfx_add_test() gives every test case its OWN
// binary, so there is no ODR collision with another TU defining these
// same symbols) and COUNT.
//
// TWO CLAIMS, both proven by counting, neither by reading the source:
//
// 1. ZERO ALLOCATION, with AND without a registered sink, for an
//    event carrying five fields including one text field - the
//    richest field set this fatia's own types support (value.hpp:
//    text/signed/unsigned/floating/boolean).
//
// 2. FILTER BEFORE BUILD (Q4's own sequence, "carregar ponteiro →
//    nulo? volta → severity < minimum? volta → montar campos na
//    pilha → chamar"): build_fields is NEVER called for an emission
//    that gets filtered out - proven by a builder that counts its own
//    calls, and PROVEN AS A REAL DEPENDENCY, not just documentation,
//    by a mutation swapping the two lines inside log_emit()
//    (sink.cpp) in a copy OUTSIDE this tracked tree (GODS_LAWS.md
//    L-27) - see the CORE-LOG service order's own report for the
//    captured red output; not repeated here because it never touches
//    a tracked file.
//
// The NANOSECOND COST of a suppressed emission is measured separately
// (molde CE-7, tools/bench/core_log_cost.cpp, two translation units so
// the compiler cannot fold the call into a compile-time constant the
// way a single-TU version let it for CORE-ERROR's own CE-7) - the
// number is recorded in TODO.md, never here (this header, like every
// public header in this project, does not record a number that ages).

namespace {

std::size_t g_alloc_count = 0;
std::size_t g_dealloc_count = 0;

void reset_counts() {
    g_alloc_count = 0;
    g_dealloc_count = 0;
}

} // namespace

void *operator new(std::size_t size) {
    ++g_alloc_count;
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void *p) noexcept {
    ++g_dealloc_count;
    std::free(p);
}

void operator delete(void *p, std::size_t /*size*/) noexcept { ::operator delete(p); }

void operator delete[](void *p) noexcept { ::operator delete(p); }

void operator delete[](void *p, std::size_t /*size*/) noexcept { ::operator delete(p); }

using glintfx::gltfx_log_event;
using glintfx::gltfx_log_field;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::gltfx_log_value;
using glintfx::log_emit;

namespace {

void reset_sink() { gltfx_log_set_sink(gltfx_log_sink{}); }

// The richest field set CL-2's own types support - five fields, one
// of them text (the kind most likely to tempt a future implementation
// into formatting/allocating).
std::span<const gltfx_log_field> five_field_builder(void * /*builder_context*/) noexcept {
    static const std::array<gltfx_log_field, 5> fields = {
        gltfx_log_field{"category", gltfx_log_value::make_text("platform.gl")},
        gltfx_log_field{"index", gltfx_log_value::make_unsigned_integer(3)},
        gltfx_log_field{"offset", gltfx_log_value::make_signed_integer(-1)},
        gltfx_log_field{"score", gltfx_log_value::make_floating(0.5)},
        gltfx_log_field{"retryable", gltfx_log_value::make_boolean(true)},
    };
    return std::span<const gltfx_log_field>{fields};
}

void quiet_sink(void * /*sink_context*/, const gltfx_log_event & /*event*/) noexcept {}

} // namespace

GLINTFX_TEST(log_emit_never_allocates_without_a_sink) {
    reset_sink();
    reset_counts();

    log_emit(gltfx_log_severity::error, "core", "five_fields", &five_field_builder, nullptr);

    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;
    std::println("log_no_alloc_test (no sink): {} allocation(s), {} deallocation(s)",
                 final_alloc_count, final_dealloc_count);
    GLINTFX_CHECK(final_alloc_count == 0);
    GLINTFX_CHECK(final_dealloc_count == 0);
}

GLINTFX_TEST(log_emit_never_allocates_with_a_sink) {
    reset_sink();
    gltfx_log_set_sink(gltfx_log_sink{&quiet_sink, nullptr, gltfx_log_severity::trace});
    reset_counts();

    log_emit(gltfx_log_severity::error, "core", "five_fields", &five_field_builder, nullptr);

    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;
    std::println("log_no_alloc_test (with sink): {} allocation(s), {} deallocation(s)",
                 final_alloc_count, final_dealloc_count);
    GLINTFX_CHECK(final_alloc_count == 0);
    GLINTFX_CHECK(final_dealloc_count == 0);
    reset_sink();
}

namespace {

int g_build_calls = 0;

std::span<const gltfx_log_field> counting_builder(void *builder_context) noexcept {
    ++*static_cast<int *>(builder_context);
    return std::span<const gltfx_log_field>{};
}

} // namespace

GLINTFX_TEST(log_emit_never_builds_fields_for_a_filtered_emission) {
    reset_sink(); // no sink at all
    g_build_calls = 0;
    log_emit(gltfx_log_severity::critical, "core", "filtered-no-sink", &counting_builder,
             &g_build_calls);
    GLINTFX_CHECK(g_build_calls == 0);

    gltfx_log_set_sink(gltfx_log_sink{&quiet_sink, nullptr, gltfx_log_severity::critical});
    g_build_calls = 0;
    log_emit(gltfx_log_severity::info, "core", "filtered-below-minimum", &counting_builder,
             &g_build_calls);
    GLINTFX_CHECK(g_build_calls == 0);
    reset_sink();
}
