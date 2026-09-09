// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

#include <glintfx/gfss/token.hpp>

// declaration_split.hpp - GFSS-DECL-PARSE, DP-2 (TODO.md wave W5,
// GODS_LAWS.md L-17/L-20/L-27/L-28/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS1.1, SS5): answers exactly
// one question - "where does each declaration in a tokenized block
// END?" - never whether the tokens between two boundaries form a VALID
// declaration (declaration_parse.hpp's own job).
//
// THE RULE, FROM THE SPEC, NOT INVENTED HERE (CSS Syntax Module Level 3
// SS5.4.5 "Consume a list of declarations"; the Editor's Draft's own
// SS5.5.5/SS5.5.6 for the nested-block extension; the plan's own SS1.5
// summary item 1): a declaration ends at the next `;` token found at
// NESTING DEPTH ZERO - counting `(`/a <function-token>/`[`/`{` as +1 and
// their matching close as -1, exactly the `paren_depth_delta()`
// technique selector_parse.cpp's own capture_functional_argument()
// already established for THIS track, extended here from one pair
// (parens) to three (parens, square brackets, curly braces) per the
// Editor's Draft's own "a `{}`-block inside a value must be skipped
// whole, never stopped at a `;` inside it".
//
// WHY STRINGS NEVER NEED THIS TRACKING (Svelte #9637, plan SS1.2 - a
// real parser that broke on `;` inside a url()'s own quoted argument):
// GFSS-TOKEN's own tokenizer already consumes a whole <string-token> as
// ONE token, embedded `;` included in its own lexeme - this file never
// sees that `;` as a token of its own to misinterpret. Operating on the
// TOKEN STREAM, never re-scanning raw bytes, is what makes this
// immunity structural rather than a case this file has to special-case.

namespace glintfx::style::detail {

// One declaration's own span inside `tokens`: `[begin, end)`, NEVER
// including the terminating `;` (if any) nor the trailing <EOF-token>
// gltfx_gfss_tokenize()'s own convention always appends.
struct declaration_span {
    std::size_t begin = 0;
    std::size_t end = 0;
};

// Splits `tokens` (a full token vector ending in an <EOF-token>) into
// one span per declaration candidate, at every top-level `;`. A span
// with NO non-whitespace token inside it (two `;` back to back, or
// whitespace-only text) is DROPPED, never produced empty - the CSS
// grammar itself treats a bare `;` and whitespace as inert (plan SS1.1
// item (c)), and declaration_parse.hpp's own `component_value`
// diagnostic already covers the DIFFERENT condition of a NAMED
// declaration with an empty VALUE.
// NOT noexcept: returns a std::vector built with push_back, the SAME
// reason tokenizer.hpp's own gltfx_gfss_tokenize() and selector_parse.
// hpp's own parse_selector_list() are not either - std::vector::
// push_back can throw std::bad_alloc, uncaught here.
[[nodiscard]] std::vector<declaration_span>
split_declarations_at_top_level_semicolons(const std::vector<gltfx_gfss_token> &tokens);

} // namespace glintfx::style::detail
