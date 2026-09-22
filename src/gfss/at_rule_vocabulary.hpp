// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ascii_case.hpp"

// at_rule_vocabulary.hpp - GFSS-SHEET-PARSE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-28/L-40): the CLOSED list of at-rule names sheet_parse.cpp
// recognizes by name, and what each one means - the SAME "closed
// membership, one question answered" shape selector_pseudo_vocabulary.hpp
// already establishes for pseudo-class/pseudo-element names, and the
// SAME X-macro technique diagnostic_vocabulary.hpp/token.hpp already use
// (GODS_LAWS.md L-40 achado 1, "a enumeracao fechada nao e fechada" - a
// hand-copied second list drifts, an X-macro-derived count cannot).
//
// TWO NAMES, TWO DIFFERENT FATES (TODO.md's own GFSS-SHEET-PARSE row,
// REESCRITA 26/08/2026, GODS_LAWS.md L-28 decisions 23-24):
// - "media" -> `ignored_with_diagnostic`: recognized, its whole block
//   skipped, ONE diagnostic per occurrence naming it as out-of-v1 -
//   never treated as an error (ESCOPO.md's own decision 4 of 21/08/2026,
//   still standing).
// - "keyframes" -> `raw`: recognized, its whole block captured BYTE FOR
//   BYTE and handed to sheet_ast.hpp's own gfss_raw_block, for
//   ANIM-KEYFRAMES (TODO.md, wave W10) to parse later - the animation
//   scope revoked the old "fora da v1, com diagnostico" fate for this
//   ONE name (L-28 decisions 23-24, ESCOPO.md's own "As 8 decisoes de
//   26/08/2026 sobre movimento e luz"); no diagnostic is ever attached
//   to a recognized "keyframes" occurrence, unlike "media" above - it is
//   accepted V1 vocabulary, not a deferred one.
//
// ANY OTHER at-keyword sheet_parse.cpp encounters is NOT in this list at
// all - at_rule_verdict_for() below returns std::nullopt for it, and the
// caller (sheet_parse.cpp's own handle_at_rule()) is what decides that
// means "unknown at-rule", with its own diagnostic naming what IS
// supported (this file's own k_at_rule_names, joined, becomes that
// diagnostic's `detail`).
//
// CASE-INSENSITIVE, the SAME rule named_colors.hpp's own
// ascii_case_insensitive_equal() already establishes for THIS track's
// keyword matching (selector_pseudo_vocabulary.hpp's own header comment
// makes the identical argument for pseudo-class names) - CSS Syntax
// Module Level 3's own <at-keyword-token> is an <ident-token> under the
// hood, and every ident this track already matches case-insensitively is
// vocabulary the FORMAT defines, never an author-chosen identifier.

namespace glintfx::style::detail {

enum class at_rule_verdict : std::uint8_t {
    ignored_with_diagnostic, // "media" - recognized, skipped, ONE diagnostic per occurrence
    raw, // "keyframes" - recognized, block captured byte for byte, no diagnostic
};

#define GLINTFX_GFSS_AT_RULE_LIST(X)                                                               \
    X("media", ignored_with_diagnostic)                                                            \
    X("keyframes", raw)

inline constexpr std::size_t k_at_rule_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_AT_RULE_COUNT_ONE(name, verdict) ++count;
    GLINTFX_GFSS_AT_RULE_LIST(GLINTFX_GFSS_AT_RULE_COUNT_ONE)
#undef GLINTFX_GFSS_AT_RULE_COUNT_ONE
    return count;
}();

struct at_rule_entry {
    std::string_view name{};
    at_rule_verdict verdict{};
};

inline constexpr std::array<at_rule_entry, k_at_rule_count> k_at_rule_entries{
#define GLINTFX_GFSS_AT_RULE_ENTRY_ONE(name, verdict)                                              \
    at_rule_entry{std::string_view{name}, at_rule_verdict::verdict},
    GLINTFX_GFSS_AT_RULE_LIST(GLINTFX_GFSS_AT_RULE_ENTRY_ONE)
#undef GLINTFX_GFSS_AT_RULE_ENTRY_ONE
};

// The space-separated names this v1 recognizes, joined ONCE at compile
// time - sheet_parse.cpp's own diagnostic for an UNKNOWN at-rule uses
// this, verbatim, as its `detail` field (docs/api-conventions.md R7: a
// space-separated list of vocabulary identifiers, never a sentence - the
// SAME convention refused_property_names.hpp's own detail already
// follows for a refused shorthand's longhand names).
inline constexpr std::array<std::string_view, k_at_rule_count> k_at_rule_names{
#define GLINTFX_GFSS_AT_RULE_NAME_ONE(name, verdict) std::string_view{name},
    GLINTFX_GFSS_AT_RULE_LIST(GLINTFX_GFSS_AT_RULE_NAME_ONE)
#undef GLINTFX_GFSS_AT_RULE_NAME_ONE
};

#undef GLINTFX_GFSS_AT_RULE_LIST

// `name` is the at-keyword's own lexeme WITHOUT the leading '@'
// (sheet_parse.cpp's own at_rule_name() strips it, the same way
// selector_parse.cpp's own function_name() strips a functional
// pseudo-class token's own trailing '('). std::nullopt means "not in
// this closed list" - the caller's own job to turn that into an
// "unknown at-rule" diagnostic, never this function's (this file only
// answers the closed-membership question, the SAME division of labor
// is_known_simple_pseudo()/is_known_functional_pseudo()
// (selector_pseudo_vocabulary.hpp) already establish).
[[nodiscard]] constexpr std::optional<at_rule_verdict>
at_rule_verdict_for(std::string_view name) noexcept {
    for (const at_rule_entry &entry : k_at_rule_entries) {
        if (ascii_case_insensitive_equal(name, entry.name)) {
            return entry.verdict;
        }
    }
    return std::nullopt;
}

} // namespace glintfx::style::detail
