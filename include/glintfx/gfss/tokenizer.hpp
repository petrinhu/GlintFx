// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <glintfx/export.hpp>
#include <glintfx/gfss/token.hpp>

// gfss/tokenizer.hpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-19/L-20/
// L-22/L-28): scans a gfss style sheet's raw text into the closed
// vocabulary token.hpp declares - the CSS Syntax Module Level 3
// tokenization grammar (see token.hpp's own header comment for the
// source and the per-production algorithms this module implements,
// cited section by section in tokenizer.cpp).
//
// WHY THIS NEVER RETURNS gltfx_rslt<T> (design decision made HERE,
// GODS_LAWS.md L-27, marked INFERENCE - R1 of docs/api-conventions.md
// still governs every OTHER fallible signature in this library
// unchanged): the CSS grammar this module implements is, BY THE
// SPEC'S OWN DESIGN (token.hpp's header comment, section 2.2),
// INCAPABLE of the kind of failure gltfx_err/gltfx_rslt<T> exist to
// report - a malformed string or url produces a RECOVERY token
// (bad_string/bad_url) carrying a gltfx_gfss_diagnostic, never a
// library-level error. gltfx_gfss_next_token() below is exactly as
// "cannot fail" as glintfx::gltfx_rgba_from_srgb8() (core/color.hpp) -
// no allocation happens inside it (every field of gltfx_gfss_token is
// a std::string_view or a scalar - see token.hpp's own comment), so
// there is no fallible signature here to wrap, the SAME reasoning
// color.hpp's own header comment gives for decision 6's conversions.
//
// TWO-LAYER API, THE SAME "R5(b)" TECHNIQUE err_format.hpp ALREADY
// ESTABLISHED for gltfx_err_fields() (docs/api-conventions.md R5):
// gltfx_gfss_next_token() is the EXPORTED, ABI-crossing primitive - it
// takes/returns everything BY REFERENCE or as a plain, non-owning
// value, so nothing it touches is a container allocated on one side
// of the .so/.dll boundary and freed on the other. gltfx_gfss_tokenize()
// below is the convenience wrapper that builds a std::vector one call
// at a time - like gltfx_err_fields(), it is ENTIRELY inline, so the
// vector's own allocation happens on the CALLER's compiled side, both
// ends, never crossing the boundary; this is also why it is NOT
// noexcept (the same as gltfx_err_fields() is not: std::vector::
// push_back can throw std::bad_alloc, and that exception never leaves
// the consumer's own compiled code, so GODS_LAWS.md L-22's "no
// exception crosses the PUBLIC BOUNDARY" is not engaged at all here).
//
// INTERNAL DEFECT SIGNAL, READ THIS BEFORE BLAMING YOUR OWN gfss FILE
// (GODS_LAWS.md L-40, fix for the CRITICO that reproved commit
// 95c0f20 - src/gfss/token_progress_recovery.hpp has the full
// rationale): if gltfx_gfss_next_token() ever returns an <EOF-token>
// carrying diagnostic.expected == "internal_tokenizer_defect" BEFORE
// your own source is exhausted, YOUR FILE IS NOT AT FAULT. This
// library detected that it had broken its own internal contract (a
// token production that consumed zero code points) and stopped rather
// than risk handing you more tokens it could no longer trust - the
// SAME recovery-token convention token.hpp's own header comment
// documents for <bad-string-token>/<bad-url-token> covers a MALFORMED
// INPUT of yours; this one covers a DEFECT IN GLINTFX ITSELF. Every
// subsequent call on the same cursor keeps returning <EOF-token>/
// false - this is a hard stop, not a resumable parse error: do not
// retry, do not treat it as a syntax error in your own gfss source,
// report it upstream to glintfx. gltfx_gfss_diagnostic is not yet
// ABI-frozen (token.hpp's own header comment) - this specific
// identifier freezes together with the rest of the type at GFSS-API's
// dedicated review (TODO.md, wave W10).

namespace glintfx::style {

// Scanning position, PUBLIC because gltfx_gfss_next_token() takes one
// by reference (a consumer resumes scanning by keeping this value
// between calls) - plain aggregate, same ABI-safety shape as
// gltfx_gfss_token (see that type's own comment): `source` is a
// non-owning view into the CALLER's buffer, which must outlive every
// cursor and token derived from it, exactly the lifetime rule
// std::string_view itself already carries.
struct gltfx_gfss_cursor {
    std::string_view source;
    std::size_t byte_offset = 0;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

// Scans exactly ONE token starting at `cursor`'s current position,
// writes it to `out_token`, and advances `cursor` past it. Comments
// (CSS Syntax Module Level 3 section 4.3.2, "Consume comments") are
// skipped silently before the token that follows them, exactly as the
// spec's own "consume a token" algorithm does - never returned as a
// token of their own kind, because the grammar this file implements
// has no <comment-token>.
//
// Returns true while there is more to scan; false the moment
// `out_token` IS the <EOF-token> - a caller loops
// `while (gltfx_gfss_next_token(cursor, token)) { ... }` and always
// still needs to consume the final `token` the loop condition read (see
// gltfx_gfss_tokenize() below for that exact loop, done once so no
// caller has to get the off-by-one right themselves).
[[nodiscard]] GLINTFX_API bool gltfx_gfss_next_token(gltfx_gfss_cursor &cursor,
                                                     gltfx_gfss_token &out_token) noexcept;

// Convenience: every token of `source`, including the trailing
// <EOF-token>, in one call - see this header's own comment above for
// why this stays entirely inline (docs/api-conventions.md R5(b)) and
// is not noexcept (unlike gltfx_gfss_next_token() above, which is).
[[nodiscard]] inline std::vector<gltfx_gfss_token> gltfx_gfss_tokenize(std::string_view source) {
    std::vector<gltfx_gfss_token> tokens;
    gltfx_gfss_cursor cursor{.source = source};
    gltfx_gfss_token token;
    bool more = true;
    while (more) {
        more = gltfx_gfss_next_token(cursor, token);
        tokens.push_back(token);
    }
    return tokens;
}

} // namespace glintfx::style
