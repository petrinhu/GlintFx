// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <glintfx/gfss/token.hpp>

// token_progress_guard.hpp - GFSS-TOKEN, private helper (GODS_LAWS.md
// L-17/L-40 achado 2 of 26/08/2026): answers exactly one question -
// "did the single call to dispatch_token() that just ran (tokenizer.cpp)
// honor its own internal contract of consuming at least one code point,
// unless it returned <EOF-token>?" - distinct from every other private
// helper in this directory (cursor_ops.hpp answers "how do I step past
// one code point", lexical_rules.hpp answers "what does the grammar say
// about escapes/idents/numbers"): this file's only concern is a
// property of the RESULT of one dispatch_token() call, not how to
// produce it.
//
// WHY THIS EXISTS AS ITS OWN HEADER, NOT AN ANONYMOUS-NAMESPACE
// FUNCTION IN tokenizer.cpp (the achado this fatia is fixing):
// gfss_tokenizer_test.cpp needs to exercise a VIOLATION of this
// invariant directly, with hand-picked offsets, to prove
// tokenizer.cpp's own gltfx_gfss_next_token() reacts to it correctly in
// BOTH build modes (see that function's own header comment for the two
// reactions: assert() in a Debug build, and a release-safe forced
// advance the assert alone never provided). Leaving dispatch_token()
// itself deliberately broken to reach that test would violate
// GODS_LAWS.md L-01/L-20's own spirit (no genuinely broken production
// code committed just to make a test red); a pure, testable predicate
// that both the real call site and the test can call with the SAME
// synthetic inputs avoids that entirely. Not installed, not part of
// glintfx's public surface - gfss_tokenizer_test.cpp reaches it only
// because tests/CMakeLists.txt grants that one test target the same
// PRIVATE "${PROJECT_SOURCE_DIR}/src" include directory
// gl_proc_address_assign_test.cpp already uses for
// render/gl_proc_address.hpp.
//
// PURE AND cursor-FREE ON PURPOSE: takes plain offsets, not a
// gltfx_gfss_cursor, so a test can construct the "as if zero progress
// happened" scenario with two integers, without needing a real,
// correctly-advancing cursor at all.

namespace glintfx::style::detail {

// True iff `kind` is the <EOF-token>, or `current_offset` moved past
// `start_offset` - the exact invariant every dispatch_token() branch in
// tokenizer.cpp promises to uphold through a DIFFERENT mechanism per
// branch (an unconditionally-consumed opening quote/bracket/at-sign/
// backslash, or would_start_number()/would_start_ident_sequence()
// having said yes forcing consume_number()/consume_ident_sequence() to
// consume something) - there is no single centralized enforcement of
// that INSIDE dispatch_token() itself, so a future one-line regression
// in any ONE branch (measured LIVE: neutralizing
// detail::consume_optional_sign() alone, in lexical_rules.cpp, makes a
// leading "-3.5e-2" produce a ZERO-LENGTH <number-token>) needs this
// caller-side check to turn into something other than
// gltfx_gfss_tokenize()'s `while` loop spinning forever.
[[nodiscard]] constexpr bool token_made_forward_progress(gltfx_gfss_token_kind kind,
                                                         std::size_t start_offset,
                                                         std::size_t current_offset) noexcept {
    return kind == gltfx_gfss_token_kind::eof || current_offset > start_offset;
}

} // namespace glintfx::style::detail
