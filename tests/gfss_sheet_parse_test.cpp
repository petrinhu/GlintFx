// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <print>
#include <string>
#include <string_view>

#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/sheet_ast.hpp"
#include "gfss/sheet_parse.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_sheet_parse_test.cpp - GFSS-SHEET-PARSE (TODO.md, GODS_LAWS.md
// L-20/L-27/L-40): the TDD witness for
// glintfx::style::detail::parse_sheet() (sheet_parse.hpp) - see that
// file's own header comment for the recovery policy and at-rule dispatch
// each case below proves.
//
// Prova exigida pela linha do item (TODO.md, GFSS-SHEET-PARSE, L-40):
// os TRES comportamentos - `@media` (diagnostico de fora-da-v1), bloco
// de animacao (`@keyframes`, guardado cru, SEM diagnostico, conteudo
// integro conferido byte a byte) e at-rule desconhecida (diagnostico com
// "o que se esperava") - ENUMERADOS numa unica matriz fechada abaixo
// (gltfx_gfss_parse_sheet_dispatches_the_three_at_rule_behaviors),
// contagem impressa, zero reprova.
using glintfx::style::detail::gfss_sheet_parse_result;
using glintfx::style::detail::k_expected_at_rule_media_is_outside_v1;
using glintfx::style::detail::k_expected_closing_curly_brace;
using glintfx::style::detail::k_expected_keyframes_name;
using glintfx::style::detail::k_expected_opening_curly_brace_after_at_rule;
using glintfx::style::detail::k_expected_opening_curly_brace_after_selector;
using glintfx::style::detail::k_expected_style_rule_or_at_rule;
using glintfx::style::detail::k_expected_supported_at_rule;
using glintfx::style::detail::parse_sheet;

// FIRST ASSERTION (this fatia's own service order, captured BEFORE
// sheet_parse.cpp existed - GODS_LAWS.md L-20 "veja o teste falhar"): a
// single, well-formed rule is read, its selector recognized, and its
// declaration read.
GLINTFX_TEST(gltfx_gfss_parse_sheet_reads_one_well_formed_rule) {
    const gfss_sheet_parse_result result = parse_sheet("button { color: red; }");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.ignored_at_rules.size(), static_cast<std::size_t>(0));
    if (!result.rules.empty()) {
        const auto &rule = result.rules.front();
        GLINTFX_CHECK_EQ(rule.selectors.selectors.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK_EQ(rule.declarations.declarations.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK_EQ(rule.source_order, static_cast<std::size_t>(0));
    }
    GLINTFX_CHECK_EQ(result.swept_count, static_cast<std::size_t>(1));
    std::println("gltfx_gfss_parse_sheet_reads_one_well_formed_rule: swept={}", result.swept_count);
}

// Multiple rules keep SOURCE ORDER 0..N-1 without a gap, even when one
// in between is rejected (a rejected rule never consumes an order
// index - decided in sheet_parse.cpp's own handle_style_rule(), proven
// here rather than merely asserted).
GLINTFX_TEST(gltfx_gfss_parse_sheet_recovers_from_a_bad_selector_and_keeps_source_order) {
    const gfss_sheet_parse_result result = parse_sheet(".ok-1 { color: red; } "
                                                       ":bogus-pseudo { color: blue; } "
                                                       ".ok-2 { color: green; }");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(2));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (result.rules.size() == 2) {
        GLINTFX_CHECK_EQ(result.rules[0].source_order, static_cast<std::size_t>(0));
        GLINTFX_CHECK_EQ(result.rules[1].source_order, static_cast<std::size_t>(1));
    }
    GLINTFX_CHECK_EQ(result.swept_count, static_cast<std::size_t>(3));
    std::println("gltfx_gfss_parse_sheet_recovers_from_a_bad_selector_and_keeps_source_order: "
                 "rules={} rejected={} swept={}",
                 result.rules.size(), result.rejected.size(), result.swept_count);
}

// A malformed DECLARATION inside an otherwise-good rule never rejects
// the whole rule - it lands in that rule's OWN declarations.rejected
// (declaration_list_parse.hpp's own per-declaration recovery,
// GFSS-DECL-PARSE), never in the sheet's own top-level `rejected`.
GLINTFX_TEST(gltfx_gfss_parse_sheet_keeps_a_rule_whose_one_declaration_is_bad) {
    const gfss_sheet_parse_result result = parse_sheet(".card { color: red; bogus-property: 1; }");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
    if (!result.rules.empty()) {
        GLINTFX_CHECK_EQ(result.rules.front().declarations.declarations.size(),
                         static_cast<std::size_t>(1));
        GLINTFX_CHECK(!result.rules.front().declarations.rejected.empty());
    }
    std::println("gltfx_gfss_parse_sheet_keeps_a_rule_whose_one_declaration_is_bad: block-level "
                 "recovery ok");
}

