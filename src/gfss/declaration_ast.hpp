// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <glintfx/core/color.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/value.hpp>

// declaration_ast.hpp - GFSS-DECL-PARSE (TODO.md wave W5, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-27/L-28/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS3): the shape one READ
// declaration takes, and the shape the whole block's read produces
// (declaration_list_parse_result). Answers exactly one question - "how
// is a declaration this track has already accepted represented?" -
// never how it got there (declaration_parse.hpp/.cpp) nor where the
// block's own boundaries are (declaration_split.hpp/.cpp).
//
// INTERNAL, ON PURPOSE (GODS_LAWS.md L-19: "o header nasce interno, em
// src/gfss/"): the SAME "public type via value.hpp/token.hpp, internal
// parser" split value_parse.hpp/color_parse.hpp/selector_parse.hpp
// already establish for this track. Nothing here is GLINTFX_API; GFSS-
// API (TODO.md, wave W10) decides the public shape.
//
// FIVE FORMS, NEVER FOLDED INTO ONE (plan SS3, D-DP-7/D-DP-6): a
// declaration's own VALUE is exactly one of five shapes - the three
// universal keywords (`inherit`/`initial`/`unset`, values empty), a
// list of decoded gltfx_gfss_value components (the ordinary case), a
// resolved color (`gltfx_rgba`), the `currentColor` keyword (its own
// form, D-DP-6 - a color property's value that is NOT a literal color
// yet, resolved later by GFSS-INHERIT), or raw tokens (an accepted
// shorthand pending expansion, D-DP-5, or a composite type this
// registry does not congeal, D-DP-4's own "raw_composite" contract
// mark). Keeping these five apart, instead of cramming color into
// `values` or `raw` into `values`, is the SAME "never claim to know
// more than was actually determined" principle GODS_LAWS.md L-27 names
// project-wide: a consumer reading `form` never has to guess which
// OTHER field is meaningful.

namespace glintfx::style::detail {

enum class gfss_declaration_value_form : std::uint8_t {
    universal_keyword, // inherit/initial/unset - `values`/`color`/`raw_tokens` all empty
    values,            // one or more decoded gltfx_gfss_value components
    color,             // a resolved gltfx_rgba (D-DP-6, from GFSS-COLOR-PARSE)
    current_color,     // the currentColor keyword itself - resolved by GFSS-INHERIT, D18
    raw,               // shorthand-pending-expansion (D-DP-5) or a raw_composite contract (D-DP-4)
};

enum class gfss_universal_keyword : std::uint8_t {
    inherit_keyword,
    initial_keyword,
    unset_keyword,
};

// One declaration this fatia's own read-a-declaration pipeline (plan
// SS2, D-DP-8) has ACCEPTED - a rejected declaration never becomes one
// of these; it is a gltfx_gfss_diagnostic in the block result's own
// `rejected` list instead (declaration_list_parse_result below).
struct gfss_declaration {
    // A property this registry knows (is_shorthand == false) or one of
    // the eleven accepted shorthand names, crude, pending expansion by
    // GFSS-SHORTHAND (is_shorthand == true, D-DP-5) - never both.
    bool is_shorthand = false;
    gltfx_gfss_property property = gltfx_gfss_property::display; // valid iff !is_shorthand
    std::string_view shorthand_name; // valid iff is_shorthand, sheet spelling

    bool important = false;

    gfss_declaration_value_form form = gfss_declaration_value_form::values;
    gfss_universal_keyword universal =
        gfss_universal_keyword::inherit_keyword; // form == universal_keyword
    std::vector<gltfx_gfss_value> values;        // form == values
    gltfx_rgba color{};                          // form == color
    std::vector<gltfx_gfss_token> raw_tokens;    // form == raw

    // D-DP-2: the aviso preso a UMA declaracao aceita, distinto de uma
    // declaracao rejeitada. Set when gfss_property_status(property) ==
    // reserved (E1) at the moment this declaration was read - never
    // meaningful when is_shorthand is true (a shorthand name carries no
    // gfss_property_status of its own).
    bool has_reserved_notice = false;
};

// The whole block's own read (plan SS3's own "e a entrada"): every
// declaration this pass ACCEPTED, every diagnostic for one it REJECTED
// (D-DP-2's other list), and the two counters GODS_LAWS.md L-40
// requires printed even when the underlying condition never fires
// (raw_composite_count for D-DP-4's own declared debt; reserved_notice_
// count for E1's own "ruido do property_reserved").
struct declaration_list_parse_result {
    std::vector<gfss_declaration> declarations;
    std::vector<gltfx_gfss_diagnostic> rejected;
    std::size_t raw_composite_count = 0;
    std::size_t reserved_notice_count = 0;
};

} // namespace glintfx::style::detail
