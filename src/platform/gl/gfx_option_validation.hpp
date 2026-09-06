// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

// platform/gl/gfx_option_validation.hpp - C-OPT (docs/plano-w6b-placa-
// e-laco.md fatia 2a, D-W6b-16, GODS_LAWS.md L-19/L-22): the ONE pure
// atom every entry point that accepts a gltfx_gfx_option_entry funnels
// through BEFORE any adapter (fatias 3/4) ever sees it - the opening
// list (gltfx_gl_context_desc::options, fatia 2b) and the live update
// (gltfx_gl_context::set_option(), fatia 2b) both call this, with
// `already_open` telling the two apart.
//
// FOUR REASONS AN ENTRY IS REFUSED, ALL gltfx_err_code::invalid_
// argument WITH rejected_value() NAMING THE OPTION (never the numeric
// id, never a sentence - docs/api-conventions.md R7):
//
//   1. `id` outside this build's own table (rejected_value carries the
//      raw numeric id, decimal, since there is no name to report for
//      an id nobody registered).
//   2/3. `value` outside [min_value, max_value] - below or above.
//   4. `when == read_only` - never accepted from a consumer, in
//      EITHER call shape (D-W6b-16 (3): the library writes these,
//      the consumer only ever reads them back).
//   5. `when == open_only` AND `already_open == true` - the option
//      belongs in the list a context opens WITH, never in a later
//      set_option() call (fatia 2b's own gl_context_desc_validation.
//      cpp already recuses this same shape for `swap_interval`;
//      D-W6b-25, the second-context-same-window rule, layers ON TOP
//      of this, at the fachada, not here).
//
// A `live` option is accepted in BOTH call shapes on purpose: a
// consumer may set one as part of the opening list too (D-W6b-17's own
// descriptor accepts any mix), or later through set_option() - both
// are the SAME option, at the SAME point in its own lifecycle.

namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void> validate_gfx_option_entry(const gltfx_gfx_option_entry &entry,
                                                         bool already_open) noexcept;

} // namespace glintfx::platform
