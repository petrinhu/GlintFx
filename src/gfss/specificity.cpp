// SPDX-License-Identifier: AGPL-3.0-or-later
#include "specificity.hpp"

#include <vector>

// specificity.cpp - GFSS-SPECIFICITY (docs/plano-w6-folha-de-estilo.md
// fatia S-1, TODO.md, GODS_LAWS.md L-17/L-20/L-27/L-40): the algorithm
// behind specificity.hpp's own three functions - see that file's own
// header comment for the design rationale (why lexicographic three
// counters, why `:not()` is the one exception) this implementation
// applies rather than re-decides.

namespace glintfx::style::detail {

namespace {

// Adds `contribution`'s own three columns into `total`, one
// saturating_increment() call per unit - never a bulk `total.ids +=
// contribution.ids` (that would need a SECOND saturation primitive
// beyond the one atom specificity.hpp's own header comment names and
// gfss_specificity_test.cpp proves at the boundary). In practice each
// column of `contribution` is small: 0 or 1 for every simple-selector
// kind except `:not()`, and `:not()`'s own argument is bounded by
// selector_parse.cpp's own k_max_not_nesting_depth - so this loop never
// runs more than a handful of times for any selector this library's
// own parser can produce.
void accumulate(gfss_specificity &total, const gfss_specificity &contribution) noexcept {
    for (std::uint32_t i = 0; i < contribution.ids; ++i) {
        total.ids = saturating_increment(total.ids);
    }
    for (std::uint32_t i = 0; i < contribution.classes; ++i) {
        total.classes = saturating_increment(total.classes);
    }
    for (std::uint32_t i = 0; i < contribution.types; ++i) {
        total.types = saturating_increment(total.types);
    }
}

} // namespace

// Mutually recursive with specificity_of_complex()/specificity_of_
// compound() below, ONLY through `pseudo_function` whose `name` is
// "not" - bounded by selector_parse.cpp's own k_max_not_nesting_depth,
// the SAME limit that already bounds gfss_simple_selector::not_
// selectors's own depth at parse time (this function never re-checks
// it: by the time a gfss_simple_selector reaches here, the parser has
// already refused anything past that depth).
// NOLINTNEXTLINE(misc-no-recursion) reason: bounded by k_max_not_nesting_depth (see above).
gfss_specificity specificity_of_simple(const gfss_simple_selector &simple) noexcept {
    switch (simple.kind) {
    case gfss_simple_selector_kind::universal:
        // "*" touches no column (CSS Selectors Level 4 SS17: "the
        // universal selector ... has no effect on specificity").
        return gfss_specificity{};
    case gfss_simple_selector_kind::type:
        return gfss_specificity{.ids = 0, .classes = 0, .types = 1};
    case gfss_simple_selector_kind::class_selector:
        return gfss_specificity{.ids = 0, .classes = 1, .types = 0};
    case gfss_simple_selector_kind::id_selector:
        return gfss_specificity{.ids = 1, .classes = 0, .types = 0};
    case gfss_simple_selector_kind::pseudo_class:
        return gfss_specificity{.ids = 0, .classes = 1, .types = 0};
    case gfss_simple_selector_kind::pseudo_function:
        if (simple.name == "not") {
            // dossie SS2.2 / TODO.md:401, already decided before this
            // fatia opened: `:not()` contributes the specificity of its
            // own MOST SPECIFIC argument, never the SUM of its
            // arguments, and never a contribution from `:not()` itself
            // besides (no "+1 class for being a pseudo-class" on top).
            gfss_specificity most_specific{};
            for (const gfss_complex_selector &nested : simple.not_selectors) {
                const gfss_specificity candidate = specificity_of_complex(nested);
                if (most_specific < candidate) {
                    most_specific = candidate;
                }
            }
            return most_specific;
        }
        // Every OTHER functional pseudo-class this fatia recognizes
        // (nth-child/nth-last-child/nth-of-type/nth-last-of-type,
        // selector_pseudo_vocabulary.hpp's own GLINTFX_GFSS_FUNCTIONAL_
        // PSEUDO_LIST) is an ORDINARY pseudo-class for this purpose -
        // specificity.hpp's own header comment above explains why.
        return gfss_specificity{.ids = 0, .classes = 1, .types = 0};
    case gfss_simple_selector_kind::pseudo_element:
        return gfss_specificity{.ids = 0, .classes = 0, .types = 1};
    case gfss_simple_selector_kind::attribute:
        // dossie SS2.1: "[id=\"x\"]" weighs as a CLASS, never as an id -
        // the exact surprise the dossier documents authors hitting,
        // answered here by construction (this file's own header
        // comment above).
        return gfss_specificity{.ids = 0, .classes = 1, .types = 0};
    }
    // Every gfss_simple_selector_kind enumerator is handled above with
    // NO default label on purpose (the SAME closed-enumeration
    // technique src/gfui/compound_match.cpp's own collect_requirements()
    // already uses for this exact enum, GODS_LAWS.md L-40 achado 1): a
    // 9th kind added to selector_ast.hpp with no matching case here
    // fails to COMPILE under -Werror (-Wswitch), never silently returns
    // a zero specificity. This line only satisfies "a non-void function
    // must return on every path" for a compiler that does not itself
    // prove the switch above exhaustive.
    return gfss_specificity{};
}

// Same bounded-by-k_max_not_nesting_depth recursion as specificity_of_
// simple() above.
// NOLINTNEXTLINE(misc-no-recursion) reason: see specificity_of_simple() above.
gfss_specificity specificity_of_compound(const gfss_compound_selector &compound) noexcept {
    gfss_specificity total{};
    for (const gfss_simple_selector &simple : compound.simple_selectors) {
        accumulate(total, specificity_of_simple(simple));
    }
    return total;
}

// Same bounded-by-k_max_not_nesting_depth recursion as specificity_of_
// simple() above.
// NOLINTNEXTLINE(misc-no-recursion) reason: see specificity_of_simple() above.
gfss_specificity specificity_of_complex(const gfss_complex_selector &complex_selector) noexcept {
    // `head` first, then every compound in `rest` - the combinator
    // itself (gfss_combined_selector::combinator) is never read here:
    // it contributes to no column (specificity.hpp's own header
    // comment above), so only the compound half of each gfss_combined_
    // selector is summed.
    gfss_specificity total = specificity_of_compound(complex_selector.head);
    for (const gfss_combined_selector &combined : complex_selector.rest) {
        accumulate(total, specificity_of_compound(combined.compound));
    }
    return total;
}

} // namespace glintfx::style::detail
