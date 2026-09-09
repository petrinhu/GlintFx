// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_parse.hpp"

#include <optional>

#include "ascii_case.hpp"
#include "declaration_color_value.hpp"
#include "declaration_value_check.hpp"
#include "diagnostic_vocabulary.hpp"
#include "important_flag.hpp"
#include "property_name_lookup.hpp"
#include "property_status.hpp"
#include "property_value_contract.hpp"
#include "token_span_trim.hpp"

// declaration_parse.cpp - GFSS-DECL-PARSE, DP-7 (GODS_LAWS.md L-17:
// each function below answers exactly one question of declaration_
// parse.hpp's own header comment scope - one step of D-DP-8's own fixed
// order, never more than one per function).

namespace glintfx::style::detail {

namespace {

[[nodiscard]] declaration_parse_result rejected(gltfx_gfss_diagnostic diagnostic) {
    return declaration_parse_result{.accepted = false, .declaration = {}, .diagnostic = diagnostic};
}

[[nodiscard]] declaration_parse_result accepted(gfss_declaration declaration) {
    return declaration_parse_result{
        .accepted = true, .declaration = std::move(declaration), .diagnostic = {}};
}

// --- name and colon ---------------------------------------------------

struct name_and_colon_outcome {
    bool ok = false;
    std::size_t value_begin = 0; // first index AFTER the ':'
    property_name_lookup_result lookup{};
    gltfx_gfss_diagnostic diagnostic{}; // valid iff !ok
};

[[nodiscard]] name_and_colon_outcome
read_name_and_colon(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin,
                    std::size_t end) {
    const trimmed_token_span span = trim_whitespace_tokens(tokens, begin, end);
    if (!span.has_content || tokens[span.first].kind != gltfx_gfss_token_kind::ident) {
        const std::size_t at = span.has_content ? span.first : begin;
        return name_and_colon_outcome{
            .ok = false,
            .diagnostic = gltfx_gfss_diagnostic{.line = tokens[at].line,
                                                .column = tokens[at].column,
                                                .expected = k_expected_property_name,
                                                .detail = {}}};
    }

    const gltfx_gfss_token &name_token = tokens[span.first];
    const property_name_lookup_result lookup =
        lookup_property_name(name_token.lexeme, name_token.line, name_token.column);
    if (lookup.kind == property_name_lookup_kind::refused ||
        lookup.kind == property_name_lookup_kind::unknown) {
        return name_and_colon_outcome{.ok = false, .diagnostic = lookup.diagnostic};
    }

    std::size_t colon_index = span.first + 1;
    while (colon_index <= span.last &&
           tokens[colon_index].kind == gltfx_gfss_token_kind::whitespace) {
        ++colon_index;
    }
    if (colon_index > span.last || tokens[colon_index].kind != gltfx_gfss_token_kind::colon) {
        return name_and_colon_outcome{
            .ok = false,
            .diagnostic = gltfx_gfss_diagnostic{.line = name_token.line,
                                                .column = name_token.column,
                                                .expected = k_expected_colon_after_property_name,
                                                .detail = {}}};
    }

    return name_and_colon_outcome{.ok = true, .value_begin = colon_index + 1, .lookup = lookup};
}

// --- the three universal keywords, D-DP-7 -----------------------------

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

// Scans `[first, last]` (inclusive, already trimmed) for the closed
// vocabulary of universal keywords - D-DP-7's own "so como valor
// inteiro": a match is only a VALID universal declaration when it is
// the span's OWN only non-whitespace token; found anywhere else, it is
// `universal_keyword_alone` (esbuild #1702, plan SS1.2).
[[nodiscard]] universal_scan_outcome
scan_for_universal_keyword(const std::vector<gltfx_gfss_token> &tokens, std::size_t first,
                           std::size_t last) {
    if (first == last && tokens[first].kind == gltfx_gfss_token_kind::ident) {
        if (const std::optional<gfss_universal_keyword> keyword =
                universal_keyword_for(tokens[first].lexeme)) {
            return universal_scan_outcome{.is_universal_alone = true, .keyword = *keyword};
        }
    }
    for (std::size_t i = first; i <= last; ++i) {
        if (tokens[i].kind == gltfx_gfss_token_kind::ident &&
            universal_keyword_for(tokens[i].lexeme)) {
            return universal_scan_outcome{.found_but_mixed = true, .mixed_position = i};
        }
    }
    return universal_scan_outcome{};
}

// --- building the accepted declaration's own value shape --------------

[[nodiscard]] declaration_parse_result build_shorthand(const std::vector<gltfx_gfss_token> &tokens,
                                                       std::string_view shorthand_name,
                                                       bool important,
                                                       const trimmed_token_span &value_span) {
    gfss_declaration declaration;
    declaration.is_shorthand = true;
    declaration.shorthand_name = shorthand_name;
    declaration.important = important;
    declaration.form = gfss_declaration_value_form::raw;
    for (std::size_t i = value_span.first; i <= value_span.last; ++i) {
        declaration.raw_tokens.push_back(tokens[i]);
    }
    return accepted(std::move(declaration));
}

// Fills IN `declaration`'s own value shape from `contract` - the ONE
// place that dispatches raw/color/values, so build_known_property()
// below stays the orchestrator D-DP-8's own plan describes it as,
// never the place that decides any of the three shapes itself.
[[nodiscard]] std::optional<gltfx_gfss_diagnostic>
apply_contract_value(const std::vector<gltfx_gfss_token> &tokens,
                     const trimmed_token_span &value_span, const property_value_contract &contract,
                     gfss_declaration &declaration) {
    if (contract.raw_composite) {
        declaration.form = gfss_declaration_value_form::raw;
        for (std::size_t i = value_span.first; i <= value_span.last; ++i) {
            declaration.raw_tokens.push_back(tokens[i]);
        }
        return std::nullopt;
    }

    if (contract.is_color) {
        const declaration_color_value_result color =
            read_declaration_color_value(tokens, value_span.first, value_span.last + 1);
        if (color.kind == declaration_color_value_kind::failed) {
            return color.diagnostic;
        }
        if (color.kind == declaration_color_value_kind::current_color) {
            declaration.form = gfss_declaration_value_form::current_color;
        } else {
            declaration.form = gfss_declaration_value_form::color;
            declaration.color = color.value;
        }
        return std::nullopt;
    }

    const declaration_value_check_result checked =
        check_declaration_value(tokens, value_span.first, value_span.last + 1, contract);
    if (!checked.ok) {
        return checked.diagnostic;
    }
    declaration.form = gfss_declaration_value_form::values;
    declaration.values = checked.values;
    return std::nullopt;
}

[[nodiscard]] declaration_parse_result
build_known_property(const std::vector<gltfx_gfss_token> &tokens, gltfx_gfss_property property,
                     bool important, const trimmed_token_span &value_span) {
    gfss_declaration declaration;
    declaration.is_shorthand = false;
    declaration.property = property;
    declaration.important = important;

    const universal_scan_outcome universal =
        scan_for_universal_keyword(tokens, value_span.first, value_span.last);
    if (universal.found_but_mixed) {
        const gltfx_gfss_token &at = tokens[universal.mixed_position];
        return rejected(gltfx_gfss_diagnostic{.line = at.line,
                                              .column = at.column,
                                              .expected = k_expected_universal_keyword_alone,
                                              .detail = {}});
    }

    if (universal.is_universal_alone) {
        declaration.form = gfss_declaration_value_form::universal_keyword;
        declaration.universal = universal.keyword;
    } else if (const std::optional<gltfx_gfss_diagnostic> failure = apply_contract_value(
                   tokens, value_span, property_value_contract_for(property), declaration)) {
        return rejected(*failure);
    }

    declaration.has_reserved_notice = gfss_property_status(property) == property_status::reserved;
    return accepted(std::move(declaration));
}

} // namespace

declaration_parse_result parse_declaration(const std::vector<gltfx_gfss_token> &tokens,
                                           std::size_t begin, std::size_t end) {
    const name_and_colon_outcome name_and_colon = read_name_and_colon(tokens, begin, end);
    if (!name_and_colon.ok) {
        return rejected(name_and_colon.diagnostic);
    }

    const important_flag_result flag = read_important_flag(tokens, name_and_colon.value_begin, end);
    if (!flag.ok) {
        return rejected(flag.diagnostic);
    }

    const trimmed_token_span value_span =
        trim_whitespace_tokens(tokens, name_and_colon.value_begin, flag.value_end);
    if (!value_span.has_content) {
        const std::size_t at =
            name_and_colon.value_begin < end ? name_and_colon.value_begin : begin;
        return rejected(gltfx_gfss_diagnostic{.line = tokens[at].line,
                                              .column = tokens[at].column,
                                              .expected = k_expected_component_value,
                                              .detail = {}});
    }

    if (name_and_colon.lookup.kind == property_name_lookup_kind::shorthand) {
        return build_shorthand(tokens, name_and_colon.lookup.shorthand_name, flag.important,
                               value_span);
    }
    return build_known_property(tokens, name_and_colon.lookup.property, flag.important, value_span);
}

} // namespace glintfx::style::detail
