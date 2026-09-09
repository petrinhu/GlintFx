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
//
// CORE-LOG-CI DEFEITO 3 (measured, run 34329543846, jobs "Arch -
// compartilhado"/"Arch - estatico"/"CachyOS - compartilhado"/"CachyOS
// - estatico", 09/09/2026): `warning` collided for real with a system
// macro - `#define warning` in gawk's own `gawkapi.h`
// (cgit.git.savannah.gnu.org/cgit/gawk.git/plain/gawkapi.h), an
// object-like macro active for any translation unit that is not gawk
// itself (`#ifndef GAWK`). Fedora/Ubuntu never surfaced it (no `gawk`
// package on those images); Arch/CachyOS do. `error` did not fail the
// gate (libattr's own `#define error(ctx, args...)` in
// `attr/error_context.h` is function-like, and inert behind an
// opt-in guard, `ERROR_CONTEXT_MACROS`, that ordinary inclusion never
// defines), but sat one active guard away from doing the same thing,
// and was renamed alongside `warning` for that reason - prevention,
// not a second live break (docs/api-conventions.md R6 has the full
// account and the forbidden-name table).
//
// `warn`/`err` were chosen over a prefix (`severity_warning`,
// rejected: repeats the already-qualified `enum class` name, GODS_
// LAWS.md L-39 the other way - lengthening without a legibility gain)
// because `warn` is the majority spelling of this level across the
// field this project already keeps company with (OpenTelemetry
// `WARN`, Go `slog.LevelWarn`, spdlog `warn`, SDL3 `SDL_LOG_PRIORITY_
// WARN`, Rust `log::Level::Warn`), and `err` already IS this
// project's own vocabulary (`gltfx_err`, `gltfx_err_code`, `err.hpp`,
// `gltfx_rslt<T>::err()` - all chosen in CE-1 for the identical
// reason: `error_code` collided with `std::error_code`). Having
// `gltfx_err` beside `gltfx_log_severity::error` was the
// inconsistency; `err` in both places is the coherent form.
//
// THE TEXT A CONSUMER SEES DOES NOT CHANGE: gltfx_log_severity_name()
// below still returns `"warning"`/`"error"` for these two values -
// only the C++ identifier changed, spdlog's own precedent for
// decoupling the two (`level::warn` prints `"warning"`, `level::err`
// prints `"error"`, `common.h` in spdlog's own repository). A
// consumer's already-written log line, and anything grepping its
// output, is unaffected; only source code spelling `gltfx_log_
// severity::warning`/`::error` needs to change, and no published
// glintfx version ever shipped this header (see the commit this
// paragraph was written in for the tag-by-tag check), so this is not
// a break of anything a consumer has already built against.
//
// NAMES PROHIBITED for a future severity level, with the source:
// `warning`, `error` (both above), `fatal`, `nonfatal`, `lintwarn`
// (gawkapi.h, same file, same unconditional guard - `fatal` was the
// natural next name for a level above `critical`; it is unavailable
// before anyone proposes it).
namespace glintfx {

enum class gltfx_log_severity : std::uint32_t { // NOLINT(performance-enum-size) reason: 32 bits
                                                // is a frozen ABI decision (D-LOG-2), matching
                                                // gltfx_err_code (err_code.hpp), not an oversight
    unknown = 0,
    trace = 100,
    debug = 200,
    info = 300,
    warn = 400,
    err = 500,
    critical = 600,
};

// R4: an unrecognized value (this build's table has no name for it)
// degrades to "unknown", never undefined behavior - the same contract
// gltfx_err_code_name() (err_code.hpp) already gives.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_log_severity_name(gltfx_log_severity severity) noexcept;

} // namespace glintfx
