// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <compare>
#include <cstdint>
#include <limits>

#include "selector_ast.hpp"

// specificity.hpp - GFSS-SPECIFICITY (docs/plano-w6-folha-de-estilo.md
// fatia S-1, TODO.md, GODS_LAWS.md L-17/L-19/L-20/L-27/L-40): CSS
// Selectors Level 4 section 17's own three-component specificity
// vector, read under GODS_LAWS.md L-29 - one COUNT per column (never a
// packed integer, D-W6-1 of the plan above: a hundred classes must
// never overflow into a single id's own weight, the exact defect the
// CSS-Tricks article the plan cites already documents for that
// shortcut), compared LEXICOGRAPHICALLY, ids first, classes second,
// types last.
//
// THREE std::uint32_t, NOT A PACKED INTEGER OR THREE 8-BIT COUNTERS
// (D-W6-1's own (a)/(c) options, both rejected in the plan): a packed
// integer lets enough classes carry into the id column by accident: an
// 8-bit counter saturates too soon for a machine-generated stylesheet.
// gfss_specificity below stores the three columns as separate, wide
// counters and compares them with a LEXICOGRAPHIC operator<=> (ids,
// then classes, then types - the SAME order the three fields are
// declared in, which is exactly what a defaulted member operator<=>
// compares by, C++20 [class.spaceship]) - never a single scalar a
// caller could be tempted to add or subtract.
//
// WHAT EACH COLUMN COUNTS (dossie/CSS Selectors Level 4 SS17, cross-
// checked against docs/plano-w6-folha-de-estilo.md SS1): `ids` is the
// number of ID selectors ("#foo"); `classes` is the number of class
// selectors ("*.foo*"), ATTRIBUTE selectors ("[foo=bar]" - dossie
// SS2.1's own documented surprise, "pesa como classe, nunca como id",
// answered here by construction) and PSEUDO-CLASSES (":hover",
// ":nth-child(2)", ...); `types` is the number of type selectors
// ("div") and PSEUDO-ELEMENTS ("::before"). The universal selector
// ("*") and every combinator touch NONE of the three columns - CSS
// Selectors Level 4 SS17's own text, "the universal selector ... has
// no effect on specificity".
//
// `:not()` IS THE ONE EXCEPTION TO "SUM EVERY SIMPLE SELECTOR OF A
// COMPOUND" (dossie SS2.2, already decided before this fatia opened,
// TODO.md:401): it contributes the specificity of its own MOST
// SPECIFIC argument - never the SUM of its arguments, and never a
// contribution from `:not()` itself besides. specificity_of_simple()
// below is where this is applied (see its own body comment) -
// `#x:not(#y)` is `(2, 0, 0)`, not `(1, 0, 0)` plus some fixed
// "pseudo-class" unit on top of that.
//
// EVERY OTHER FUNCTIONAL PSEUDO-CLASS THIS TRACK RECOGNIZES TODAY
// (nth-child/nth-last-child/nth-of-type/nth-last-of-type,
// selector_pseudo_vocabulary.hpp's own GLINTFX_GFSS_FUNCTIONAL_PSEUDO_
// LIST) IS AN ORDINARY PSEUDO-CLASS FOR THIS PURPOSE - CSS Selectors
// Level 4 SS17 singles out only `:not()`/`:is()`/`:has()` (and,
// separately, `:where()` - GFSS-SEL-ZERO-SPECIFICITY, fatia S-4, not
// yet parsed when this fatia opened) for special treatment; the regra
// de paridade this onda's own plan states at its own top ("se o padrao
// aceita, o gfss aceita") is exactly why an nth-* pseudo-class counts
// as a plain class-column unit here, the same as ":hover" does.
//
// PURE FUNCTIONS OVER AN ALREADY-PARSED AST, NO DIAGNOSTIC OF ITS OWN
// (unlike selector_parse.hpp's own parse_selector_list()): every
// selector this file is handed already parsed successfully - there is
// no "invalid specificity" outcome, only a value, so every function
// below returns gfss_specificity directly, never a diagnostic-shaped
// result the way the parser does.
//
// INTERNAL IN THIS SLICE, ON PURPOSE (GODS_LAWS.md L-19, the SAME
// reasoning selector_ast.hpp's own header comment already gives for
// this track): lives under src/gfss/, not include/glintfx/ -
// GFSS-API (TODO.md, wave W10) is the dedicated review that decides
// whether/how a consumer ever reads a rule's own specificity directly.
// Nothing here is ABI-frozen.

namespace glintfx::style::detail {

// One column count per CSS Selectors Level 4 SS17 component - see this
// file's own header comment above for what each column counts.
// Comparison is DEFAULTED, never hand-written: C++20's own
// [class.spaceship] rule compares three-way by declaration order
// (ids, classes, types), which IS the lexicographic order this whole
// track relies on - a hand-written comparator here would just be that
// same rule spelled out a second time, one column reorder away from
// silently drifting from it (CONTRACT.md SS6.7's own "duplicacao
// real" test).
struct gfss_specificity {
    std::uint32_t ids = 0;
    std::uint32_t classes = 0;
    std::uint32_t types = 0;

    [[nodiscard]] constexpr auto operator<=>(const gfss_specificity &) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const gfss_specificity &) const noexcept = default;
};

// Adds one to `value`, clamped at std::uint32_t's own maximum instead
// of wrapping to zero (GODS_LAWS.md L-43: the library never trusts a
// stylesheet's own author to be well-behaved, LEI ZERO's own "base de
// consumidores aberta e desconhecida" - a machine-generated selector
// list with an absurd id count must saturate, never wrap around into a
// SMALLER specificity than a normal selector's own). The single atom
// specificity_of_compound()/specificity_of_complex() (specificity.cpp)
// both call once per contributing simple selector - gfss_specificity_
// test.cpp's own saturation case proves the boundary (std::uint32_t's
// own max stays put) and one call PAST it (still put, never wraps).
[[nodiscard]] constexpr std::uint32_t saturating_increment(std::uint32_t value) noexcept {
    return value == std::numeric_limits<std::uint32_t>::max() ? value : value + 1;
}

// The column ONE simple selector touches - see this file's own header
// comment above for the full mapping, and specificity.cpp's own body
// comment on the `pseudo_function`/"not" case for the one exception.
[[nodiscard]] gfss_specificity specificity_of_simple(const gfss_simple_selector &simple) noexcept;

// The sum of every simple selector's own contribution in one compound
// selector (e.g. "button.primary#ok" sums three simple selectors' own
// columns) - see specificity.cpp.
[[nodiscard]] gfss_specificity
specificity_of_compound(const gfss_compound_selector &compound) noexcept;

// The sum across every compound of a complex selector (head plus every
// compound in `rest`) - a combinator itself contributes to NO column
// (this file's own header comment above), so only the compounds are
// summed, never the combinators between them. See specificity.cpp.
[[nodiscard]] gfss_specificity
specificity_of_complex(const gfss_complex_selector &complex_selector) noexcept;

} // namespace glintfx::style::detail
