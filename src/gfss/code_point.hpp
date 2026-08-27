// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// code_point.hpp - GFSS-TOKEN, private helper (GODS_LAWS.md L-17):
// answers exactly one question - "what CLASS does this code point
// belong to?" - per CSS Syntax Module Level 3 section 4.2
// ("Definitions"), read under GODS_LAWS.md L-29 (a public
// specification, never a line of RmlUi/SDL3 code). Not installed, not
// part of glintfx's public surface - included via a quoted path from
// the .cpp files under this directory that need it.
//
// THE "int code point or EOF" SENTINEL (design choice, GODS_LAWS.md
// L-27, not a spec requirement): every function below takes a plain
// `int`, -1 meaning "past the end of input" - the same shape C's own
// getc()/EOF convention uses, chosen so every classification
// predicate is a free function of one scalar, testable in complete
// isolation from any cursor or source buffer.
//
// ASCII-ONLY CLASSIFICATION (scope decision, GODS_LAWS.md L-27): every
// predicate here answers correctly for any value 0-127 exactly as the
// spec defines it, and answers false for every value 128 and above
// EXCEPT is_non_ascii()/is_ident_start()/is_ident_continue(), which the
// spec itself defines as "true for every non-ASCII code point" - this
// file never has to decode a multi-byte UTF-8 sequence to answer any
// of these, because the spec's own definition of "non-ASCII code
// point" is already a single, sourceless boolean once cursor.hpp (see
// that file's own header comment) has decided the code point's value
// is >= 0x80.

namespace glintfx::style::detail {

[[nodiscard]] constexpr bool is_digit(int cp) noexcept { return cp >= '0' && cp <= '9'; }

[[nodiscard]] constexpr bool is_hex_digit(int cp) noexcept {
    return is_digit(cp) || (cp >= 'A' && cp <= 'F') || (cp >= 'a' && cp <= 'f');
}

[[nodiscard]] constexpr bool is_uppercase_letter(int cp) noexcept { return cp >= 'A' && cp <= 'Z'; }

[[nodiscard]] constexpr bool is_lowercase_letter(int cp) noexcept { return cp >= 'a' && cp <= 'z'; }

[[nodiscard]] constexpr bool is_letter(int cp) noexcept {
    return is_uppercase_letter(cp) || is_lowercase_letter(cp);
}

// >= U+0080 - cursor.hpp's advance_code_point() already folds an
// entire multi-byte UTF-8 sequence into ONE call to peek()/advance(),
// so this file only ever sees a genuine single code point value here,
// never a lone continuation byte.
[[nodiscard]] constexpr bool is_non_ascii(int cp) noexcept { return cp >= 0x80; }

[[nodiscard]] constexpr bool is_ident_start(int cp) noexcept {
    return is_letter(cp) || is_non_ascii(cp) || cp == '_';
}

[[nodiscard]] constexpr bool is_ident_continue(int cp) noexcept {
    return is_ident_start(cp) || is_digit(cp) || cp == '-';
}

[[nodiscard]] constexpr bool is_non_printable(int cp) noexcept {
    return (cp >= 0x00 && cp <= 0x08) || cp == 0x0B || (cp >= 0x0E && cp <= 0x1F) || cp == 0x7F;
}

// U+000A LINE FEED only - the spec's own definition (section 4.2), NOT
// "any of CR/LF/CRLF": glintfx does not implement section 3.3's input
// preprocessing pass (replacing CR/CRLF/FF with LF before
// tokenization) in THIS slice - a scope decision (GODS_LAWS.md L-27,
// declared, not silently skipped) left to whichever future gfss slice
// owns reading a file from disk, since a std::string_view source
// (this module's whole input contract) may already have been
// normalized by its own caller.
[[nodiscard]] constexpr bool is_newline(int cp) noexcept { return cp == '\n'; }

[[nodiscard]] constexpr bool is_whitespace(int cp) noexcept {
    return is_newline(cp) || cp == '\t' || cp == ' ';
}

} // namespace glintfx::style::detail
