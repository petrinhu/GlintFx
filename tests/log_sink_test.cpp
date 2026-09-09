// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/sink.hpp>

// PRIVATE header (GODS_LAWS.md L-19): log_emit() is how THIS library's
// own sources trigger an emission - a consumer never calls it (no
// public header declares it). Exported (GLINTFX_API) purely so this
// test can call the SAME registry gltfx_log_set_sink/get_sink (also
// declared in the public sink.hpp, above) manipulate - all three live
// in the ONE already-linked copy inside glintfx::glintfx, never a
// second, disconnected copy recompiled into this test binary (the
// gfss tests' "compile the .cpp again" trick would break EXACTLY that
// property here, since it would give this test its own separate
// static registry with no line to the one gltfx_log_set_sink writes).
#include "core/log/emit.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_sink_test.cpp - CL-4 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-19/L-20/L-26/L-40): gltfx_log_set_sink/gltfx_log_get_sink and
// delivery through log_emit(), plano/core-log.md Q1/Q5/D-LOG-1/D-LOG-5.

using glintfx::gltfx_log_event;
using glintfx::gltfx_log_get_sink;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::log_emit;

namespace {

// Every case resets the process-wide registry first (GODS_LAWS.md
// L-40: state left over from a previous case is not a clean slate) -
// `function == nullptr` IS the documented removal (D-LOG spec, Q5).
void reset_sink() { gltfx_log_set_sink(gltfx_log_sink{}); }

struct call_record {
    int count = 0;
    std::vector<std::string_view> names;
    const void *last_context = nullptr;
};

void recording_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *record = static_cast<call_record *>(sink_context);
    ++record->count;
    record->names.push_back(event.name());
    record->last_context = sink_context;
}

void emit_sample(gltfx_log_severity severity, std::string_view name) {
    // nullptr build_fields: no fields, see emit.hpp's own comment.
    log_emit(severity, "core", name, nullptr, nullptr);
}

} // namespace

GLINTFX_TEST(log_sink_no_sink_registered_calls_nothing) {
    reset_sink();
    // No crash, no observable side effect - there is nothing to
    // assert ON except that this does not throw or fault.
    emit_sample(gltfx_log_severity::err, "unreachable");
    GLINTFX_CHECK(true);
}

GLINTFX_TEST(log_sink_registered_is_called_once_per_emission_in_order) {
    reset_sink();
    call_record record;
    gltfx_log_set_sink(gltfx_log_sink{&recording_sink, &record, gltfx_log_severity::info});

    emit_sample(gltfx_log_severity::info, "first");
    emit_sample(gltfx_log_severity::warn, "second");

    GLINTFX_CHECK(record.count == 2);
    GLINTFX_CHECK(record.names.size() == 2);
    GLINTFX_CHECK(record.names[0] == std::string_view{"first"});
    GLINTFX_CHECK(record.names[1] == std::string_view{"second"});
    reset_sink();
}

GLINTFX_TEST(log_sink_set_returns_the_previous_registration) {
    reset_sink();
    call_record record_a;
    call_record record_b;
    const gltfx_log_sink first{&recording_sink, &record_a, gltfx_log_severity::info};
    const gltfx_log_sink second{&recording_sink, &record_b, gltfx_log_severity::warn};

    const gltfx_log_sink initial_previous = gltfx_log_set_sink(first);
    GLINTFX_CHECK(initial_previous.function == nullptr); // reset_sink() above

    const gltfx_log_sink previous = gltfx_log_set_sink(second);
    GLINTFX_CHECK(previous.function == &recording_sink);
    GLINTFX_CHECK(previous.context == &record_a);
    GLINTFX_CHECK(previous.minimum == gltfx_log_severity::info);
    reset_sink();
}

GLINTFX_TEST(log_sink_get_reflects_the_current_registration) {
    reset_sink();
    call_record record;
    const gltfx_log_sink sink{&recording_sink, &record, gltfx_log_severity::err};
    gltfx_log_set_sink(sink);

    const gltfx_log_sink observed = gltfx_log_get_sink();
    GLINTFX_CHECK(observed.function == &recording_sink);
    GLINTFX_CHECK(observed.context == &record);
    GLINTFX_CHECK(observed.minimum == gltfx_log_severity::err);
    reset_sink();
}

GLINTFX_TEST(log_sink_null_function_removes_the_sink) {
    reset_sink();
    call_record record;
    gltfx_log_set_sink(gltfx_log_sink{&recording_sink, &record, gltfx_log_severity::info});
    gltfx_log_set_sink(gltfx_log_sink{}); // function == nullptr: removal

    emit_sample(gltfx_log_severity::critical, "should-not-arrive");
    GLINTFX_CHECK(record.count == 0);
    GLINTFX_CHECK(gltfx_log_get_sink().function == nullptr);
}

GLINTFX_TEST(log_sink_minimum_filters_below_and_admits_equal) {
    reset_sink();
    call_record record;
    gltfx_log_set_sink(gltfx_log_sink{&recording_sink, &record, gltfx_log_severity::warn});

    emit_sample(gltfx_log_severity::info, "below-minimum");
    GLINTFX_CHECK(record.count == 0);

    emit_sample(gltfx_log_severity::warn, "at-minimum");
    GLINTFX_CHECK(record.count == 1);
    reset_sink();
}

// extern "C" (RFC 2137 concern, plano/core-log.md 1.2): no capture, no
// C++-only type crossing the boundary beyond the reference itself -
// this is what proves gltfx_log_sink_fn is usable from any binding.
extern "C" void extern_c_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *record = static_cast<call_record *>(sink_context);
    ++record->count;
    record->names.push_back(event.name());
    record->last_context = sink_context;
}

namespace {

struct static_member_sink_owner {
    static void handle(void *sink_context, const gltfx_log_event &event) noexcept {
        auto *record = static_cast<call_record *>(sink_context);
        ++record->count;
        record->names.push_back(event.name());
        record->last_context = sink_context;
    }
};

} // namespace

GLINTFX_TEST(log_sink_extern_c_and_static_member_sinks_work_and_context_is_identical) {
    reset_sink();
    call_record record_c;
    gltfx_log_set_sink(gltfx_log_sink{&extern_c_sink, &record_c, gltfx_log_severity::info});
    emit_sample(gltfx_log_severity::info, "via-extern-c");
    GLINTFX_CHECK(record_c.count == 1);
    GLINTFX_CHECK(record_c.last_context == &record_c);

    call_record record_member;
    gltfx_log_set_sink(gltfx_log_sink{&static_member_sink_owner::handle, &record_member,
                                      gltfx_log_severity::info});
    emit_sample(gltfx_log_severity::info, "via-static-member");
    GLINTFX_CHECK(record_member.count == 1);
    GLINTFX_CHECK(record_member.last_context == &record_member);
    reset_sink();
}
