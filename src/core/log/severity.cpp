// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/log/severity.hpp>

// core/log/severity.cpp - CL-1 of CORE-LOG. ONE table (GODS_LAWS.md
// L-17: "table of data once", the same discipline err_code.cpp's own
// name lookup already follows), not a switch that grows a case per
// caller.

namespace glintfx {

std::string_view gltfx_log_severity_name(gltfx_log_severity severity) noexcept {
    switch (severity) {
    case gltfx_log_severity::unknown:
        return "unknown";
    case gltfx_log_severity::trace:
        return "trace";
    case gltfx_log_severity::debug:
        return "debug";
    case gltfx_log_severity::info:
        return "info";
    // CORE-LOG-CI DEFEITO 3: the ENUMERATOR names are `warn`/`err`
    // (severity.hpp's own header comment), but the TEXT stays
    // "warning"/"error" - identifier and printed string are
    // deliberately decoupled here, the same way spdlog's own
    // `level::warn`/`level::err` still print "warning"/"error".
    case gltfx_log_severity::warn:
        return "warning";
    case gltfx_log_severity::err:
        return "error";
    case gltfx_log_severity::critical:
        return "critical";
    }
    // Unnamed value (R4): a newer glintfx's severity this build's
    // table has never seen. Degrades, never undefined behavior.
    return "unknown";
}

} // namespace glintfx
