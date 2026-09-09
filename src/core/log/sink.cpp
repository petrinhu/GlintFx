// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

#include <glintfx/core/log/sink.hpp>

#include "core/log/emit.hpp"
#include "core/log/event_data.hpp"

// core/log/sink.cpp - CL-4 of CORE-LOG: the process-wide sink
// registry and the one internal doorway (log_emit()) that reads it.
//
// MECHANISM (Q4/Q5): the LIVE registration is one
// `std::atomic<const gltfx_log_sink*>`, CONSTANT-initialized to
// nullptr (no dynamic constructor, no static-initialization-order
// dependency - SDL creates its mutex lazily and that IS a source of
// bugs, plano/core-log.md 1.1). Emitting with no sink registered costs
// exactly one atomic load plus a null check, before any field is ever
// touched.
//
// D-LOG-1 FORBIDS ALLOCATING ON REGISTRATION: gltfx_log_set_sink
// cannot call `new`. The atomic pointer therefore never points at
// caller-owned or heap-allocated storage - it points into a FIXED-SIZE
// static ring (`g_sink_slots` below), and each registration copies the
// caller's small, trivially-copyable gltfx_log_sink into the next slot
// before publishing it.
//
// DECLARED RESIDUAL LIMITATION (same spirit as the "no RCU/grace
// period" waiver, plano/core-log.md Q5 - "declaro a regra em vez de
// fingir que o problema não existe"): the ring has a FIXED capacity,
// so a slot can be REUSED by a far-later registration while an
// extremely slow, concurrent log_emit() call on another thread is
// still reading that same slot - a genuine, if vanishingly narrow,
// data race on the registry's OWN internal storage (never on a
// consumer's `context`, which this library never touches except to
// hand its address to `sink.function`). log_emit() below copies the
// whole gltfx_log_sink in ONE step immediately after loading the
// pointer, which shrinks the exposure to a single struct-sized read
// instead of spreading it across the severity check and the call, but
// does not eliminate it. This is not expected to matter in practice
// (it requires ring-capacity-or-more registrations racing a single
// in-flight emission) and is not proven absent by the sanitizer
// coverage this project runs (ASan, not TSan - CL-6's own test
// documents the same downgrade). A future fatia that needs a stronger
// guarantee has a name for the fix already surveyed and rejected here
// for being disproportionate at this scale: hazard pointers or an
// RCU-style grace period.
namespace glintfx {

namespace {

// Arbitrary, generous capacity - registrations are expected to happen
// a handful of times per process lifetime (startup, tests, maybe a
// runtime reconfiguration), never per-frame. Not part of the frozen
// public contract (plano/core-log.md section 3: "implementação
// interna da emissão" is free to change).
constexpr std::size_t k_sink_slot_count = 64;

std::array<gltfx_log_sink, k_sink_slot_count> g_sink_slots{};
std::atomic<std::size_t> g_next_slot{0};
std::atomic<const gltfx_log_sink *> g_current{nullptr};

} // namespace

// gltfx_log_sink is a small (3 pointer-widths), trivially-copyable
// value type (the same shape gltfx_err_code/gltfx_log_severity are),
// and this function's own job is to COPY it into g_sink_slots below -
// by-value is the idiom, not an oversight cppcheck's generic size
// heuristic understands.
// cppcheck-suppress passedByValue
gltfx_log_sink gltfx_log_set_sink(gltfx_log_sink sink) noexcept {
    const gltfx_log_sink *previous = nullptr;
    if (sink.function == nullptr) {
        // Removal (Q5): nothing to store - just point at nothing.
        previous = g_current.exchange(nullptr, std::memory_order_acq_rel);
    } else {
        const std::size_t slot =
            g_next_slot.fetch_add(1, std::memory_order_relaxed) % k_sink_slot_count;
        g_sink_slots[slot] = sink;
        previous = g_current.exchange(&g_sink_slots[slot], std::memory_order_acq_rel);
    }
    return previous != nullptr ? *previous : gltfx_log_sink{};
}

gltfx_log_sink gltfx_log_get_sink() noexcept {
    const gltfx_log_sink *current = g_current.load(std::memory_order_acquire);
    return current != nullptr ? *current : gltfx_log_sink{};
}

namespace {

// Shared by log_would_emit()/log_emit(): loads the registry and
// copies it (see "DECLARED RESIDUAL LIMITATION" above for why this is
// ONE non-atomic copy, not spread reads), returning an EMPTY optional
// when there is no sink or `severity` is below its threshold - the
// one gate both functions answer, so they can never disagree.
std::optional<gltfx_log_sink> snapshot_admitting(gltfx_log_severity severity) noexcept {
    const gltfx_log_sink *current = g_current.load(std::memory_order_acquire);
    if (current == nullptr) {
        return std::nullopt;
    }
    const gltfx_log_sink snapshot = *current;
    if (severity < snapshot.minimum) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace

bool log_would_emit(gltfx_log_severity severity) noexcept {
    return snapshot_admitting(severity).has_value();
}

// CL-5's ordering promise (Q4, emit.hpp's own "DEFERRED FIELD
// CONSTRUCTION" comment) lives in these two lines, in THIS order:
// filter FIRST (snapshot_admitting), montar (call build_fields())
// SECOND - tests/log_no_alloc_test.cpp's mutation swaps them to prove
// the order is load-bearing, not decorative.
void log_emit(gltfx_log_severity severity, std::string_view category, std::string_view name,
              log_field_builder_fn build_fields, void *builder_context) noexcept {
    const std::optional<gltfx_log_sink> snapshot = snapshot_admitting(severity);
    if (!snapshot.has_value()) {
        return;
    }
    const std::span<const gltfx_log_field> fields = build_fields != nullptr
                                                        ? build_fields(builder_context)
                                                        : std::span<const gltfx_log_field>{};
    const gltfx_log_event_data data{severity, category, name, fields};
    const gltfx_log_event event(data);
    snapshot->function(snapshot->context, event);
}

} // namespace glintfx
