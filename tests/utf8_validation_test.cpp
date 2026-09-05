// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/utf8_validation.hpp"

// utf8_validation_test.cpp - W-C' (docs/plano-w6a-janela.md fatia 4,
// D-W6a-23): proves is_valid_utf8() accepts everything RFC 3629 allows
// and rejects everything it does not - the six edge cases the plan
// names (docs/plano-w6a-janela.md sec. 2.2, row 4's own test list: two
// accepted shapes, four rejected ones - "sequencia truncada, overlong,
// surrogate, acima de U+10FFFF"), using literal byte sequences ("\xNN"
// escapes) rather than trusting this source file's own encoding for
// the invalid ones - a malformed sequence cannot be typed as a normal
// string literal and stay malformed once the compiler re-encodes it.
// The empty-string case lives one layer up, in window_desc_validation_
// test.cpp's own "vazio aceito" case - it is a fact about WHICH FIELDS
// require text, not about UTF-8 decoding, so it is not repeated here
// (GODS_LAWS.md L-17: one atom, one subject).

GLINTFX_TEST(ascii_text_is_valid) {
    GLINTFX_CHECK(glintfx::platform::is_valid_utf8("hello, glintfx"));
}

GLINTFX_TEST(valid_multibyte_text_with_accents_is_valid) {
    // "Configuração" - the 'ç' (U+00E7, 2 bytes) and 'ã' (U+00E3, 2
    // bytes) are exactly the kind of accented, valid multi-byte text
    // window_desc_validation_test's own "titulo valido com acento"
    // case exercises one layer up.
    GLINTFX_CHECK(glintfx::platform::is_valid_utf8("Configura\xc3\xa7\xc3\xa3o"));
}

GLINTFX_TEST(truncated_sequence_is_invalid) {
    // 0xE2 announces a 3-byte sequence ("€", the EURO SIGN is
    // 0xE2 0x82 0xAC) but the string ends after only one continuation
    // byte - truncated mid-sequence.
    GLINTFX_CHECK(!glintfx::platform::is_valid_utf8("euro \xe2\x82"));
}

GLINTFX_TEST(overlong_encoding_is_invalid) {
    // 0xC0 0x80 spells NUL (U+0000) in two bytes - the classic overlong
    // encoding RFC 3629 outlaws (NUL fits in one byte, so a two-byte
    // spelling of it is never valid, decoder confusion attacks being
    // exactly why the rule exists).
    GLINTFX_CHECK(!glintfx::platform::is_valid_utf8(std::string_view{"\xc0\x80", 2}));
}

GLINTFX_TEST(surrogate_half_is_invalid) {
    // 0xED 0xA0 0x80 decodes to U+D800, the first UTF-16 surrogate
    // half - never a valid Unicode scalar value on its own, only ever
    // legal as one half of a UTF-16 pair.
    GLINTFX_CHECK(!glintfx::platform::is_valid_utf8("\xed\xa0\x80"));
}

GLINTFX_TEST(code_point_past_max_scalar_value_is_invalid) {
    // 0xF4 0x90 0x80 0x80 decodes to U+110000 - one past U+10FFFF, the
    // highest Unicode scalar value that exists.
    GLINTFX_CHECK(!glintfx::platform::is_valid_utf8("\xf4\x90\x80\x80"));
}
