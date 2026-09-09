// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// core/log/severity.hpp - CL-1 of CORE-LOG (TODO.md W5, GODS_LAWS.md
// L-19/L-22/L-26): the numeric identity of how grave a log event is.
//
// D-LOG-2 (plano/core-log.md Q2), the decision this file executes:
// `uint32_t`, named values with a GAP OF 100 between them, HIGHER
// number = MORE severe, zero = unknown, APPEND-ONLY - never an enum
// with a `COUNT` sentinel. Sources behind this shape, not invented
// here: OpenTelemetry's SeverityNumber (ranges of 4, compared purely
// numerically), Go's slog (gap of 4, explicitly so a third party can
// insert an intermediate level the author never named), and this
// project's own gltfx_err_code (err_code.hpp: append-only, zero as
// the "not recognized by this build's table" sentinel). RFC 5424's
// syslog severities go the OTHER way (0 = most severe) - a known
// footgun this project deliberately does not repeat; the ordering
// here is `>=` in the natural direction, matching OTel/slog/Rust log/
// SDL, not syslog.
//
// A GAP, NOT A DENSE RANGE, is what buys the R4/CE-1 guarantee this
// file's own test proves: a value this build's table does not name
// (say 350, emitted by a NEWER glintfx a consumer has not recompiled
// against) still ORDERS CORRECTLY between its neighbors without the
// consumer knowing what it is, and gltfx_log_severity_name() below
// degrades to "unknown" reading it - never undefined behavior (R4,
// docs/api-conventions.md), the same contract gltfx_err_code_name()
// already gives for an unrecognized error code.
//
// APPEND-ONLY (same contract as gltfx_err_code, err_code.hpp's own
// header comment): once a value ships, its name and number never
// change. A new named level is a NEW enumerator at a NEW number,
// never a renumbering of an existing one.

namespace glintfx {

enum class gltfx_log_severity : std::uint32_t { // NOLINT(performance-enum-size) reason: 32 bits
                                                // is a frozen ABI decision (D-LOG-2), matching
                                                // gltfx_err_code (err_code.hpp), not an oversight
    unknown = 0,
    trace = 100,
    debug = 200,
    info = 300,
    warning = 400,
    error = 500,
    critical = 600,
};

// R4: an unrecognized value (this build's table has no name for it)
// degrades to "unknown", never undefined behavior - the same contract
// gltfx_err_code_name() (err_code.hpp) already gives.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_log_severity_name(gltfx_log_severity severity) noexcept;

} // namespace glintfx
