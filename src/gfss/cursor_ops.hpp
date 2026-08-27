// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/gfss/tokenizer.hpp>

#include "code_point.hpp"

// cursor_ops.hpp - GFSS-TOKEN, private helper (GODS_LAWS.md L-17):
// answers exactly one question - "how do I look at, and step past,
// the next code point of a gltfx_gfss_cursor?" - distinct from
// code_point.hpp's own question ("what CLASS does a code point
// belong to?"): this file changes if HOW this project reads bytes
// changes; code_point.hpp changes only if the CSS-defined character
// classes themselves do.
//
// UTF-8 SEQUENCE LENGTH IS CLASSIFICATION, NOT VALIDATION (scope
// decision, GODS_LAWS.md L-27, not a spec requirement): the CSS
// grammar operates on already-decoded Unicode code points; this
// module operates on UTF-8 bytes and only ever needs to know HOW MANY
// BYTES one code point occupies, never its numeric value (every
// classification predicate this scanner calls for a non-ASCII code
// point returns the same answer regardless of which one it is - see
// code_point.hpp's own header comment). A malformed/orphan
// continuation byte encountered as a LEADING byte (which never
// happens in well-formed UTF-8) degrades to length 1, advancing past
// just that byte - this file classifies well-formed input correctly
// and never gets stuck on ill-formed input, it does not claim to
// validate UTF-8.

namespace glintfx::style::detail {

// 1 for plain ASCII (0x00-0x7F) and for a stray continuation/invalid
// lead byte (see this file's own header comment on the malformed-
// input degrade); 2/3/4 for a genuine UTF-8 lead byte, read off its
// high bits exactly as RFC 3629 defines them.
[[nodiscard]] constexpr int utf8_sequence_length(unsigned char lead_byte) noexcept {
    if ((lead_byte & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead_byte & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead_byte & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

[[nodiscard]] constexpr bool at_end(const gltfx_gfss_cursor &cursor) noexcept {
    return cursor.byte_offset >= cursor.source.size();
}

// The BYTE at cursor.byte_offset + lookahead, or -1 past the end of
// source - see tokenizer.hpp/code_point.hpp's own "int code point or
// EOF" convention. Never mutates `cursor` - see advance_code_point
// below for the one function in this file that does.
[[nodiscard]] constexpr int peek(const gltfx_gfss_cursor &cursor,
                                 std::size_t lookahead = 0) noexcept {
    const std::size_t index = cursor.byte_offset + lookahead;
    if (index >= cursor.source.size()) {
        return -1;
    }
    return static_cast<unsigned char>(cursor.source[index]);
}

// Consumes exactly one CODE POINT: 1 byte if ASCII, N bytes if a
// multi-byte UTF-8 lead byte (utf8_sequence_length above) - clamped to
// never read past source.size(), so a truncated trailing sequence at
// EOF still advances to the end instead of reading out of bounds.
// column counts CODE POINTS, not bytes, so a diagnostic's column means
// what a human counts reading the line - is_newline() (code_point.hpp)
// is the SAME "U+000A only" definition the rest of this scanner uses,
// not CR/CRLF (see that file's own header comment on why).
constexpr void advance_code_point(gltfx_gfss_cursor &cursor) noexcept {
    if (at_end(cursor)) {
        return;
    }
    const auto lead_byte = static_cast<unsigned char>(cursor.source[cursor.byte_offset]);
    const int sequence_length = utf8_sequence_length(lead_byte);
    const std::size_t remaining = cursor.source.size() - cursor.byte_offset;
    const auto step = static_cast<std::size_t>(sequence_length) < remaining
                          ? static_cast<std::size_t>(sequence_length)
                          : remaining;

    if (is_newline(static_cast<int>(lead_byte))) {
        ++cursor.line;
        cursor.column = 1;
    } else {
        ++cursor.column;
    }
    cursor.byte_offset += step;
}

} // namespace glintfx::style::detail
