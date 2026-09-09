// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/export.hpp>

// core/log/emit.hpp - CL-4 of CORE-LOG (GODS_LAWS.md L-19: "o header
// nasce interno"). NOT under include/glintfx/: no public header ever
// declares this, so a consumer can never call it directly - the
// library itself is the only caller (CL-7's GPU-kind atom is the
// first real one). "Implementação interna da emissão" is explicitly
// NOT frozen ABI (plano/core-log.md section 3) - this signature is
// free to change without notice.
//
// EXPORTED (GLINTFX_API) ANYWAY, for one reason only: this test
// suite's own log_sink_test.cpp needs to drive an emission against
// the EXACT SAME registry gltfx_log_set_sink/get_sink (sink.hpp,
// public) manipulate, and those two live inside glintfx_library's own
// compiled object code. Recompiling sink.cpp a second time into the
// test binary - the trick gfss_color_parse_test.cpp uses for ITS OWN
// private, non-exported functions - would instead give the test a
// SECOND, disconnected copy of the static registry, with no way to
// observe what gltfx_log_set_sink just registered. Exporting the one
// function that reads it is the smaller compromise.
//
// FAST PATH (Q4): no sink registered, or `severity < sink.minimum` -
// both checked BEFORE `fields` is even looked at, let alone copied.

namespace glintfx {

GLINTFX_API void log_emit(gltfx_log_severity severity, std::string_view category,
                          std::string_view name, std::span<const gltfx_log_field> fields) noexcept;

} // namespace glintfx
