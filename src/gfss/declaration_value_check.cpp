// SPDX-License-Identifier: AGPL-3.0-or-later
#include "declaration_value_check.hpp"

#include <algorithm>
#include <string>

#include "ascii_case.hpp"
#include "diagnostic_vocabulary.hpp"
#include "nesting_depth.hpp"
#include "token_span_trim.hpp"
#include "value_parse.hpp"

// declaration_value_check.cpp - GFSS-DECL-PARSE, DP-6 (GODS_LAWS.md
// L-17: each function below answers exactly one question of
// declaration_value_check.hpp's own header comment scope).
// trim_whitespace_tokens() itself now lives in token_span_trim.hpp -
// see that file's own header comment for why (CONTRACT.md SS6.7's
// "regra de tres").

namespace glintfx::style::detail {

namespace {

// Every declaration_ diagnostic constructor this file needs shares the
// same three fields - one atom instead of repeating the aggregate
// literal at every failing return.
[[nodiscard]] declaration_value_check_result
fail(const gltfx_gfss_token &at, std::string_view expected, std::string_view detail = {}) noexcept {
    return declaration_value_check_result{
        .ok = false,
        .values = {},
        .diagnostic = gltfx_gfss_diagnostic{
            .line = at.line, .column = at.column, .expected = expected, .detail = detail}};
}

// Space-separated identifiers naming every keyword EVERY contract in
// the registry accepts (R7: identifiers, never a sentence) - one entry
// per property.hpp's own id, built ONCE at static-initialization time,
// in PROGRAM-LIFETIME storage.
//
// NOT a std::string returned BY VALUE from a per-call helper - that was
// this file's own first shape, and cppcheck's own returnDanglingLifetime
// caught it live (tools/preci.sh's own lint stage, run locally before
// this fatia's own closing commit):
// `fail(first, ..., accepted_keyword_detail(contract))` bound the
// temporary std::string's buffer to `gltfx_gfss_diagnostic::detail` (a
// non-owning std::string_view, token.hpp's own R4/R5 shape), then the
// temporary was destroyed at the end of THAT full expression - the
// returned diagnostic's own `.detail` view dangled. GLINTFX_CHECK(...
// detail == "block flex none") in this file's own test still PASSED
// (the freed bytes had not been overwritten yet) - undefined behavior
// that happened to read back correct text, exactly the "verde que nao
// prova nada" GODS_LAWS.md L-36 warns against; a static analyzer, not
// this fatia's own runtime suite, is what actually caught it. A cache
// keyed by std::string_view (a per-check-call temporary too) would
// have the SAME defect one layer up - only PERMANENT storage, built
// once, fixes it structurally.
[[nodiscard]] const std::array<std::string, gltfx_gfss_property_count> &
accepted_keyword_detail_table() {
    static const std::array<std::string, gltfx_gfss_property_count> table = [] {
        std::array<std::string, gltfx_gfss_property_count> built{};
        for (std::size_t i = 0; i < k_property_value_contracts.size(); ++i) {
            const property_value_contract &c = k_property_value_contracts[i];
            std::string detail;
            for (std::uint8_t k = 0; k < c.keyword_count; ++k) {
                if (k > 0) {
                    detail += ' ';
                }
                detail += gltfx_gfss_keyword_name(c.keywords[k]);
            }
            built[i] = std::move(detail);
        }
        return built;
    }();
    return table;
}

// `contract` is always one of k_property_value_contracts' own rows,
// reached through property_value_contract_for() (declaration_parse.cpp
// never builds an ad hoc contract) - contract.id is therefore always
// that row's own real index, safe to use here directly.
[[nodiscard]] std::string_view accepted_keyword_detail(const property_value_contract &contract) {
    return accepted_keyword_detail_table()[static_cast<std::size_t>(contract.id)];
}

[[nodiscard]] bool matches_accepted_keyword(const property_value_contract &contract,
                                            std::string_view keyword_text) noexcept {
    for (std::uint8_t i = 0; i < contract.keyword_count; ++i) {
        if (ascii_case_insensitive_equal(gltfx_gfss_keyword_name(contract.keywords[i]),
                                         keyword_text)) {
            return true;
        }
    }
    return false;
}

// The k_nature_* bit for a DECODED value's own kind - an explicit
// mapping, never `1 << static_cast<uint8_t>(kind)`: value.hpp's own
// gltfx_gfss_value_kind enumerator order (keyword, number, integer,
// length, percentage, angle, time) does NOT match this file's own
// property_value_contract.hpp bit order (length, percentage, number,
// integer, angle, time) - the two lists exist for different readers
// (value.hpp's own spec-citation order vs. this contract's own "cheap
// natures first" order), and deriving one from the other's ordinal
// would silently break the moment either list is reordered. `keyword`
// has no bit of its own (a decoded keyword is checked against
// `keywords`, never `accepted_natures`) - callers never reach this
// function with that kind.
[[nodiscard]] std::uint8_t nature_bit_for(gltfx_gfss_value_kind kind) noexcept {
    switch (kind) {
    case gltfx_gfss_value_kind::length:
        return k_nature_length;
    case gltfx_gfss_value_kind::percentage:
        return k_nature_percentage;
    case gltfx_gfss_value_kind::number:
        return k_nature_number;
    case gltfx_gfss_value_kind::integer:
        return k_nature_integer;
    case gltfx_gfss_value_kind::angle:
        return k_nature_angle;
    case gltfx_gfss_value_kind::time:
        return k_nature_time;
    case gltfx_gfss_value_kind::keyword:
        return 0;
    }
    return 0;
}

// --- pure keyword contract (no numeric nature at all) -----------------

declaration_value_check_result check_keyword_only(const std::vector<gltfx_gfss_token> &tokens,
                                                  const trimmed_token_span &span,
                                                  const property_value_contract &contract) {
    const gltfx_gfss_token &first = tokens[span.first];
    if (first.kind != gltfx_gfss_token_kind::ident ||
        !matches_accepted_keyword(contract, first.lexeme)) {
        return fail(first, k_expected_keyword_for_property, accepted_keyword_detail(contract));
    }

    std::size_t next = span.first + 1;
    while (next <= span.last && tokens[next].kind == gltfx_gfss_token_kind::whitespace) {
        ++next;
    }
    if (next <= span.last) {
        return fail(tokens[next], k_expected_semicolon_or_end_of_declaration);
    }

    declaration_value_check_result result;
    result.ok = true;
    result.values.push_back(
        gltfx_gfss_value{.kind = gltfx_gfss_value_kind::keyword, .keyword_text = first.lexeme});
    return result;
}

// --- one component, shared by the space-separated and comma-separated
// walkers below --------------------------------------------------------

// Decodes ONE token as a component of `contract`'s own value - the
// keyword/nature/range checks D-DP-4's own contract exists to make, all
// in one place so neither walker below repeats them.
struct component_outcome {
    bool ok = false;
    gltfx_gfss_value value{};
    gltfx_gfss_diagnostic diagnostic{};
};

[[nodiscard]] component_outcome
check_one_component(const gltfx_gfss_token &token,
                    const property_value_contract &contract) noexcept {
    value_parse_result parsed = parse_value(token);
    if (!parsed.ok) {
        return component_outcome{.ok = false, .diagnostic = parsed.diagnostic};
    }

    if (parsed.value.kind == gltfx_gfss_value_kind::keyword) {
        if (!matches_accepted_keyword(contract, parsed.value.keyword_text)) {
            return component_outcome{
                .ok = false,
                .diagnostic = gltfx_gfss_diagnostic{.line = token.line,
                                                    .column = token.column,
                                                    .expected = k_expected_keyword_for_property,
                                                    .detail = accepted_keyword_detail(contract)}};
        }
        return component_outcome{.ok = true, .value = parsed.value};
    }

    // A bare, unitless number folds to milliseconds on a time-typed
    // contract (project leader's 28/08/2026 decision, docs/gfss-
    // property-registry-v1.md SS5 item 3) - tried BEFORE the ordinary
    // nature-bit check, since a bare number's own decoded kind
    // (::number/::integer) would otherwise fail a contract whose
    // accepted_natures is time-only.
    if (contract.time_unitless_ms && (parsed.value.kind == gltfx_gfss_value_kind::number ||
                                      parsed.value.kind == gltfx_gfss_value_kind::integer)) {
        const double magnitude = (parsed.value.kind == gltfx_gfss_value_kind::number)
                                     ? parsed.value.number
                                     : static_cast<double>(parsed.value.integer_value);
        gltfx_gfss_value folded{};
        folded.kind = gltfx_gfss_value_kind::time;
        folded.duration = gltfx_gfss_time{.magnitude = magnitude, .unit = gltfx_gfss_time_unit::ms};
        return component_outcome{.ok = true, .value = folded};
    }

    // An integer-SPELLED literal ("1", "-1") still satisfies a contract
    // that only declares the `number` nature - CSS Syntax Module Level
    // 3's own 4.3.12 type flag (value_parse.cpp's own number_lexeme_is_
    // integer()) marks "1" as an <integer-token> lexically, but a
    // property whose own grammar cell says simply "numero" (opacity,
    // gltfx-Velocity, flex-grow/flex-shrink) has never meant to REFUSE
    // the common, decimal-point-free spelling of that number - only a
    // property whose OWN cell says "inteiro" (z-index, order,
    // gltfx-Chaos_Seed) requires the type flag itself, and those
    // contracts declare `k_nature_integer`, never `k_nature_number`
    // alone, so this fold never masks THAT distinction (SS5 item 5).
    if (parsed.value.kind == gltfx_gfss_value_kind::integer &&
        (contract.accepted_natures & k_nature_integer) == 0 &&
        (contract.accepted_natures & k_nature_number) != 0) {
        gltfx_gfss_value as_number{};
        as_number.kind = gltfx_gfss_value_kind::number;
        as_number.number = static_cast<double>(parsed.value.integer_value);
        parsed.value = as_number;
    }

    const std::uint8_t nature_bit = nature_bit_for(parsed.value.kind);
    if ((contract.accepted_natures & nature_bit) == 0) {
        return component_outcome{
            .ok = false,
            .diagnostic = gltfx_gfss_diagnostic{.line = token.line,
                                                .column = token.column,
                                                .expected = k_expected_value_nature_for_property,
                                                .detail = {}}};
    }

    if (!contract.has_range) {
        return component_outcome{.ok = true, .value = parsed.value};
    }

    const bool is_integer = parsed.value.kind == gltfx_gfss_value_kind::integer;
    const double magnitude =
        is_integer ? static_cast<double>(parsed.value.integer_value) : parsed.value.number;
    if (magnitude >= contract.range_min && magnitude <= contract.range_max) {
        return component_outcome{.ok = true, .value = parsed.value};
    }
    if (!contract.clamp_range) {
        return component_outcome{
            .ok = false,
            .diagnostic = gltfx_gfss_diagnostic{.line = token.line,
                                                .column = token.column,
                                                .expected = k_expected_value_in_range_for_property,
                                                .detail = {}}};
    }
    const double clamped = std::clamp(magnitude, contract.range_min, contract.range_max);
    gltfx_gfss_value value = parsed.value;
    if (is_integer) {
        value.integer_value = static_cast<long long>(clamped);
    } else {
        value.number = clamped;
    }
    return component_outcome{.ok = true, .value = value};
}

// --- space-separated arity (the ordinary, non-comma case) -------------

declaration_value_check_result check_space_separated(const std::vector<gltfx_gfss_token> &tokens,
                                                     const trimmed_token_span &span,
                                                     const property_value_contract &contract) {
    declaration_value_check_result result;
    for (std::size_t i = span.first; i <= span.last; ++i) {
        if (tokens[i].kind == gltfx_gfss_token_kind::whitespace) {
            continue;
        }
        if (result.values.size() == contract.max_count) {
            return fail(tokens[i], k_expected_value_count_for_property);
        }
        const component_outcome outcome = check_one_component(tokens[i], contract);
        if (!outcome.ok) {
            return declaration_value_check_result{
                .ok = false, .values = {}, .diagnostic = outcome.diagnostic};
        }
        result.values.push_back(outcome.value);
    }
    if (result.values.size() < contract.min_count) {
        return fail(tokens[span.last], k_expected_value_count_for_property);
    }
    result.ok = true;
    return result;
}

// --- top-level comma-separated groups (the `<time>#` shape) -----------

declaration_value_check_result check_comma_separated(const std::vector<gltfx_gfss_token> &tokens,
                                                     const trimmed_token_span &span,
                                                     const property_value_contract &contract) {
    declaration_value_check_result result;
    std::size_t group_start = span.first;
    int depth = 0;

    auto close_group = [&](std::size_t group_end) -> const gltfx_gfss_diagnostic * {
        static gltfx_gfss_diagnostic diagnostic{};
        const trimmed_token_span group = trim_whitespace_tokens(tokens, group_start, group_end);
        if (!group.has_content || group.first != group.last) {
            diagnostic = gltfx_gfss_diagnostic{.line = tokens[group_start].line,
                                               .column = tokens[group_start].column,
                                               .expected = k_expected_value_count_for_property,
                                               .detail = {}};
            return &diagnostic;
        }
        const component_outcome outcome = check_one_component(tokens[group.first], contract);
        if (!outcome.ok) {
            diagnostic = outcome.diagnostic;
            return &diagnostic;
        }
        result.values.push_back(outcome.value);
        return nullptr;
    };

    for (std::size_t i = span.first; i <= span.last; ++i) {
        if (tokens[i].kind == gltfx_gfss_token_kind::comma && depth == 0) {
            if (const gltfx_gfss_diagnostic *diagnostic = close_group(i)) {
                return declaration_value_check_result{
                    .ok = false, .values = {}, .diagnostic = *diagnostic};
            }
            group_start = i + 1;
            continue;
        }
        // Same clamp as declaration_split.cpp's own loop - a stray
        // closing token must never drive depth negative.
        depth += nesting_depth_delta(tokens[i].kind);
        if (depth < 0) {
            depth = 0;
        }
    }
    if (const gltfx_gfss_diagnostic *diagnostic = close_group(span.last + 1)) {
        return declaration_value_check_result{.ok = false, .values = {}, .diagnostic = *diagnostic};
    }

    if (result.values.size() < contract.min_count || result.values.size() > contract.max_count) {
        return fail(tokens[span.last], k_expected_value_count_for_property);
    }
    result.ok = true;
    return result;
}

} // namespace

declaration_value_check_result check_declaration_value(const std::vector<gltfx_gfss_token> &tokens,
                                                       std::size_t begin, std::size_t end,
                                                       const property_value_contract &contract) {
    const trimmed_token_span span = trim_whitespace_tokens(tokens, begin, end);
    if (!span.has_content) {
        return declaration_value_check_result{
            .ok = false,
            .values = {},
            .diagnostic = gltfx_gfss_diagnostic{.expected = k_expected_component_value}};
    }

    if (contract.keyword_count > 0 && contract.accepted_natures == 0) {
        return check_keyword_only(tokens, span, contract);
    }
    if (contract.comma_separated) {
        return check_comma_separated(tokens, span, contract);
    }
    return check_space_separated(tokens, span, contract);
}

} // namespace glintfx::style::detail
