// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/export.hpp>

// core/log/sink.hpp - CL-4 of CORE-LOG (TODO.md W5, GODS_LAWS.md
// L-19/L-22/L-26): the public recebedor of structured log events -
// plano/core-log.md Q1/Q5, decisions D-LOG-1 and D-LOG-5.
//
// PLAIN FUNCTION POINTER + OPAQUE CONTEXT (D-LOG-1), NOT
// std::function: this recebedor can be REGISTERED, TRADED and
// INVOKED from any thread at any time (Q5), and a consumer's own
// binding (C, Rust via cbindgen, anything else) needs a plain,
// C-callable pointer, not a C++ standard-library type whose ABI is
// this project's own to freeze (the same reasoning gltfx_log_value,
// value.hpp, already gives against a standard-library sum type).
// Precedent in this same codebase: `gltfx_node_class_visitor_fn`
// (include/glintfx/gfui/node_view.hpp) is function-pointer-plus-
// context for exactly this reason; `gltfx_loop_callbacks`
// (platform/loop/loop.hpp) went the OTHER way with std::function, and
// that header's own comment names the cost it accepted - a cost this
// sink deliberately does NOT pay, because unlike a loop callback (set
// once, for one thread, at the moment gltfx_loop::run() is called), a
// log sink can be REPLACED WHILE THE PROGRAM IS RUNNING, from ANY
// thread, which std::function cannot do atomically.

namespace glintfx {

using gltfx_log_sink_fn = void (*)(void *sink_context, const gltfx_log_event &event) noexcept;

struct gltfx_log_sink {
    // nullptr = no sink registered (Q1) - gltfx_log_set_sink with this
    // left as nullptr REMOVES whatever was registered before (Q5).
    gltfx_log_sink_fn function = nullptr;
    void *context = nullptr;
    // Emissions below this severity never reach `function` - checked
    // BEFORE any field is built (Q4's cheap-when-disabled promise).
    gltfx_log_severity minimum = gltfx_log_severity::info;
};

// Registers `sink`, returning whatever was registered before (GLFW's
// own error-callback convention, plano/core-log.md 1.1) - never fails
// (no allocation, GODS_LAWS.md L-07/Q4), so this is not a
// gltfx_rslt<T>: R1 covers FALLIBLE calls, and this one cannot fail.
//
// PROMISE (D-LOG-5, Q5): the swap is atomic (one pointer). After this
// call returns, no NEW emission reaches the sink that was just
// replaced - but a call already in flight on ANOTHER thread may still
// be running against it. This library never holds a lock while
// calling `sink.function` (SDL issue #2463, plano/core-log.md 1.2: a
// mutex held around the consumer's own callback serializes every
// emitting thread behind the slowest sink and forces a recursive
// lock the moment a sink itself logs) - a consumer using glintfx from
// more than one thread must make its own sink safe to call
// concurrently, and must not destroy `context` until it is certain no
// emission is still in flight (in practice: replace the sink before
// creating, or after destroying, whatever `context` points at).
// Not [[nodiscard]] (unlike the accessor below): most callers
// register a sink to set it going forward and have no use for the
// previous one (GLFW's glfwSetErrorCallback, the same shape, is not
// force-captured either) - a caller that DOES want to restore the
// previous registration later is still free to capture the result.
GLINTFX_API gltfx_log_sink gltfx_log_set_sink(gltfx_log_sink sink) noexcept;

[[nodiscard]] GLINTFX_API gltfx_log_sink gltfx_log_get_sink() noexcept;

} // namespace glintfx
