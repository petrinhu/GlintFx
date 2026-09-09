// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// core_log_cost_functions.hpp - CL-5 of CORE-LOG (TODO.md, molde
// CE-7): declarations ONLY, no bodies - core_log_cost.cpp (main())
// sees just this file, the body lives in the SEPARATE translation
// unit core_log_cost_functions.cpp. Same reason CE-7's own sibling
// header states: a single-TU version lets the compiler's
// interprocedural constant propagation see straight through a
// trivial, noinline-marked function and fold the whole timing loop
// into a closed-form constant, timing nothing.

namespace glintfx::bench {

// Calls log_emit() for an event that gets FILTERED OUT (severity
// below the registered sink's minimum) - what this measures IS the
// "carregar ponteiro -> nulo? volta -> severity < minimum? volta"
// path (plano/core-log.md Q4), never the field-building or sink-call
// paths (those never run when this returns).
void suppressed_emit() noexcept;

} // namespace glintfx::bench
