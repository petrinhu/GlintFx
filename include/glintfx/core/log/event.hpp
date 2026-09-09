// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/export.hpp>

// core/log/event.hpp - CL-3 of CORE-LOG (TODO.md W5, GODS_LAWS.md
// L-19/L-22/L-26): gltfx_log_event, D-LOG-4's semi-opaque envelope,
// in the mold of gltfx_err (err.hpp).
//
// OPAQUE ON PURPOSE, SAME REASON AS err_context (err.hpp's own header
// comment): gltfx_log_event_data below is only FORWARD-declared here;
// its real layout lives in a PRIVATE header
// (src/core/log/event_data.hpp), included only by this library's own
// translation units. A consumer can never construct one - it only
// ever receives `const gltfx_log_event &` as the sink's own
// parameter (core/log/sink.hpp, CL-4). This is what makes a field
// ADDED to gltfx_log_event_data later purely additive (bumps B): a
// STRUCT VISIBLE to the consumer with a field added would instead
// shift every later field's offset in whatever the consumer already
// compiled (spdlog issue #2454, cited in plano/core-log.md 1.2, is
// exactly this mistake, made for real).
//
// STACK-ALLOCATED, NEVER OWNED (Q4: zero allocation on the hot path):
// this library builds one gltfx_log_event_data on ITS OWN stack frame
// per emission and hands the sink a `const gltfx_log_event &`
// wrapping a pointer to it - gltfx_log_event itself never allocates,
// never owns, never outlives the call. VALID ONLY DURING THE SINK
// CALL (same rule SQLite's errlog.html and GLFW's intro_guide.html
// give their own callbacks, plano/core-log.md 1.1): a consumer who
// wants to keep a field's text or the field list itself must COPY it
// out before returning - the pointer this class carries, and every
// std::string_view/std::span it reads through, become dangling the
// moment the sink call returns.
//
// ONE POINTER WIDE, FROZEN (GODS_LAWS.md L-26): the only member is a
// pointer to an incomplete type, so this class's OWN footprint can
// never grow or shrink by accident - see the static_assert below.

namespace glintfx {

struct gltfx_log_event_data;

class gltfx_log_event {
  public:
    // Only callable where gltfx_log_event_data's real layout is
    // visible (this library's own log/ sources, and this fatia's own
    // tests, which include the private header the same way the gfss
    // tests already include THEIR private headers) - a consumer
    // outside this library never has that, so it can never construct
    // one of these on its own.
    explicit gltfx_log_event(const gltfx_log_event_data &data) noexcept : m_data(&data) {}

    [[nodiscard]] GLINTFX_API gltfx_log_severity severity() const noexcept;
    [[nodiscard]] GLINTFX_API std::string_view category() const noexcept;
    [[nodiscard]] GLINTFX_API std::string_view name() const noexcept;
    [[nodiscard]] GLINTFX_API std::span<const gltfx_log_field> fields() const noexcept;

  private:
    const gltfx_log_event_data *m_data;
};

static_assert(sizeof(gltfx_log_event) == sizeof(void *),
              "gltfx_log_event footprint is frozen ABI, CORE-LOG CL-3");

} // namespace glintfx
