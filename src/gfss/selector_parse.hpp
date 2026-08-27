// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/gfss/token.hpp>

#include "selector_ast.hpp"

// selector_parse.hpp - GFSS-SEL-PARSE-CORE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-27/L-28/L-40): reads gfss selector TEXT into
// selector_ast.hpp's own gfss_selector_list - a tag/.class/#id/*
// simple selector, a compound of several glued together with no
// combinator between them, the four combinators, a comma-separated
// list, and every pseudo-class ESCOPO.md SS4 decision 3 puts in the
// v1 ("seletor COMPLETO na v1"): the 14 argument-less ones
// (selector_pseudo_vocabulary.hpp's own GLINTFX_GFSS_SIMPLE_PSEUDO_
// LIST), matched by name, and the 5 functional ones (nth-child/
// nth-last-child/nth-of-type/nth-last-of-type/not), RECOGNIZED by
// name with their argument captured RAW, byte for byte, and left
// UNANALYZED for GFSS-SEL-PARSE-NTH/GFSS-SEL-PARSE-NOT (TODO.md, wave
// W4/W5) to read - this fatia's own scope line, "guarda o argumento
// cru para as duas fatias seguintes".
//
// OUT OF SCOPE, ON PURPOSE (TODO.md GFSS-SEL-PARSE-CORE row and the
// service order that opened this fatia): the attribute selector
// ([foo], [foo=bar], ...) - GFSS-SEL-PARSE-ATTR, wave W4 - and any
// analysis of a functional pseudo-class's OWN argument content -
// GFSS-SEL-PARSE-NTH/GFSS-SEL-PARSE-NOT, wave W4/W5. Neither omission
// is a defect of this file; both are named fatias with their own
// service order.
//
// INTERNAL IN THIS SLICE, ON PURPOSE (GODS_LAWS.md L-19: "o header
// nasce interno, em src/gfss/"): this header lives here, not under
// include/glintfx/, and parse_selector_list() below is not
// GLINTFX_API. GFSS-API (TODO.md, wave W10) is the dedicated review
// that decides the PUBLIC shape (name, namespace, whether it takes a
// gltfx_gfss_cursor or a plain std::string_view). Nothing here
// freezes that.
//
// DIAGNOSTIC-SHAPED RESULT, THE SAME UNRESOLVED TENSION color_parse.
// hpp's OWN HEADER COMMENT ALREADY NAMES (GODS_LAWS.md L-27, marked
// INFERENCE - read that file's own comment before "fixing" this to
// match core/color.hpp's forward-looking gltfx_rslt<T> prediction):
// selector_parse_result below follows the SAME line/column/"what was
// expected" shape token.hpp/tokenizer.hpp/color_parse.hpp already
// establish for THIS track, for the SAME reason - a malformed
// selector is a DIAGNOSABLE SYNTAX defect, not the OS/runtime failure
// category gltfx_err's own CE-3 fields are shaped around. THIS
// TENSION IS STILL UNRESOLVED ON PURPOSE: whichever shape is right is
// GFSS-API's call, not an implementer's, and nothing here is
// ABI-frozen yet.
//
// WHY THIS IS NOT noexcept, UNLIKE color_parse.hpp's OWN parse_color()
// (design tension resolved by a DIFFERENT precedent than the one
// above, GODS_LAWS.md L-27, marked INFERENCE): color_parse.hpp's own
// parse_color() returns a single gltfx_rgba - a plain scalar struct,
// no allocation anywhere in its signature. This file's own
// parse_selector_list() returns a gfss_selector_list, which is
// std::vector ALL THE WAY DOWN (a list of complex selectors, each
// holding a vector of combined selectors, each holding a vector of
// simple selectors) - the SAME shape tokenizer.hpp's own convenience
// wrapper gltfx_gfss_tokenize() already has, and that file's own
// header comment already gives the exact reasoning this function
// inherits: std::vector::push_back can throw std::bad_alloc, and nothing
// here catches it before returning, so this signature is not, and must
// not claim to be, noexcept.

namespace glintfx::style::detail {

// bool ok = false by default so a default-constructed result reads
// back as a FAILURE with an absent diagnostic (token.hpp's own R4
// convention: empty/zero = "never attached") - never a fabricated
// gfss_selector_list a caller could mistake for a real answer, the
// SAME shape color_parse_result (color_parse.hpp) already uses.
struct selector_parse_result {
    bool ok = false;
    gfss_selector_list value{};
    gltfx_gfss_diagnostic diagnostic{};
};

// Analyzes the WHOLE of `text` as one gfss selector list - one or more
// complex selectors separated by top-level commas (a comma INSIDE a
// functional pseudo-class's own raw argument, e.g. ":not(a, b)", is
// never a list separator - see selector_parse.cpp's own header
// comment on capture_functional_argument() for how paren depth is
// tracked). Internally tokenizes `text` with this SAME track's own
// gltfx_gfss_tokenize() (tokenizer.hpp) - a selector is layered ON TOP
// of the CSS Syntax Module Level 3 token stream, exactly the
// relationship color_parse.hpp's own header comment already describes
// for GFSS-COLOR-PARSE.
[[nodiscard]] selector_parse_result parse_selector_list(std::string_view text);

} // namespace glintfx::style::detail
