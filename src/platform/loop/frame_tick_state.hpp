// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/time.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>

// platform/loop/frame_tick_state.hpp - LOOP-RUN fatia 6a (docs/plano-
// w6b-fatias-6-8.md D-W6b-42/44/45, GODS_LAWS.md L-17/L-19/L-20): the
// ONE pure atom that turns "the previous tick's own bookkeeping, plus
// what just happened" into the next gltfx_frame_tick a consumer reads
// - no display, no window, no wl_display, no HWND reachable from this
// header (the same "sem SO" property src/platform/wayland/frame_
// callback_sequence.hpp's own header comment already establishes one
// fatia over): this atom is handed EVERYTHING it needs as plain
// arguments, and a later fatia's own loop_facade.cpp is the only
// caller that ever supplies them from a real pump/probe.
//
// P4 LIVES HERE, EXACTLY (docs/plano-w6b-fatias-6-8.md sec. 3.2): a
// tick's own `should_render` is NOT the plain equality
// `last_present != skipped_hidden` an earlier draft of the loop's own
// plan asserted (docs/plano-w6b-fatias-6-8.md finding F4) - that
// equality never lets the loop recover once a window has been hidden,
// because nothing in it ever asks whether the window came back.
// `probe_says_visible` is that missing question, supplied by the
// caller (a later fatia's own gl_context adapter probe, D-W6b-46) -
// this atom's own job is only the two IMPLICATIONS the header comment
// on gltfx_frame_tick::should_render (platform/loop/loop.hpp) names,
// proved as a formula, never as a lookup table:
//
//   should_render = (last_present != skipped_hidden) || probe_says_visible
//
// which gives, by construction: `last_present == presented` always
// forces `should_render == true` (the first disjunct alone decides
// it), and `should_render == false` can only happen when BOTH
// disjuncts are false, which means `last_present == skipped_hidden`
// (the second promise the header names). `last_present` itself is
// never re-derived - it is copied straight through from the caller's
// own argument, independent of `should_render` (a tick CAN report
// `should_render == true` and `last_present == skipped_hidden` in the
// same call: the window was hidden last time and is visible again NOW,
// so this tick should draw, but the STORED last_present has not been
// overwritten yet - only the next present() call does that).

namespace glintfx::platform {

// Builds the next gltfx_frame_tick from the previous tick's own
// bookkeeping and what the caller measured just now:
//
//   previous_now         - the gltfx_time_point the PREVIOUS tick
//                           closed on (elapsed is measured from here).
//   now                   - the fresh gltfx_now() reading THIS tick
//                           closes on.
//   previous_frame_index  - the PREVIOUS tick's own frame_index (0
//                           before the very first tick this loop ever
//                           produces - the returned tick's own
//                           frame_index is always this plus one, so
//                           the first tick reports 1, per P2).
//   last_present          - what the last present() call actually
//                           returned (gltfx_present_outcome::presented
//                           before the very first one).
//   probe_says_visible    - whether the display adapter's own probe
//                           (a later fatia) currently says the window
//                           is NOT hidden - see this header's own top
//                           comment for the exact formula this feeds.
//
// TOTAL: every argument is plain data, there is no invalid combination
// this function refuses - a caller that supplies a `now` earlier than
// `previous_now` (a clock stepping backward) gets `elapsed == 0`, the
// same "no direction, no meaningful span, never UB" rule core/time.
// hpp's own D8 paragraph already applies one layer down, inside
// gltfx_duration_between() itself; this atom then additionally clamps
// the OTHER direction (an `elapsed` larger than
// glintfx::k_gltfx_max_frame_elapsed, platform/loop/loop.hpp) that
// gltfx_duration_between() alone has no opinion about.
[[nodiscard]] gltfx_frame_tick compute_frame_tick(gltfx_time_point previous_now,
                                                  gltfx_time_point now,
                                                  std::uint64_t previous_frame_index,
                                                  gltfx_present_outcome last_present,
                                                  bool probe_says_visible) noexcept;

} // namespace glintfx::platform
