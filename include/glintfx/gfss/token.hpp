// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// gfss/token.hpp - GFSS-TOKEN (TODO.md, GODS_LAWS.md L-19/L-20/L-28):
// the closed vocabulary of token classes the gfss tokenizer can
// produce, and the two value types every one of them carries.
//
// SOURCE OF THE VOCABULARY (GODS_LAWS.md L-27, fact vs. inference):
// ESCOPO.md SS4 fixes that gfss is modeled on the public web standards'
// vocabulary, never on a single consumer's need. The 25 token classes
// below, their names and the two "recovery" classes (bad_string,
// bad_url) are a DIRECT enumeration of the CSS Syntax Module Level 3
// tokenization grammar (https://www.w3.org/TR/css-syntax-3/,
// section 4 "Tokenization" - the per-production subsections are cited
// individually in tokenizer.hpp and in the .cpp files that implement
// each one). This is a FACT read from the public specification, not an
// agent's memory of it (GODS_LAWS.md L-29: reading a public standard to
// learn its shape is legitimate; nothing here is copied CODE from any
// implementation, only the VOCABULARY a public spec defines for
// anyone to implement).
//
// WHY THE GRAMMAR NEVER "FAILS" (fact, same source, section 2.2 and
// the per-production algorithms 4.3.5/4.3.6): CSS's own tokenizer
// philosophy is that "the parser attempts to recover gracefully,
// throwing away only the minimum amount of content before returning to
// parsing as normal" - tokenization NEVER aborts; a malformed string or
// url produces a dedicated RECOVERY token (bad_string/bad_url) instead
// of stopping. gfss inherits this on purpose (ESCOPO.md SS4: the
// format is modeled on the public standard's vocabulary AND its
// behavior, not just its spelling) - see tokenizer.hpp's own header
// comment for how this resolves the "stop at first error or keep
// going" question the service order for this slice raised.
//
// THE THIRD DIAGNOSTIC FIELD, "gltfx_gfss_diagnostic" (decision of the
// project leader, 26/08/2026, ESCOPO.md SS4 "Legibilidade humana":
// "erro de escrita se diagnostica com linha, coluna e o que se
// esperava"): this type is BORN in this slice and is NOT yet frozen
// ABI - GFSS-API's own dedicated API review (TODO.md, wave W10) is
// where it congeals into a permanent contract (that row's own text:
// "o tipo nasce em GFSS-TOKEN, e e aqui que ele vira contrato"). No
// static_assert freezes its footprint here on purpose - doing so now
// would be promising a shape before the review that owns that promise
// has looked at it.
//
// "expected" IS A VOCABULARY TOKEN, NOT A SENTENCE (docs/api-
// conventions.md R7, the SAME convention gltfx_err_fields() already
// uses project-wide): a stable, English, snake_case identifier such as
// "closing_quote" or "closing_parenthesis" - never a human sentence.
// glintfx never picks the consumer's language (LEI ZERO: the consumer
// base is public and unknown) - the identifier is what a consumer's
// own message-building code matches on.
//
// EMPTY MEANS "NO DIAGNOSTIC WAS ATTACHED" (the SAME 0/empty
// convention CE-3's diagnostic fields already use in
// glintfx::gltfx_err - docs/api-conventions.md R4): there is no
// separate has_diagnostic() in this v1. A token produced with no parse
// error leaves gltfx_gfss_diagnostic::expected empty; a consumer checks
// that one field, exactly like it already checks gltfx_err::path()
// being empty to mean "never attached".
//
// LEXEME IS THE RAW SOURCE SPAN, NOT A DECODED VALUE (scope decision
// made HERE, GODS_LAWS.md L-27, marked as INFERENCE, not a spec fact):
// gltfx_gfss_token::lexeme is a std::string_view into the CALLER's own
// source buffer, unprocessed - escape sequences inside a string or
// ident are NOT resolved to the code point they represent, and a
// number's digits are not converted to a numeric value. This fatia is
// the FIRST of 23 in the gfss track (TODO.md, "A trilha GFSS") and
// every later one - GFSS-VALUE-style decoding, GFSS-COLOR-PARSE's own
// hex/rgb/hsl conversion - consumes these raw lexemes and does its own
// decoding; bundling that here would make this единица answer two
// unrelated questions ("what kind of token is this" AND "what value
// does it hold once escapes are resolved"), which GODS_LAWS.md L-17's
// "a frase sem e" test rejects.

namespace glintfx::style {

// The closed vocabulary. Order matches the CSS Syntax Module Level 3
// grammar's own enumeration order (spec section 4.3.1 "Consume a
// token") purely for readability against the spec while reviewing -
// std::uint8_t is an explicit, stable underlying type, wide enough for
// the 25 classes this fatia freezes plus real headroom before a future
// slice could ever need a 26th. UNLIKE glintfx::gltfx_err_code (whose
// own header comment justifies 32 bits by a REAL coupling - the width
// is baked into gltfx_err's frozen two-pointer footprint, CE-2), this
// type has no such coupling: it is not yet ABI-frozen (see this
// header's own comment above on GFSS-API congealing it later), so
// there is no reason to pay four bytes when clang-tidy's
// performance-enum-size correctly says one suffices.
enum class gltfx_gfss_token_kind : std::uint8_t {
    ident,
    function,
    at_keyword,
    hash,
    string,
    bad_string,
    url,
    bad_url,
    delim,
    number,
    percentage,
    dimension,
    whitespace,
    cdo,
    cdc,
    colon,
    semicolon,
    comma,
    open_square,
    close_square,
    open_paren,
    close_paren,
    open_curly,
    close_curly,
    eof,
};

// Returns the IDENTIFIER of `kind` (e.g. "bad_url"), never a sentence -
// the same gltfx_err_code_name() convention (docs/api-conventions.md
// R7), and what tests/tools/todo the closed-enumeration proof
// (GODS_LAWS.md L-40) prints alongside its swept count so a failure
// names which of the 25 classes was not produced. noexcept, never
// undefined behavior: a `kind` value outside this table (a stale
// binary reading a newer library's numbering, or a raw cast) returns
// "unknown", the same fallback convention gltfx_err_code_name() uses.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_token_kind_name(gltfx_gfss_token_kind kind) noexcept;

// Line/column/"what was expected" - see this header's own comment
// above for why this type exists and why it is not ABI-frozen yet.
// Plain aggregate, no owned resource (Rule of Zero, CONTRACT.md SS2.2),
// same shape as glintfx::gltfx_rgba/glintfx::version: trivially
// copyable, safe to pass and return by value across the library
// boundary (tokenizer.hpp's gltfx_gfss_next_token() does exactly
// that).
struct gltfx_gfss_diagnostic {
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::string_view expected; // empty = no diagnostic attached (R4)
};

// One token. Plain aggregate, same ABI-safety reasoning as
// gltfx_gfss_diagnostic above - lexeme and diagnostic.expected are both
// std::string_view, non-owning, so this type never allocates and never
// owns memory crossing the .so/.dll boundary (docs/api-conventions.md
// R5's own rationale for why gltfx_err_fields() had to be careful does
// not apply here: there is no heap object anywhere in this type for
// two different CRTs to disagree about).
struct gltfx_gfss_token {
    gltfx_gfss_token_kind kind = gltfx_gfss_token_kind::eof;
    std::string_view lexeme;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    gltfx_gfss_diagnostic diagnostic;
};

} // namespace glintfx::style
