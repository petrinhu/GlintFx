// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

// platform/window/utf8_validation.hpp - the one atom D-W6a-23 (docs/
// plano-w6a-janela.md) exists for: deciding, from raw bytes alone,
// whether a string is valid UTF-8, before window_desc_validation.hpp
// hands a title or application_id to either backend. Lives in its own
// file rather than inside window_desc_validation.{hpp,cpp}
// (GODS_LAWS.md L-17: one atom, one subject) because the plan already
// names where it moves next: "quando a trilha de keymap trouxer o
// validador dela, e o mesmo atomo movido para core/, nao um segundo".
//
// WHY THIS EXISTS AT ALL (measured divergence, not taste): Win32's
// MultiByteToWideChar(MB_ERR_INVALID_CHARS, ...) REJECTS a malformed
// byte sequence outright; Wayland's xdg_toplevel_set_title just copies
// the bytes into a wl_array and lets the compositor render whatever it
// can. Validating BEFORE either backend ever sees the string is what
// keeps the SAME malformed title from behaving differently on the two
// platforms (GODS_LAWS.md L-04).
//
// Pure: no allocation, no locale, no backend header. Rejects exactly
// what a naive byte-length check misses: truncated sequences (a lead
// byte with too few - or zero - trailing continuation bytes), invalid
// lead/continuation bytes, overlong encodings (a code point spelled
// with more bytes than RFC 3629 allows for its range - e.g. 0xC0 0x80
// for NUL), UTF-16 surrogate halves (U+D800..U+DFFF, never a valid
// scalar value on their own), and any code point past U+10FFFF (the
// highest Unicode scalar value that exists).

namespace glintfx::platform {

[[nodiscard]] bool is_valid_utf8(std::string_view text) noexcept;

} // namespace glintfx::platform
