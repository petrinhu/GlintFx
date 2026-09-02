// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>

#include <glintfx/core/color.hpp>

#include "ascii_case.hpp"

// named_colors.hpp - GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md
// L-17/L-27/L-29/L-40): the closed table of CSS named color keywords,
// and NOTHING else - answers exactly one question, "what gltfx_rgba8
// does this keyword name?" - distinct from color_parse.hpp/.cpp (which
// decides WHEN a keyword lookup applies inside a gfss color value).
// Private to this project (not installed, not part of glintfx's public
// surface) - see color_parse.hpp's own header comment for why the
// whole GFSS-COLOR-PARSE surface is internal in this slice.
//
// SOURCE, AND WHY THE COUNT IS 149, NOT THE "~148" TODO.md's OWN ROW
// ESTIMATES (GODS_LAWS.md L-27, fact vs. inference; L-29, reading a
// public standard to learn its shape is legitimate - nothing here is
// copied CODE from any implementation, only DATA a public spec
// publishes for anyone to implement): 148 opaque keywords - the 147
// extended color keywords the CSS Color Module Level 4 "Named Colors"
// table defines (https://www.w3.org/TR/css-color-4/#named-colors,
// fetched and enumerated mechanically, not recalled from memory) PLUS
// "rebeccapurple" (a later addition to that same table,
// https://lists.w3.org/Archives/Public/www-style/2014Mar/0307.html) -
// PLUS "transparent", which MDN's own <named-color> reference
// (https://developer.mozilla.org/en-US/docs/Web/CSS/named-color) lists
// as part of the SAME value type, mapped here to rgba(0,0,0,0), CSS
// Color 4 SS4.2.4's own definition of that keyword. 148 + 1 = 149,
// k_named_color_count below (this file's own mechanically-counted
// constant, never a hand-copied literal - the SAME technique
// token.hpp's own gltfx_gfss_token_kind_count already uses).
//
// CROSS-CHECKED, NOT A SINGLE SOURCE TRUSTED ON FAITH (GODS_LAWS.md
// L-40's "enumerate the small space" extended to DATA, not just code):
// every one of the 148 opaque entries was diffed, channel by channel,
// against a SECOND, independently-fetched source
// (https://www.ditig.com/css-named-colors) for the 139 names present
// on both - zero mismatches on any red/green/blue value. The 9 entries
// only this table carries (the "grey"-spelled aliases of gray/
// darkgray/dimgray/lightgray/lightslategray/slategray/darkslategray,
// plus "grey" itself, plus "cyan"/"magenta") are not independently
// re-verified by that diff because ditig.com collapses each such alias
// to its one canonical spelling - each of those 9 is instead defined
// BY THE SPEC ITSELF to be identical to its already cross-checked
// counterpart (cyan == aqua, magenta == fuchsia, every "grey" ==
// its "gray" twin), so the diff still closes the loop for all 148.
//
// gltfx_rgba8, NOT gltfx_rgba (design choice made HERE, GODS_LAWS.md
// L-27, marked INFERENCE): CSS named colors are DEFINED as sRGB
// display-encoded triples (core/color.hpp's own header comment names
// this the boundary type - "what #rrggbb/rgb() text spells out digit
// by digit"), the SAME representation #rrggbb hex literals use.
// color_parse.cpp converts through glintfx::gltfx_rgba_from_srgb8()
// (core/color.hpp, already public, already tested) to reach the
// canonical linear gltfx_rgba every gfss color value resolves to -
// this table never repeats that conversion itself, so it stays
// answering only its own one question.
//
// ASCII CASE-INSENSITIVE LOOKUP (fact, not a scope decision - CSS
// Syntax Module Level 3's own definition of an <ident-token> match:
// keywords are matched "ASCII case-insensitively"): lookup_named_color()
// below treats "RED", "Red" and "red" identically, the SAME rule gfss's
// own function-name matching needs for "RGB(", "Rgb(" et al.
// (color_parse.cpp reuses ascii_case.hpp's own ASCII-fold helper for
// exactly that - a real, current use past CONTRACT.md SS6's "three
// occurrences" bar for a shared helper, see that helper's own header
// comment for the full count).

namespace glintfx::style::detail {

struct named_color_entry {
    // Default member initializers, not a user-declared constructor
    // (which would forfeit aggregate-init at every row of
    // named_colors.cpp's own k_table) - the same cppcheck
    // uninitMemberVarNoCtor fix tests/gfss_tokenizer_test.cpp's own
    // kind_sample and src/core/err_code.cpp's own err_code_entry
    // already apply: the checker cannot see that aggregate init
    // already sets every field at every use site.
    std::string_view name;
    gltfx_rgba8 value{};
};

// The ASCII case-fold helpers used to be DEFINED here (A named color
// keyword is always ASCII by construction -
// code_point.hpp's own is_ident_start()/is_ident_continue() accept
// non-ASCII too, but no CSS keyword this table defines ever contains
// one, so folding only ASCII is complete for this comparison, not a
// partial implementation of Unicode case-folding). GFSS-MATCH-SIMPLE
// fatia A (D-MS-3, this file's own header comment above) moved them
// VERBATIM to ascii_case.hpp, same namespace, same behavior - a fourth
// real consumer (the compound matcher) made keeping them bundled with
// this file's own 149-row color TABLE pull the whole table declaration
// into a layer that has no business seeing it.

// The table's own cardinality, counted mechanically from the table
// defined in named_colors.cpp - see that file's own comment for why
// the count lives there (next to the data it counts) rather than here.
[[nodiscard]] std::size_t named_color_count() noexcept;

// Direct enumeration by index, 0 <= index < named_color_count() -
// PRECONDITION, UB otherwise if the assert in named_colors.cpp is
// compiled out (the SAME precondition category gltfx_rslt<T>::value()
// already documents, docs/api-conventions.md R1's "reading the wrong
// value" paragraph - not a new idiom invented here). Exists so a test
// can enumerate the WHOLE table (GODS_LAWS.md L-40) by walking every
// index and round-tripping each name through lookup_named_color()
// below, instead of hand-copying all 149 rows a second time into the
// test file, which would risk exactly the drift L-40 exists to catch.
[[nodiscard]] const named_color_entry &named_color_at(std::size_t index) noexcept;

// The one real question this file answers: does `name` name a CSS
// color keyword, and if so, what gltfx_rgba8 does it mean? Returns
// false (out_value left UNCHANGED) when `name` matches nothing in the
// table - docs/api-conventions.md R4's "never undefined behavior"
// convention, applied to a boolean-plus-out-parameter shape because
// this is a private, unexported helper, not a public fallible
// signature (R1 governs the PUBLIC boundary only).
[[nodiscard]] bool lookup_named_color(std::string_view name, gltfx_rgba8 &out_value) noexcept;

} // namespace glintfx::style::detail
