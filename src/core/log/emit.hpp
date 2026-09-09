// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <string_view>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/export.hpp>

// core/log/emit.hpp - CL-4/CL-5 of CORE-LOG (GODS_LAWS.md L-19: "o
// header nasce interno"). NOT under include/glintfx/: no public
// header ever declares this, so a consumer can never call it
// directly - the library itself is the only caller (CL-7's GPU-kind
// atom is the first real one). "Implementação interna da emissão" is
// explicitly NOT frozen ABI (plano/core-log.md section 3) - this
// signature is free to change without notice, and CL-5 exercises
// exactly that freedom (see "DEFERRED FIELD CONSTRUCTION" below).
//
// EXPORTED (GLINTFX_API) ANYWAY, for one reason only: this test
// suite's own log_sink_test.cpp/log_no_alloc_test.cpp need to drive
// an emission against the EXACT SAME registry gltfx_log_set_sink/
// get_sink (sink.hpp, public) manipulate, and those two live inside
// glintfx_library's own compiled object code. Recompiling sink.cpp a
// second time into a test binary - the trick gfss_color_parse_test.cpp
// uses for ITS OWN private, non-exported functions - would instead
// give the test a SECOND, disconnected copy of the static registry,
// with no way to observe what gltfx_log_set_sink just registered.
// Exporting the functions that read it is the smaller compromise.
//
// FAST PATH (Q4): `log_would_emit()` is a load-plus-two-comparisons
// query - no field is ever touched to answer it.
//
// DEFERRED FIELD CONSTRUCTION (CL-5, plano/core-log.md Q4: "carregar
// ponteiro → nulo? volta → severity < minimum? volta → montar campos
// na pilha → chamar"): CL-4's own first cut took `fields` as an
// ALREADY-BUILT std::span, which cannot prove this ordering - by the
// time log_emit() runs, the CALLER has already paid to build the
// array, filtered or not. `log_field_builder_fn` below is called ONLY
// AFTER the severity check passes (see sink.cpp's own log_emit()):
// the caller hands over a FUNCTION, not a value, exactly the same
// "plain pointer + opaque context" shape gltfx_log_sink_fn already
// uses (D-LOG-1) - and log_emit() decides whether to ever call it.
// tests/log_no_alloc_test.cpp's mutation (swap the two lines inside
// log_emit()) is what PROVES this ordering is real, not merely
// documented.

namespace glintfx {

using log_field_builder_fn = std::span<const gltfx_log_field> (*)(void *builder_context) noexcept;

[[nodiscard]] GLINTFX_API bool log_would_emit(gltfx_log_severity severity) noexcept;

// `build_fields == nullptr` means "no fields" (R4-style absent input,
// never a required non-null contract) - the common case for an event
// with nothing to attach, and what log_sink_test.cpp's own
// `emit_sample()` helper uses.
GLINTFX_API void log_emit(gltfx_log_severity severity, std::string_view category,
                          std::string_view name, log_field_builder_fn build_fields,
                          void *builder_context) noexcept;

} // namespace glintfx
