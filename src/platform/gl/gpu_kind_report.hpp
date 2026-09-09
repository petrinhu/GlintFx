// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <glintfx/platform/gl/gpu.hpp>

// platform/gl/gpu_kind_report.hpp - CL-7 of CORE-LOG (TODO.md W5,
// GODS_LAWS.md L-04/L-17/L-19/L-22): the FIRST real emission this
// project's structured-log recebedor carries (plano/core-log.md
// Q6.2/D-LOG-8) - a consumer's own recebedor learns WHICH gpu kind
// this run resolved to and, crucially, WHY (which of the three vias
// answered), information the plain `gltfx_gpu_kind` return value
// (gpu.hpp) never carried on its own.
//
// WRAPS gpu_kind_seam.hpp's own two pure functions, NEVER duplicates
// their logic (GODS_LAWS.md L-17, "table of data once"):
// resolve_gpu_kind_report() below calls resolve_kernel_and_exclusion()
// and resolve_memory_separation() with the EXACT SAME inputs a caller
// already has (both adapters already compute kernel_kind,
// enumeration_index, the exclusion-candidate list, definitely_not_
// software and the via-2 optional, on the way to their own existing
// `return {kind, enumeration_index};`) - this file only adds the
// BOOKKEEPING of which via actually answered, mirroring the exact
// branching those two functions already use internally (sink.cpp's
// own header comment, sec. "gpu_kind_seam.cpp", is the source of
// truth for that branching; this file is the ONLY place outside it
// that reproduces the ORDER of the three checks, never their result).
//
// PLATFORM-AGNOSTIC ON PURPOSE (same reason gpu_kind_seam.hpp already
// gives, one directory over): BOTH adapters - wayland/egl_context_
// adapter.cpp and win32/wgl_context_adapter.cpp - call the SAME two
// functions below, at the SAME point (immediately before their own
// `return {kind, enumeration_index};`), so paridade (GODS_LAWS.md
// L-04) is guaranteed by the COMPILER sharing one definition, never
// by two files read side by side. `git grep -n '#if.*_WIN32'` on this
// file (and its .cpp) must stay empty.
//
// gpu_kind_path is NOT gltfx_log_severity/gltfx_err_code: it is
// purely internal bookkeeping (never declared under include/glintfx/,
// never crosses the ABI as a value) - the CONSUMER only ever sees its
// NAME as the log event's "path" text field (gpu_kind_report.cpp),
// never this enum itself. No append-only contract needed: changing
// this enum's values never breaks a consumer's compiled code, because
// no consumer-facing symbol has this type.

namespace glintfx::platform {

enum class gpu_kind_path : std::uint8_t {
    unknown,
    kernel,
    exclusion,
    memory_separation,
};

struct gpu_kind_report {
    gltfx_gpu_kind kind;
    gpu_kind_path path;
};

// Pure - no I/O, no SO call, same testability class as gpu_kind_seam.
// hpp's own two functions (gpu_kind_report_test.cpp's own header
// comment enumerates the 4 control-flow paths this answers).
[[nodiscard]] gpu_kind_report
resolve_gpu_kind_report(gltfx_gpu_kind kernel_kind, std::uint32_t enumeration_index,
                        std::span<const gltfx_gpu_kind> enumeration_kinds,
                        bool definitely_not_software,
                        std::optional<gltfx_gpu_kind> memory_separation_kind) noexcept;

// The one real emission (Q6.2): builds and delivers the
// `gpu_kind_resolved` log event - category "platform.gl", fields
// "kind" (gltfx_gpu_kind's name) and "path" (gpu_kind_path's name).
// Called ONCE per resolution, from BOTH adapters, right where they
// already have the `report` this function needs.
void report_gpu_kind_resolved(gpu_kind_report report) noexcept;

} // namespace glintfx::platform
