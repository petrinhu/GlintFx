// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>

// ascii_case.hpp - GFSS-MATCH-SIMPLE fatia A (TODO.md, GODS_LAWS.md
// L-17/L-27/L-33/L-40; /var/tmp/glintfx-plan/gfss-match-simple-plano.md
// SS3.7, decision D-MS-3): ASCII case-insensitive comparison, moved
// VERBATIM out of named_colors.hpp - same namespace, same functions,
// same behavior, only the file that owns them changes.
//
// WHY THIS MOVE, NOW: named_colors.hpp already had THREE real call
// sites for these two functions (named_colors.cpp's own table lookup,
// color_parse.cpp's function-name match, selector_pseudo_vocabulary.
// hpp's own pseudo-class name match) - past CONTRACT.md SS6's "three
// occurrences" bar for a shared helper. GFSS-MATCH-SIMPLE's own
// compound_match.cpp is the FOURTH consumer, for tag and pseudo-class
// name comparison (the plan's own D-MS-4 policy). Including named_
// colors.hpp from src/gfui/ just to reach this one pair of functions
// would drag the whole 149-row named-color TABLE declaration into the
// motor layer (ESCOPO.md SS4: gfui is the motor, not the leaf format)
// - GODS_LAWS.md L-17's own "what comes in through the include"
// question, failing. The fix is a lar (home) move, not a behavior
// change: same two functions, same namespace, no call site anywhere in
// the tree changes what it computes - only where it #includes them
// from.
//
// REFACTOR UNDER A GREEN SUITE (GODS_LAWS.md L-20 step 3): no new red
// case exists for this fatia. gfss_color_parse_test and gfss_selector_
// parse_test (the suite's own two consumers at write time) stay green
// before and after, same case count both times.

namespace glintfx::style::detail {

// U+0041-U+005A ('A'-'Z') and U+0061-U+007A ('a'-'z') only - the ASCII
// range CSS Syntax Module Level 3's own case-insensitive ASCII match
// covers (section 4.2's "ASCII case-insensitive" definition never
// touches non-ASCII code points).
[[nodiscard]] constexpr char ascii_to_lower(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

[[nodiscard]] constexpr bool ascii_case_insensitive_equal(std::string_view a,
                                                          std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_to_lower(a[i]) != ascii_to_lower(b[i])) {
            return false;
        }
    }
    return true;
}

} // namespace glintfx::style::detail
