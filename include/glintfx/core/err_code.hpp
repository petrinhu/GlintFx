// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// core/err_code.hpp - the numeric identity of every error glintfx can
// report (CE-1 of CORE-ERROR, TODO.md, GODS_LAWS.md L-22/L-26).
//
// NAME, CORRECTED (CORE-ERROR, adversarial review, 25/08/2026): the
// first cut of this file spelled the type `glintfx::error_code`. Built
// against a TU that had not yet pulled in this header, clang's own
// diagnostic offered "did you mean 'std::error_code'?" - proof, from
// the compiler itself, that the unprefixed name is one character away
// from a REAL, WIDELY-USED standard-library type with DIFFERENT
// semantics (std::error_code carries a std::error_category, this type
// does not). If the compiler reaches for the standard library first,
// an external consumer will too. `gltfx_err_code` carries the same
// short prefix the leader chose for its two siblings on this same
// public surface (`gltfx_err`, `gltfx_rslt` - CE-2/CE-4) for exactly
// this reason: this project's checklist below already screened for
// collision with SYSTEM macros; it had not yet screened for collision
// with STANDARD-LIBRARY names, and `error`/`error_code`/`result`/
// `expected` all live there. Both checks are mandatory from here on
// for any name on this public surface.
//
// APPEND-ONLY CONTRACT (decision of the leader, TODO.md CORE-ERROR row,
// 25/08/2026): once a value ships, its NAME and its NUMBER never change
// and are never reused for a different meaning, even if the original
// use is later judged a mistake. Adding a value is compatible (bumps
// B). Renumbering or removing one breaks a consumer's already-compiled
// switch/if-chain (bumps A, GODS_LAWS.md L-26/CONTRACT.md 13.2). If a
// value is ever retired, its row stays in the table (err_code.cpp),
// documented as retired; the number is never given to a new meaning.
//
// RESERVED RANGES, one block per domain, so a later slice adds values
// without ever touching a range another domain already owns:
//   0          sentinel: unknown / not recognized by THIS build's
//              table (see gltfx_err_code_name() below - the fallback
//              for a value produced by a NEWER glintfx than the one
//              reading it; graceful degradation, never undefined
//              behavior)
//   1..999     generic (this file, v1; see the enumerators below)
//   1000..1999 reserved: resource/asset domain (TODO.md ASSET-LOAD)
//   2000..2999 reserved: map domain (TODO.md MAP-* slices)
//   3000..3999 reserved: style/RCSS domain (TODO.md RCSS-* slices)
//   4000+      reserved for a domain not yet named; the NEXT one to
//              need codes appends the next free block here, never
//              reuses a gap
//
// TABLE OF DATA, ONCE (GODS_LAWS.md L-17): name and value are looked up
// through ONE table in err_code.cpp, not a switch that grows a case per
// feature. This header only declares the enumerators (the values
// themselves) and the single exported lookup function.
//
// COLLISION CHECKLIST (GODS_LAWS.md L-19/CORE-ERROR, mandatory before
// closing this slice - the same class of bug already bit
// glintfx/core/version.hpp's major/minor fields, see that header, and
// this file's own type name above): every identifier below was checked
// against (a) the function-like macros known to collide on this
// project's five platforms - min/max, ERROR, DELETE, IN, OUT, CONST,
// VOID, TRUE, FALSE, interface, small, near, far, STRICT (the Windows
// system headers, named in prose rather than as literal #include text
// on purpose - see the same rationale spelled out in version.hpp -
// because tests/tools/check_layers.sh greps this file for OS-header
// patterns and would trip a false positive on a comment that merely
// names them), and major/minor/makedev, stdin/stdout/stderr,
// unix/linux (glibc/POSIX headers, Linux legs); and now also (b) names
// already used by the C++ standard library, the check this same slice
// missed the first time - `error`, `error_code`, `error_condition`,
// `result`, `expected`. `gltfx_err_code`, `unknown`, `out_of_memory`,
// `io_failure`, `not_found`, `invalid_argument`, `parse_failure`,
// `unsupported`, `platform_failure` and `gltfx_err_code_name` match
// none of either list.

namespace glintfx {

// uint32_t is the width the CORE-ERROR plan specifies (TODO.md,
// "Enumeracao de 32 bits sem sinal"), not an accident clang-tidy can
// right-size down - it is also the width baked into gltfx_err's frozen
// two-pointer footprint (CE-2), and the reserved domain ranges
// documented above already reach 4000+, past what even uint16_t could
// hold alongside headroom for the next domain.
enum class gltfx_err_code : std::uint32_t { // NOLINT(performance-enum-size) reason: see the
                                            // paragraph above; 32 bits is a frozen ABI/plan
                                            // decision, not an oversight
    unknown = 0,

    // Generic band (1..999). v1 carries exactly these seven; adding an
    // eighth means adding both the enumerator here and its row in
    // err_code.cpp's table - the two never drift because the table is
    // the only place a name is produced from a value.
    out_of_memory = 1,
    io_failure = 2,
    not_found = 3,
    invalid_argument = 4,
    parse_failure = 5,
    unsupported = 6,
    platform_failure = 7,
};

// Returns the IDENTIFIER of `code` (e.g. "out_of_memory"), not a
// sentence - stable across locales and safe to log or match on.
// noexcept, never undefined behavior: a `code` this build's table does
// not recognize (including any raw value cast from an integer that is
// not one of the enumerators above) returns "unknown", the same string
// as gltfx_err_code::unknown itself.
[[nodiscard]] GLINTFX_API std::string_view gltfx_err_code_name(gltfx_err_code code) noexcept;

} // namespace glintfx
