// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

#include <glintfx/gfss/token.hpp>

// important_flag.hpp - GFSS-DECL-PARSE, DP-3 (TODO.md wave W5, GODS_
// LAWS.md L-17/L-20/L-27/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS1.1 item MDN/Cascade 5):
// answers exactly one question - "does this span of value tokens end
// with a well-formed `!important` flag, and if so, where does the
// VALUE without it end?" - never whether the remaining value itself is
// a valid declaration value (declaration_value_check.hpp's job).
//
// THE RULE (CSS Cascade Level 5 SS6.3; MDN's own "!important"
// reference, both cited in the plan's own SS1.1): importance is
// exactly the LAST TWO non-whitespace tokens of the value being a
// delim `!` followed by an ident `important`, ASCII case-insensitive -
// whitespace (and, upstream, a comment - already silently skipped by
// GFSS-TOKEN's own tokenizer) is allowed between the two ("`!
// important`" is valid, "the important flag must be the last token in
// the declaration"). A `!` token ANYWHERE in the span that is not part
// of exactly this trailing shape makes the whole read fail - CSS has no
// other legal use for a bare `!` delim in a declaration's value.
//
// EMPTY REMAINDER IS TREATED AS MALFORMED TOO - AN INFERENCE, MARKED
// (GODS_LAWS.md L-27): neither the Cascade 5 spec nor MDN's own page
// says what a bare "!important" (nothing before it) means for a
// declaration's OWN value; this project's own plan lists it among the
// SEVEN rejected forms of its own twelve-form enumeration (SS4.2) all
// the same. This file follows that classification: stripping a
// well-formed trailing pair that leaves NOTHING (no non-whitespace
// token) behind is `ok = false`, the same `important_flag_at_end_of_
// value` identifier every other malformed shape in this file reports -
// "important, applied to nothing" is not a value with a flag, and
// declaration_parse.cpp's own separate `component_value` diagnostic
// (an EMPTY declaration value) is reserved for a DIFFERENT caller shape
// (a name and a colon with nothing after), never reused here.

namespace glintfx::style::detail {

struct important_flag_result {
    bool ok = false;
    bool important = false;
    // Valid iff ok: [begin, value_end) of the ORIGINAL span is the
    // value with the flag (and the whitespace touching it) removed.
    // When `important` is false, value_end == the span's own `end`
    // (nothing was there to strip).
    std::size_t value_end = 0;
    gltfx_gfss_diagnostic diagnostic{};
};

// Reads `tokens[begin, end)`'s own trailing `!important` flag - see
// this file's own header comment for the exact rule and for the one
// inference (an empty remainder) it declares rather than states as
// spec fact.
[[nodiscard]] important_flag_result read_important_flag(const std::vector<gltfx_gfss_token> &tokens,
                                                          std::size_t begin, std::size_t end) noexcept;

} // namespace glintfx::style::detail
