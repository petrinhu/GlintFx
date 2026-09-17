// SPDX-License-Identifier: AGPL-3.0-or-later
#include "shorthand_expand.hpp"

#include <optional>

#include "ascii_case.hpp"
#include "diagnostic_vocabulary.hpp"
#include "property_status.hpp"
#include "shorthand_axis_pair.hpp"
#include "shorthand_border_parts.hpp"
#include "shorthand_flex_flow.hpp"
#include "shorthand_four_corners.hpp"
#include "shorthand_four_sides.hpp"
#include "shorthand_longhand_table.hpp"

// shorthand_expand.cpp - GFSS-SHORTHAND (GODS_LAWS.md L-17: each
// function below answers exactly one question of shorthand_expand.hpp's
// own header comment scope). The universal-keyword scan below is a
// DELIBERATE, small duplicate of declaration_parse.cpp's own
// scan_for_universal_keyword() (that file's anonymous namespace, not
// reachable from here) - CONTRACT.md SS6.7's "regra de tres" is not yet
// triggered (this is only the SECOND real occurrence), and the two
// scans differ in what they receive (a `[first, last]` index pair into
// the BLOCK's own token vector there, a whole `raw_tokens` vector of its
// own here) enough that sharing one function would need an extra
// indirection neither caller wants.

namespace glintfx::style::detail {

namespace {

[[nodiscard]] std::optional<gfss_universal_keyword>
universal_keyword_for(std::string_view text) noexcept {
    if (ascii_case_insensitive_equal(text, "inherit")) {
        return gfss_universal_keyword::inherit_keyword;
    }
    if (ascii_case_insensitive_equal(text, "initial")) {
        return gfss_universal_keyword::initial_keyword;
    }
    if (ascii_case_insensitive_equal(text, "unset")) {
        return gfss_universal_keyword::unset_keyword;
    }
    return std::nullopt;
}

struct universal_scan_outcome {
    bool is_universal_alone = false;
    bool found_but_mixed = false;
    gfss_universal_keyword keyword = gfss_universal_keyword::inherit_keyword;
    std::size_t mixed_position = 0; // valid iff found_but_mixed
};

[[nodiscard]] universal_scan_outcome
scan_shorthand_for_universal_keyword(const std::vector<gltfx_gfss_token> &tokens) {
    if (tokens.size() == 1 && tokens[0].kind == gltfx_gfss_token_kind::ident) {
        if (const std::optional<gfss_universal_keyword> keyword =
                universal_keyword_for(tokens[0].lexeme)) {
            return universal_scan_outcome{.is_universal_alone = true, .keyword = *keyword};
        }
    }
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].kind == gltfx_gfss_token_kind::ident &&
            universal_keyword_for(tokens[i].lexeme)) {
            return universal_scan_outcome{.found_but_mixed = true, .mixed_position = i};
        }
    }
    return universal_scan_outcome{};
}

[[nodiscard]] shorthand_expand_result
expand_to_universal_keyword(const shorthand_longhand_entry &entry, gfss_universal_keyword keyword,
                            bool important) {
    shorthand_expand_result result;
    result.ok = true;
    result.longhands.reserve(entry.longhand_count);
    for (std::uint8_t i = 0; i < entry.longhand_count; ++i) {
        gfss_declaration declaration;
        declaration.is_shorthand = false;
        declaration.property = entry.longhands[i];
        declaration.important = important;
        declaration.form = gfss_declaration_value_form::universal_keyword;
        declaration.universal = keyword;
        declaration.has_reserved_notice =
            gfss_property_status(entry.longhands[i]) == property_status::reserved;
        result.longhands.push_back(std::move(declaration));
    }
    return result;
}

[[nodiscard]] shorthand_expand_result dispatch_family(const std::vector<gltfx_gfss_token> &tokens,
                                                      const shorthand_longhand_entry &entry,
                                                      bool important) {
    switch (entry.family) {
    case shorthand_family::four_sides:
        return expand_four_sides(tokens, entry, important);
    case shorthand_family::four_corners:
        return expand_four_corners(tokens, entry, important);
    case shorthand_family::axis_pair:
        return expand_axis_pair(tokens, entry, important);
    case shorthand_family::border_parts:
        return expand_border_parts(tokens, entry, important);
    case shorthand_family::flex_flow:
        return expand_flex_flow(tokens, entry, important);
    }
    return shorthand_expand_result{};
}

} // namespace

shorthand_expand_result expand_shorthand(const gfss_declaration &shorthand) {
    const shorthand_longhand_entry *entry = find_shorthand_longhand_entry(shorthand.shorthand_name);
    if (entry == nullptr) {
        // Should not happen for any caller inside this library's own
        // sources: `shorthand.shorthand_name` is only ever set from
        // k_shorthand_names (declaration_parse.cpp's own
        // build_shorthand()), and shorthand_longhand_table.hpp's own
        // every_vocabulary_shorthand_has_a_table_row() static_assert
        // (GODS_LAWS.md L-36/L-40, finding #1 of the 16/09/2026
        // adversarial review - the OLD assert there only ever proved a
        // number equal to itself, never that every name actually had a
        // row) now really does prove every vocabulary name has a
        // matching row. That proof covers THIS library's own closed
        // table, never an arbitrary caller-supplied std::string_view -
        // GODS_LAWS.md LEI ZERO's own "base de consumidores aberta e
        // desconhecida" means this function refuses to trust an
        // invariant it cannot itself verify. A declared, diagnosed
        // refusal, never a dereference of `entry` (GODS_LAWS.md L-22: no
        // UB crosses a function this library exposes, the same
        // discipline "no exception crosses the public API" already
        // applies to).
        return shorthand_expand_result{
            .ok = false,
            .longhands = {},
            .diagnostic = gltfx_gfss_diagnostic{
                .line = shorthand.raw_tokens.empty() ? 0 : shorthand.raw_tokens.front().line,
                .column = shorthand.raw_tokens.empty() ? 0 : shorthand.raw_tokens.front().column,
                .expected = k_expected_internal_shorthand_table_defect,
                .detail = {}}};
    }

    const universal_scan_outcome universal =
        scan_shorthand_for_universal_keyword(shorthand.raw_tokens);
    if (universal.found_but_mixed) {
        const gltfx_gfss_token &at = shorthand.raw_tokens[universal.mixed_position];
        return shorthand_expand_result{
            .ok = false,
            .longhands = {},
            .diagnostic = gltfx_gfss_diagnostic{.line = at.line,
                                                .column = at.column,
                                                .expected = k_expected_universal_keyword_alone,
                                                .detail = {}}};
    }
    if (universal.is_universal_alone) {
        return expand_to_universal_keyword(*entry, universal.keyword, shorthand.important);
    }

    return dispatch_family(shorthand.raw_tokens, *entry, shorthand.important);
}

} // namespace glintfx::style::detail
