// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

// core/log/event_data.hpp - CL-3 of CORE-LOG (GODS_LAWS.md L-19: "o
// header nasce interno"). The REAL layout gltfx_log_event
// (include/glintfx/core/log/event.hpp) hides behind a pointer -
// PRIVATE, not installed, not exported. Only this library's own
// sources (and tests that add this same source directory as a
// PRIVATE include dir, the way tests/CMakeLists.txt already does for
// every other src/*/*.hpp private header) can construct one of these.
//
// A plain, non-owning aggregate: every member is a view into storage
// someone ELSE owns - see event.hpp's own header comment for the
// full lifetime rule (valid only during the sink call).

namespace glintfx {

struct gltfx_log_event_data {
    // Default member initializers, not a user-declared constructor
    // (aggregate-init stays available at every construction site) -
    // the same cppcheck uninitMemberVarNoCtor fix named_colors.hpp's
    // own header comment already documents.
    gltfx_log_severity severity = gltfx_log_severity::unknown;
    std::string_view category;
    std::string_view name;
    std::span<const gltfx_log_field> fields;
};

} // namespace glintfx
