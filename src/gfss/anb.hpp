// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// anb.hpp - GFSS-SEL-PARSE-NTH (TODO.md, GODS_LAWS.md L-17/L-19/L-27;
// source read under L-29/L-43: CSS Syntax Module Level 3 SS6 "The An+B
// microsyntax", https://www.w3.org/TR/css-syntax-3/#anb-microsyntax,
// cross-checked against MDN's own :nth-child() page for the worked
// examples anb_parse.cpp's own test suite enumerates): the closed
// vocabulary of the An+B MICROSYNTAX's own PARSED shape - two signed
// integers, `a` (the cyclic step) and `b` (the constant offset). "The
// An+B-th element of a list" (the spec's own phrasing) reads literally
// as A*n+B for every non-negative integer n=0,1,2,...
//
// SEPARATE FILE FROM anb_parse.hpp/.cpp - deliberate (GODS_LAWS.md
// L-17: "arquivo e atomo de assunto"), the SAME split selector_ast.hpp/
// selector_parse.hpp already establish for this track: this file
// answers "what SHAPE does a parsed An+B value take", never "how do I
// read An+B TEXT into that shape".
//
// long long, NOT int (GODS_LAWS.md L-27, marked INFERENCE): matches
// value.hpp's own gltfx_gfss_value::integer_value width - a hostile
// author's own digit run (e.g. ":nth-child(99999999999999999999n)")
// must SATURATE, never silently wrap or narrow into undefined
// behavior, the SAME "the library never aborts the consumer's process
// on hostile input" principle numeric_lexeme.cpp's own decode_number_
// lexeme() already applies (ESCOPO.md SS2 decision 1) - anb_parse.cpp's
// own decode_anb_integer() is where that saturation actually happens.
//
// INTERNAL IN THIS SLICE, ON PURPOSE (GODS_LAWS.md L-19, the SAME
// reasoning selector_ast.hpp's own header comment already gives for
// this track): lives under src/gfss/, not include/glintfx/ - GFSS-API
// (TODO.md, wave W10) is the dedicated review that decides the PUBLIC
// shape. Nothing here is ABI-frozen.

namespace glintfx::style::detail {

// One parsed An+B value - `a` is 0 for a bare <integer> (":nth-child(5)"
// means a=0, b=5 - "every 0th element plus 5" collapses to "only the
// 5th"), and `b` is 0 for a bare coefficient with no offset
// (":nth-child(3n)" means a=3, b=0). Plain aggregate, no owned
// resource (Rule of Zero, CONTRACT.md SS2.2) - the same ABI-safety
// shape gfss_combinator_entry (selector_ast.hpp) already uses.
struct gfss_anb {
    long long a = 0;
    long long b = 0;
};

} // namespace glintfx::style::detail