// THE MATRIX (TODO.md's own "Prova (L-40)" line, verbatim): the three
// at-rule behaviors, enumerated together so no fourth or fifth case can
// sneak into ANY of the three without a row here proving it.
GLINTFX_TEST(gltfx_gfss_parse_sheet_dispatches_the_three_at_rule_behaviors) {
    // 1. @media - recognized, ignored, WITH a diagnostic naming it
    // out-of-v1 (ESCOPO.md's own decision 4 of 21/08/2026, still
    // standing) - never treated as an error, never silently dropped.
    {
        const gfss_sheet_parse_result result =
            parse_sheet("@media screen and (min-width: 100px) { .a { color: red; } } "
                        ".after { color: blue; }");
        GLINTFX_CHECK_EQ(result.ignored_at_rules.size(), static_cast<std::size_t>(1));
        if (!result.ignored_at_rules.empty()) {
            GLINTFX_CHECK(result.ignored_at_rules.front().expected ==
                          k_expected_at_rule_media_is_outside_v1);
        }
        GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
        // The whole @media BLOCK is skipped whole - the nested rule
        // inside it (".a") is never read as a sheet-level rule; only
        // the rule AFTER the @media survives.
        GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    }

    // 2. @keyframes - recognized, guarded CRU, NO diagnostic, content
    // proven BYTE FOR BYTE (memcmp against the sheet's own source
    // slice, never a copy compared by value).
    {
        static constexpr std::string_view k_body =
            "\n  0% { opacity: 0; }\n  100% { opacity: 1; }\n";
        const std::string source = std::string("@keyframes fade-in {") + std::string(k_body) +
                                   "} .after { color: green; }";
        const gfss_sheet_parse_result result = parse_sheet(source);
        GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK_EQ(result.ignored_at_rules.size(), static_cast<std::size_t>(0));
        GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
        if (!result.raw_blocks.empty()) {
            const auto &block = result.raw_blocks.front();
            GLINTFX_CHECK(block.name == std::string_view{"fade-in"});
            GLINTFX_CHECK_EQ(block.raw_body.size(), k_body.size());
            GLINTFX_CHECK(block.raw_body.size() == k_body.size() &&
                          std::memcmp(block.raw_body.data(), k_body.data(), k_body.size()) == 0);
        }
        GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    }

    // 3. Unknown at-rule - recognized as an at-keyword, but NOT in
    // at_rule_vocabulary.hpp's own closed list - ignored, WITH a
    // diagnostic naming what IS supported ("o que se esperava"). Proven
    // both WITH a block and WITHOUT one (community dossier's own
    // "@import url(x);" case).
    {
        const gfss_sheet_parse_result with_block =
            parse_sheet("@supports (display: grid) { .a { color: red; } } .after { color: blue; }");
        GLINTFX_CHECK_EQ(with_block.ignored_at_rules.size(), static_cast<std::size_t>(1));
        if (!with_block.ignored_at_rules.empty()) {
            const auto &diagnostic = with_block.ignored_at_rules.front();
            GLINTFX_CHECK(diagnostic.expected == k_expected_supported_at_rule);
            GLINTFX_CHECK(!diagnostic.detail.empty());
        }
        GLINTFX_CHECK_EQ(with_block.rules.size(), static_cast<std::size_t>(1));

        const gfss_sheet_parse_result without_block =
            parse_sheet("@import url(theme.gfss); .after { color: blue; }");
        GLINTFX_CHECK_EQ(without_block.ignored_at_rules.size(), static_cast<std::size_t>(1));
        if (!without_block.ignored_at_rules.empty()) {
            GLINTFX_CHECK(without_block.ignored_at_rules.front().expected ==
                          k_expected_supported_at_rule);
        }
        GLINTFX_CHECK_EQ(without_block.rules.size(), static_cast<std::size_t>(1));
    }

    std::println("gltfx_gfss_parse_sheet_dispatches_the_three_at_rule_behaviors: 3 at-rule "
                 "behavior(s) checked (media, keyframes raw, unknown)");
    std::println("SCANCOUNT gfss_sheet_parse_test.at_rule_behaviors_checked=3");
}

// A `@keyframes` missing its own NAME is rejected, not stored as a raw
// block with an empty name a later reader (ANIM-KEYFRAMES) could
// mistake for something real.
GLINTFX_TEST(gltfx_gfss_parse_sheet_rejects_keyframes_without_a_name) {
    const gfss_sheet_parse_result result = parse_sheet("@keyframes { 0% { a: 1px } } .after {}");
    GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected == k_expected_keyframes_name);
    }
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
}

// A `@keyframes` with NO block at all (terminated by ';' instead of
// '{') is rejected - the SAME family this file's own service order
// requires ("@keyframes sem bloco").
GLINTFX_TEST(gltfx_gfss_parse_sheet_rejects_keyframes_without_a_block) {
    const gfss_sheet_parse_result result =
        parse_sheet("@keyframes fade-in; .after { color: red; }");
    GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected ==
                      k_expected_opening_curly_brace_after_at_rule);
    }
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
}

