// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/value.hpp>

// value_parse.hpp - GFSS-VALUE (TODO.md, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-27/L-28/L-40): decodes ONE already-tokenized gfss component
// value (a single <ident-token>/<number-token>/<percentage-token>/
// <dimension-token> - GFSS-TOKEN's own token.hpp) into its typed
// glintfx::style::gltfx_gfss_value (value.hpp) - never more than one
// token, never a relative-unit resolution (GFSS-RESOLVE's own job,
// ESCOPO.md SS4's declared boundary), never a multi-token declaration
// (space-separated lists, `!important`, validation against the
// property registry - GFSS-DECL-PARSE's own job, TODO.md: "a
// representacao, em GFSS-VALUE" is this file's half of that split).
//
// INTERNAL IN THIS SLICE, ON PURPOSE - THE TYPE IS PUBLIC, THE PARSER
// IS NOT (GODS_LAWS.md L-19: "o header nasce interno, em src/gfss/"):
// value.hpp's own gltfx_gfss_value/gltfx_gfss_length/gltfx_gfss_value_
// kind/gltfx_gfss_length_unit ARE the [PMU]-frozen public layout this
// fatia's own service order congeals - GFSS-PROP-REGISTRY (TODO.md)
// already depends on that type existing under include/glintfx/.
// parse_value() below, in contrast, lives here, not under include/
// glintfx/, and is not GLINTFX_API - the SAME "design tension,
// deliberately unresolved" color_parse.hpp's own header comment
// already documents for parse_color(): whether the eventual PUBLIC
// entry point takes a gltfx_gfss_token directly (this file's own
// current shape), a gltfx_gfss_cursor, or a std::string_view is GFSS-
// API's call (TODO.md, wave W10), not an implementer's - nothing here
// freezes that.
//
// WHY THIS RETURNS value_parse_result, NOT gltfx_rslt<T> (the SAME
// reasoning color_parse.hpp's own header comment already gives, cited
// instead of repeated in full here): a malformed dimension unit or an
// unrecognized component-value token kind is a DIAGNOSABLE SYNTAX
// defect (line, column, "what was expected" - exactly the
// gltfx_gfss_diagnostic category), not the OS/runtime-failure category
// gltfx_err/gltfx_rslt<T> exist to report.

namespace glintfx::style::detail {

// bool ok = false by default so a default-constructed result reads
// back as a FAILURE with an absent diagnostic (token.hpp's own R4
// convention: empty/zero = "never attached") - never a fabricated
// gltfx_gfss_value a caller could mistake for a real answer.
struct value_parse_result {
    bool ok = false;
    gltfx_gfss_value value{};
    gltfx_gfss_diagnostic diagnostic{};
};

// Decodes `token` - one component value GFSS-TOKEN's own tokenizer
// already produced - into its typed representation. An <ident-token>
// decodes to kind::keyword (see value.hpp's own header comment: the
// three universal keywords are NOT special-cased here); a <number-
// token> decodes to kind::integer or kind::number, by CSS Syntax
// Module Level 3 4.3.12's own type flag; a <percentage-token> decodes
// to kind::percentage, its magnitude PRESERVED, never folded into a
// length; a <dimension-token> decodes to kind::length if its own unit
// ident matches one of the 12 in value.hpp's own closed enum, or fails
// with k_expected_known_length_unit (diagnostic_vocabulary.hpp)
// otherwise. Every other token kind fails with k_expected_component_
// value.
[[nodiscard]] value_parse_result parse_value(const gltfx_gfss_token &token) noexcept;

} // namespace glintfx::style::detail
