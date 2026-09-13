// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// alloc_counter_snapshot.hpp - CONTAINER-LEAK-COUNTER sub-fatia S4
// (/var/tmp/glintfx-plan/leak-counter.md sec. 5 S4, GODS_LAWS.md
// L-33/L-34): declares the ONE public entry point tests/container/
// alloc_counter_hook.cpp (S2) exposes besides the replaced global
// allocation operators themselves - a MID-PROCESS read of the live
// counters, for a fixture that needs to observe growth ACROSS several
// open/close cycles of the SAME process (alloc_cycle_growth_smoke.cpp),
// never waiting for the atexit-registered print_report() (S2), which
// only ever runs once, at the very end of the process.
//
// WHY A SEPARATE HEADER, NOT ADDED TO alloc_counter_classify.hpp: that
// header is S1's own contract (classify_allocation_frames(), pure
// arithmetic, shared with its own unit test on all five platforms,
// never touching backtrace()/atomics/threads). This one is S2's own
// contract instead - a live process's own counters, container-only
// (GODS_LAWS.md L-33: one atom, one reason to change; the two never
// share a header just because they live in the same directory).
//
// NOT THREAD-SAFE BEYOND WHAT THE COUNTERS THEMSELVES ALREADY ARE:
// each field is read via a single relaxed atomic load inside the .cpp
// (same memory order print_report() already uses for the same fields)
// - a snapshot taken while another thread is mid-allocation can
// undercount that ONE in-flight block by one. alloc_cycle_growth_
// smoke.cpp (S4) only ever calls this between its own close() calls,
// after every Wayland/EGL resource that fixture itself opened has
// already been torn down - there is no allocation this fixture's own
// thread could still have in flight at that point, and third-party
// worker threads (llvmpipe) racing a snapshot by one block is exactly
// the kind of noise the growth METRIC (delta across two snapshots,
// never a single snapshot's absolute value) is built to absorb.

namespace glintfx_leak_counter {

// Only the two LIVE counts a consumer of this header needs today
// (alloc_cycle_growth_smoke.cpp reads only third_party; live_ours
// rides along for free - same two fields print_report()'s own MEASURED
// alloc_live_ours/alloc_live_third_party lines already carry, GODS_
// LAWS.md L-39: no third field added until something actually reads
// it).
struct alloc_snapshot {
    std::uint64_t live_ours;
    std::uint64_t live_third_party;
};

// Defined in alloc_counter_hook.cpp. Every fixture that links alloc_
// counter_hook.cpp (all nineteen, tests/container/Containerfile) gets
// this symbol whether it calls it or not - same shape as the operator
// new/delete replacements themselves, which every fixture also links
// unconditionally.
[[nodiscard]] alloc_snapshot alloc_counter_snapshot() noexcept;

} // namespace glintfx_leak_counter
