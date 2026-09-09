// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/core/log/value.hpp>

#include "core/log/emit.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_sink_concurrency_test.cpp - CL-6 of CORE-LOG (TODO.md,
// GODS_LAWS.md L-19/L-20/L-26/L-40): plano/core-log.md Q5/D-LOG-5's
// concurrency promise - one atomic pointer, no mutex around the
// consumer's own callback, ordered delivery PER THREAD, atomic swap,
// reentrancy delivered.
//
// DOWNGRADE DECLARED (plano/core-log.md CL-6 row, verbatim): "não há
// TSan nos portões; a prova de ausência de data race é por desenho
// (um std::atomic de ponteiro) e por ASan, não por TSan." This test
// runs under the `sanitizer` CI job's ASan/UBSan build - it catches
// memory-safety bugs (use-after-free, heap corruption, misaligned
// atomics), not the class of pure data-race bugs only TSan detects.
// The registry's own residual limitation (a slot in the fixed-size
// ring reused far enough in the future to race a very slow in-flight
// reader) is documented in sink.cpp's own header comment and is not
// what this test tries to catch - it exercises realistic concurrency
// (a handful of registrations, many emissions), not registry
// exhaustion.

using glintfx::gltfx_log_event;
using glintfx::gltfx_log_field;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::gltfx_log_value;
using glintfx::log_emit;

namespace {

void reset_sink() { gltfx_log_set_sink(gltfx_log_sink{}); }

// PER-THREAD MONOTONICITY: each field carries the emitting thread's
// numeric id and its own sequence number - the sink's bookkeeping
// (this struct) is allowed to use a mutex (it is the CONSUMER's own
// code, not the library's), the LIBRARY itself never holds one around
// the call (D-LOG-5).
struct ordering_record {
    std::mutex guard;
    std::unordered_map<std::uint64_t, std::uint64_t> last_seen_per_thread; // thread_id -> last seq
    std::uint64_t total_calls = 0;
    bool monotonicity_violated = false;
};

std::uint64_t field_value(const gltfx_log_event &event, std::string_view name) noexcept {
    for (const gltfx_log_field &f : event.fields()) {
        if (f.name == name) {
            return f.value.unsigned_integer();
        }
    }
    return 0;
}

void ordering_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *record = static_cast<ordering_record *>(sink_context);
    const std::uint64_t thread_id = field_value(event, "thread_id");
    const std::uint64_t seq = field_value(event, "seq");

    const std::lock_guard<std::mutex> lock(record->guard);
    ++record->total_calls;
    auto [it, inserted] = record->last_seen_per_thread.try_emplace(thread_id, seq);
    if (!inserted) {
        if (seq <= it->second) {
            record->monotonicity_violated = true;
        }
        it->second = seq;
    }
}

std::span<const gltfx_log_field> pair_builder(void *builder_context) noexcept {
    // Storage lives in the CALLING thread's own stack frame (the
    // caller of log_emit, one level up) - safe: log_emit is synchronous,
    // this builder never outlives the call that invoked it.
    return *static_cast<std::span<const gltfx_log_field> *>(builder_context);
}

} // namespace

GLINTFX_TEST(log_sink_concurrent_emissions_preserve_per_thread_order) {
    reset_sink();
    ordering_record record;
    gltfx_log_set_sink(gltfx_log_sink{&ordering_sink, &record, gltfx_log_severity::trace});

    constexpr int k_threads = 8;
    constexpr std::uint64_t k_events_per_thread = 2000;

    std::vector<std::thread> workers;
    workers.reserve(k_threads);
    for (int t = 0; t < k_threads; ++t) {
        workers.emplace_back([t] {
            const auto thread_id = static_cast<std::uint64_t>(t);
            for (std::uint64_t seq = 0; seq < k_events_per_thread; ++seq) {
                const std::array<gltfx_log_field, 2> fields = {
                    gltfx_log_field{"thread_id", gltfx_log_value::make_unsigned_integer(thread_id)},
                    gltfx_log_field{"seq", gltfx_log_value::make_unsigned_integer(seq)},
                };
                std::span<const gltfx_log_field> span_view{fields};
                log_emit(gltfx_log_severity::info, "concurrency", "seq_event", &pair_builder,
                         &span_view);
            }
        });
    }
    for (std::thread &w : workers) {
        w.join();
    }

    GLINTFX_CHECK(!record.monotonicity_violated);
    GLINTFX_CHECK(record.total_calls ==
                  static_cast<std::uint64_t>(k_threads) * k_events_per_thread);
    reset_sink();
}

namespace {

struct swap_record {
    std::atomic<std::uint64_t> count{0};
};

void counting_sink(void *sink_context, const gltfx_log_event & /*event*/) noexcept {
    static_cast<swap_record *>(sink_context)->count.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

// D-LOG-5's promise: after gltfx_log_set_sink() RETURNS, no NEW
// emission reaches the sink it just replaced. A call already in
// flight when the swap happens may still complete against the old
// sink - this test only checks the state AFTER emitting threads have
// been joined (long past any such in-flight window), the same
// declared scope sink.hpp's own header comment gives the promise.
GLINTFX_TEST(log_sink_swap_stops_reaching_the_old_sink) {
    reset_sink();
    swap_record record_a;
    swap_record record_b;
    gltfx_log_set_sink(gltfx_log_sink{&counting_sink, &record_a, gltfx_log_severity::trace});

    std::atomic<bool> stop{false};
    std::thread emitter([&stop] {
        while (!stop.load(std::memory_order_relaxed)) {
            log_emit(gltfx_log_severity::info, "concurrency", "swap_probe", nullptr, nullptr);
        }
    });

    // Let the emitter run a bit against A, then swap to B.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    gltfx_log_set_sink(gltfx_log_sink{&counting_sink, &record_b, gltfx_log_severity::trace});
    const std::uint64_t a_count_at_swap = record_a.count.load(std::memory_order_relaxed);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop.store(true, std::memory_order_relaxed);
    emitter.join();

    // A's count may have grown by a FEW calls already in flight at the
    // exact moment of the swap, but must stabilize immediately after -
    // it cannot keep growing for the whole 20ms B was live.
    const std::uint64_t a_count_final = record_a.count.load(std::memory_order_relaxed);
    GLINTFX_CHECK(a_count_final - a_count_at_swap < 1000); // generous slack, never "kept growing"
    GLINTFX_CHECK(record_b.count.load(std::memory_order_relaxed) > 0);
    reset_sink();
}

namespace {

struct reentrant_record {
    std::atomic<int> outer_calls{0};
    std::atomic<int> inner_calls{0};
};

void reentrant_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *record = static_cast<reentrant_record *>(sink_context);
    if (event.name() == "outer") {
        record->outer_calls.fetch_add(1, std::memory_order_relaxed);
        // Reentrant: emitting FROM WITHIN a sink call - legal because
        // no lock is held during the call (D-LOG-5).
        log_emit(gltfx_log_severity::info, "concurrency", "inner", nullptr, nullptr);
    } else {
        record->inner_calls.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace

GLINTFX_TEST(log_sink_reentrant_emission_from_within_a_sink_is_delivered) {
    reset_sink();
    reentrant_record record;
    gltfx_log_set_sink(gltfx_log_sink{&reentrant_sink, &record, gltfx_log_severity::trace});

    log_emit(gltfx_log_severity::info, "concurrency", "outer", nullptr, nullptr);

    GLINTFX_CHECK(record.outer_calls.load() == 1);
    GLINTFX_CHECK(record.inner_calls.load() == 1);
    reset_sink();
}
