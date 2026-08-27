// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/gfss/token.hpp>

// color_parse.hpp - GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-27/L-28/L-40): analyzes gfss color TEXT (hex
// literals, the legacy comma-separated rgb()/rgba()/hsl()/hsla()
// functions, and the CSS named color keywords - named_colors.hpp's own
// table) into the canonical linear-light glintfx::gltfx_rgba
// (core/color.hpp) every gfss color value resolves to.
//
// INTERNAL IN THIS SLICE, ON PURPOSE (GODS_LAWS.md L-19: "the header
// nasce interno, em src/gfss/"): this header lives here, not under
// include/glintfx/, and parse_color() below is not GLINTFX_API. Nothing
// in this fatia's service order promotes it - GFSS-API (TODO.md, wave
// W10) is the dedicated review that decides the PUBLIC shape (name,
// namespace, whether it takes a gltfx_gfss_cursor or a plain
// std::string_view, whether an already-parsed hash/function token can
// be handed in directly to skip re-tokenizing a caller's own gfss
// document). Nothing here freezes that.
//
// WHY THIS RETURNS color_parse_result, NOT gltfx_rslt<gltfx_rgba>
// (design tension, GODS_LAWS.md L-27, explicitly marked - read before
// "fixing" this to match core/color.hpp's own forward-looking comment):
// core/color.hpp's decision-6 paragraph, written BEFORE this fatia
// existed, predicts the eventual public signature returns "a FALLIBLE
// gltfx_rslt<gltfx_rgba>". This fatia's own service order, resolved
// with the project leader in the L-34 brainstorm step that opened it,
// instead directs the SAME shape token.hpp/tokenizer.hpp's own
// gltfx_gfss_diagnostic already establishes for THIS track - line,
// column, and a stable "what was expected" vocabulary token
// (color_diagnostic_vocabulary.hpp), never gltfx_err/gltfx_rslt<T>.
// The reasoning tokenizer.hpp's own header comment already gives for
// why IT never returns gltfx_rslt<T> applies here with equal force: a
// malformed hex literal or a mistyped rgb() argument is a DIAGNOSABLE
// SYNTAX defect (exactly the category gltfx_gfss_diagnostic exists
// for), not the OS/runtime failure category gltfx_err's own CE-3
// fields (path, byte_offset, os_error_code) are shaped around - and
// gltfx_err's fields have no "what was expected" vocabulary slot at
// all, the exact thing this slice's own service order asks for by
// name. THIS TENSION IS UNRESOLVED ON PURPOSE: whichever shape is
// right is GFSS-API's call, not an implementer's, and nothing here is
// ABI-frozen yet (this file is not installed) - the two candidate
// designs are recorded here, side by side, so that review has the real
// trade-off in front of it instead of a silently-picked answer.
//
// SCOPE CUTS MADE HERE (GODS_LAWS.md L-27, all marked INFERENCE - the
// service order that opened this fatia names the FOUR function names
// and the closed hex/percentage/keyword shape; it does not itself
// pick a grammar for each one, and CSS itself has offered more than
// one across its own history):
//
//   1. LEGACY, COMMA-SEPARATED SYNTAX ONLY (CSS Color Module Level 3's
//      own grammar - https://www.w3.org/TR/css-color-3/#rgb-color,
//      read under GODS_LAWS.md L-29) - not CSS Color 4's later
//      space-separated, slash-alpha unification where rgb()/rgba()
//      (and hsl()/hsla()) become spelling ALIASES of one another,
//      each accepting 3 OR 4 arguments. The service order names FOUR
//      distinct function identifiers, matching the legacy spec's own
//      FOUR distinct grammar productions, one arity each - this is the
//      simpler, unambiguous, fully-public reading of that scope.
//   2. rgb()/hsl() REQUIRE EXACTLY 3 ARGUMENTS; rgba()/hsla() REQUIRE
//      EXACTLY 4 - a mismatch (rgb() with 4, rgba() with 3) is a
//      parse error (color_diagnostic_vocabulary.hpp's own
//      k_color_expected_argument_count), not a silently-accepted
//      alias of the other name. This is the SAME legacy spec's own
//      per-function arity, not a new invention.
//   3. rgb()'s three color components MUST be the SAME type - all
//      <number> (0-255) or all <percentage> (0%-100%) - CSS Color 3's
//      own "all values must be of the same type" clause. alpha (in
//      rgba()/hsla()) is INDEPENDENT of that rule: an
//      <alpha-value> is always <number> [0,1] OR <percentage>
//      [0%,100%], regardless of what type the color components used.
//   4. hsl()'s hue is a BARE <number> in degrees in this v1 - no
//      <angle> dimension (`180deg`/`0.5turn`) yet. A dimension token
//      where hue is expected is a parse error
//      (k_color_expected_number), a declared cut, not a silent
//      truncation.
//   5. OUT-OF-RANGE COMPONENTS ARE CLAMPED, NEVER REJECTED - CSS Color
//      4 SS4/13's own "the used value... MUST be clamped" rule, the
//      SAME convention core/color.hpp's own gltfx_rgba_to_srgb8()
//      already applies at the opposite boundary (linear-to-8-bit).
//      rgb(300, -10, 0) is valid gfss, clamping to (255, 0, 0) - it is
//      NOT one of this file's parse errors.
//   6. oklch()/lab()/lch()/oklab() ARE RECOGNIZED BY NAME AND REJECTED
//      WITH A DEDICATED DIAGNOSTIC (k_color_expected_shipped_color_
//      notation), distinct from an UNKNOWN function name
//      (k_color_expected_known_color_function) - ESCOPO.md SS4 decision
//      8: oklch() ships in its OWN slice (TODO.md, GFSS-OKLCH, wave
//      W4) because it needs perceptual-space conversion and gamut
//      mapping none of the other notations this file implements do;
//      lab()/lch()/oklab() stay out of the v1 scope entirely. The
//      distinct identifier is what lets a consumer's own error message
//      say "not yet" instead of "never" for exactly these four names -
//      the CONTENT that ties it to GFSS-OKLCH specifically lives in
//      this project's own test and docs, not baked into the public
//      vocabulary token itself (docs/api-conventions.md R7: a stable
//      identifier, never a sentence, never a project-internal ID).

namespace glintfx::style::detail {

// bool ok = false by default so a default-constructed result reads
// back as a FAILURE with an absent diagnostic (token.hpp's own R4
// convention: empty/zero = "never attached") - never a fabricated
// gltfx_rgba a caller could mistake for a real answer.
struct color_parse_result {
    bool ok = false;
    gltfx_rgba value{};
    gltfx_gfss_diagnostic diagnostic{};
};

// Analyzes the WHOLE of `text` as one gfss color value - a single hex
// literal, function call, or named keyword, with only whitespace
// allowed before and after it (k_color_expected_no_trailing_content
// covers anything else trailing). Internally tokenizes `text` with
// this SAME track's own gltfx_gfss_next_token() (tokenizer.hpp) - a
// color value is layered ON TOP of the CSS Syntax Module Level 3 token
// stream, exactly the relationship token.hpp's own header comment
// already describes for "GFSS-VALUE-style decoding, GFSS-COLOR-PARSE's
// own hex/rgb/hsl conversion".
[[nodiscard]] color_parse_result parse_color(std::string_view text) noexcept;

} // namespace glintfx::style::detail