// A stray '}' at the top level recovers - a RELIABLE resync point (the
// next token is exactly where a rule could start), never fatal.
GLINTFX_TEST(gltfx_gfss_parse_sheet_recovers_from_a_stray_closing_brace) {
    const gfss_sheet_parse_result result = parse_sheet("} .after { color: red; }");
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected == k_expected_style_rule_or_at_rule);
    }
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
}

// THE ONE FATAL CONDITION (sheet_parse.cpp's own header comment, GODS_
// LAWS.md L-40's own "no more sheet to trust"): a `{` that never finds
// its own matching `}` stops the WHOLE sheet - the rule after it is
// NEVER read, the community dossier's own Firefox failure mode (item
// 4.5) proven here on purpose, refused rather than silently swallowed.
GLINTFX_TEST(gltfx_gfss_parse_sheet_stops_after_an_unclosed_brace_and_reads_nothing_past_it) {
    const gfss_sheet_parse_result result = parse_sheet(".broken { color: red; "
                                                       ".after { color: blue; }");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected == k_expected_closing_curly_brace);
    }
    std::println("gltfx_gfss_parse_sheet_stops_after_an_unclosed_brace_and_reads_nothing_past_it: "
                 "rules={} (must be 0 - nothing after an unclosed brace is ever read)",
                 result.rules.size());
}

// An unclosed brace INSIDE a `@keyframes` block is the SAME fatal
// condition, one level up (dossier item 4.5's own generalization: the
// community pain is real for ANY unclosed brace, not only a style
// rule's own).
GLINTFX_TEST(gltfx_gfss_parse_sheet_stops_after_an_unclosed_keyframes_brace) {
    const gfss_sheet_parse_result result =
        parse_sheet("@keyframes broken { 0% { a: 1px } .after { color: blue; }");
    GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected == k_expected_closing_curly_brace);
    }
}

// A `{` inside a QUOTED STRING (an attribute selector's own value)
// never counts as a real brace - community dossier item 4.6, proven
// here rather than merely asserted: the rule still reads correctly,
// and its own declaration block is exactly the miolo after the REAL
// closing brace, never truncated early at the fake one inside the
// string.
GLINTFX_TEST(gltfx_gfss_parse_sheet_brace_inside_a_quoted_string_is_not_a_real_brace) {
    const gfss_sheet_parse_result result = parse_sheet(R"([data-token="{"] { color: red; })");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
    if (!result.rules.empty()) {
        GLINTFX_CHECK_EQ(result.rules.front().declarations.declarations.size(),
                         static_cast<std::size_t>(1));
    }
}

// Trailing garbage that never reaches a '{' at all (no rule, no
// at-rule, just leftover text at EOF) is the OTHER "no more sheet to
// trust" fatal condition, one level below an unclosed brace: there was
// never even an OPEN brace to look for the close of.
GLINTFX_TEST(gltfx_gfss_parse_sheet_stops_when_a_selector_prelude_never_reaches_an_open_brace) {
    const gfss_sheet_parse_result result = parse_sheet(".ok { color: red; } trailing garbage");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(1));
    if (!result.rejected.empty()) {
        GLINTFX_CHECK(result.rejected.front().expected ==
                      k_expected_opening_curly_brace_after_selector);
    }
}

// An EMPTY sheet is a valid, zero-rule result - not a floor violation
// of THIS parser's own count (GODS_LAWS.md L-40's own "zero varridos
// reprova" governs the TEST's own enumeration of SCENARIOS, proven by
// this whole file having more than zero test cases - it does not mean
// every individual sheet under test must itself be non-empty).
GLINTFX_TEST(gltfx_gfss_parse_sheet_of_empty_text_is_zero_rules_not_a_failure) {
    const gfss_sheet_parse_result result = parse_sheet("");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.raw_blocks.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.rejected.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.ignored_at_rules.size(), static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(result.swept_count, static_cast<std::size_t>(0));
}

// A comma-separated selector list is ONE rule with several selectors,
// never several rules - selector_parse.hpp's own gfss_selector_list is
// what the whole prelude parses into, unchanged by this layer.
GLINTFX_TEST(gltfx_gfss_parse_sheet_reads_a_comma_separated_selector_list_as_one_rule) {
    const gfss_sheet_parse_result result = parse_sheet(".a, .b, .c { color: red; }");
    GLINTFX_CHECK_EQ(result.rules.size(), static_cast<std::size_t>(1));
    if (!result.rules.empty()) {
        GLINTFX_CHECK_EQ(result.rules.front().selectors.selectors.size(),
                         static_cast<std::size_t>(3));
    }
}
