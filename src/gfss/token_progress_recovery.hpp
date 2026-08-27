// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

#include "diagnostic_vocabulary.hpp"

// token_progress_recovery.hpp - GFSS-TOKEN, private helper
// (GODS_LAWS.md L-17/L-22/L-40; fix for the CRITICO that reproved
// commit 95c0f20): answers exactly one question - "gltfx_gfss_next_
// token() already KNOWS (token_progress_guard.hpp's own
// token_made_forward_progress() said false) that dispatch_token() just
// violated its own internal contract; what token does the caller get
// back?" - distinct from token_progress_guard.hpp's own question ("did
// the violation happen?"): that file is a pure predicate over two
// offsets, this file is what runs ONLY once the answer was already no.
//
// WHY THIS REPLACES 95c0f20's OWN "FORCE ONE CODE POINT OF ADVANCE"
// RAMO, RATHER THAN KEEPING IT ALONGSIDE: the adversarial review of
// 95c0f20 neutralized detail::consume_optional_sign() (the SAME mutant
// that motivated the guard originally), built in REAL Release, and
// tokenized a leading "-3.5e-2" through 95c0f20's own code. The result:
// kind=number/lexeme="-" then kind=number/lexeme="3.5e-2", both with
// diagnostic.expected EMPTY. docs/api-conventions.md's own R4 fixes
// what an empty `expected` means project-wide: "no diagnostic was
// attached" - i.e. "this token is fine, keep going". Forcing an advance
// and staying silent about it manufactures exactly that false signal:
// it hands the caller two PLAUSIBLE tokens and blames nothing, when the
// defect is entirely glintfx's own. token_progress_guard.hpp already
// KNEW about the violation right there (`made_progress` was already
// false) and 95c0f20 chose not to say so - this file exists so that
// choice is never made again.
//
// EOF, NOT "KEEP GOING WITH A LOUD DIAGNOSTIC" (GODS_LAWS.md L-27,
// decision of the project leader executed here, not a spec fact - CSS
// itself has no concept of an internal implementer defect, see
// diagnostic_vocabulary.hpp's own header comment): continuing to
// produce MORE tokens after an internal defect only grows the surface
// of plausible-looking, wrong output. Ending the stream with a single
// diagnosed <EOF-token> is the smallest, most legible signal a
// consumer's own recovery code (already written, by this library's own
// R4/R7 conventions, to treat a non-empty `expected` as "stop trusting
// this parse") can act on without a new code path just for this case.
//
// PINS THE CURSOR AT source.size() (the WHOLE remainder of the buffer,
// not just past the violation): a caller that ignores the `false`
// return and calls gltfx_gfss_next_token() again on the SAME cursor
// (any shape of loop, not only the canonical `while` tokenizer.hpp's
// own header comment documents) then reaches genuine <EOF-token>
// STRUCTURALLY - dispatch_token()'s own `current == -1` branch, the
// SAME one a normal, well-formed source hits at its true end, not a
// second call into this file. This is what makes the guarantee hold
// for ANY shape of caller loop, not just the one this slice's own test
// happens to write.

namespace glintfx::style::detail {

// `violation_line`/`violation_column` are the VIOLATION's own position
// - gltfx_gfss_next_token()'s start_line/start_column, i.e. where the
// token that failed to advance BEGAN (and, because it made ZERO
// progress, is also exactly where the cursor still sits at the moment
// of the call) - never re-read off `cursor` AFTER this function has
// pinned it, which would report the END of the buffer instead of the
// point that actually failed.
[[nodiscard]] constexpr gltfx_gfss_token
recover_from_forward_progress_violation(gltfx_gfss_cursor &cursor, std::uint32_t violation_line,
                                        std::uint32_t violation_column) noexcept {
    cursor.byte_offset = cursor.source.size();
    return gltfx_gfss_token{
        .kind = gltfx_gfss_token_kind::eof,
        .lexeme = std::string_view{},
        .line = violation_line,
        .column = violation_column,
        .diagnostic =
            gltfx_gfss_diagnostic{
                .line = violation_line,
                .column = violation_column,
                .expected = k_expected_internal_tokenizer_defect,
            },
    };
}

} // namespace glintfx::style::detail
