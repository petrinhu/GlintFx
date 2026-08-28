// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <iterator>
#include <print>
#include <string>
#include <string_view>

#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfss/selector_pseudo_vocabulary.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_selector_parse_test.cpp - GFSS-SEL-PARSE-CORE (TODO.md,
// GODS_LAWS.md L-20/L-27/L-28/L-40): the TDD red/green witness for
// glintfx::style::detail::parse_selector_list() (selector_parse.hpp) -
// see that file's own header comment for the design rationale each
// check below proves.
//
// ONE SHARED DIAGNOSTIC VOCABULARY, NOT A THIRD ONE (project leader's
// decision, 27/08/2026, GODS_LAWS.md L-27 - reached this fatia through
// the orchestrator mid-implementation, correcting an earlier draft of
// this same file that DID open a separate selector_diagnostic_
// vocabulary.hpp): every diagnostic this parser produces
// (simple_selector, identifier_after_dot, identifier_after_colon,
// known_pseudo_class, known_pseudo_function, comma_or_end_of_selector_
// list) lives in diagnostic_vocabulary.hpp - the SAME list tokenizer.
// cpp already uses, whose own k_expected_closing_parenthesis this file
// REUSES rather than re-spelling (see this file's own duplicate-word
// proof below for why that reuse is checked, not assumed).
using glintfx::style::detail::gfss_combinator;
using glintfx::style::detail::gfss_combinator_count;
using glintfx::style::detail::gfss_combinator_table;
using glintfx::style::detail::gfss_simple_selector_kind;
using glintfx::style::detail::k_expected_closing_parenthesis;
using glintfx::style::detail::k_expected_comma_or_end_of_selector_list;
using glintfx::style::detail::k_expected_identifier_after_colon;
using glintfx::style::detail::k_expected_identifier_after_dot;
using glintfx::style::detail::k_expected_known_pseudo_class;
using glintfx::style::detail::k_expected_known_pseudo_function;
using glintfx::style::detail::k_expected_simple_selector;
using glintfx::style::detail::k_expected_vocabulary;
using glintfx::style::detail::k_expected_vocabulary_count;
using glintfx::style::detail::k_functional_pseudo_count;
using glintfx::style::detail::k_simple_pseudo_count;
using glintfx::style::detail::k_simple_pseudo_names;
using glintfx::style::detail::parse_selector_list;

// FIRST ASSERTION (this fatia's own service order, captured BEFORE
// selector_parse.cpp existed - GODS_LAWS.md L-20 "veja o teste
// falhar"): "button.primary #ok" is a compound selector
// (button.primary) followed by a DESCENDANT combinator and a second
// compound selector (#ok).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_compound_then_descendant_combinator) {
    const auto result = parse_selector_list("button.primary #ok");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    const auto &complex_selector = result.value.selectors.front();

    GLINTFX_CHECK_EQ(complex_selector.head.simple_selectors.size(), static_cast<std::size_t>(2));
    GLINTFX_CHECK(complex_selector.head.simple_selectors[0].kind ==
                  gfss_simple_selector_kind::type);
    GLINTFX_CHECK(complex_selector.head.simple_selectors[0].name == std::string_view{"button"});
    GLINTFX_CHECK(complex_selector.head.simple_selectors[1].kind ==
                  gfss_simple_selector_kind::class_selector);
    GLINTFX_CHECK(complex_selector.head.simple_selectors[1].name == std::string_view{"primary"});

    GLINTFX_CHECK_EQ(complex_selector.rest.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(complex_selector.rest[0].combinator == gfss_combinator::descendant);
    GLINTFX_CHECK_EQ(complex_selector.rest[0].compound.simple_selectors.size(),
                     static_cast<std::size_t>(1));
    GLINTFX_CHECK(complex_selector.rest[0].compound.simple_selectors[0].kind ==
                  gfss_simple_selector_kind::id_selector);
    GLINTFX_CHECK(complex_selector.rest[0].compound.simple_selectors[0].name ==
                  std::string_view{"ok"});
}

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado"): every one of the 14
// argument-less pseudo-classes this fatia's own service order names,
// swept from selector_pseudo_vocabulary.hpp's own k_simple_pseudo_names
// - never a hand-copied second list that could drift from it.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_simple_pseudo_class) {
    static_assert(
        k_simple_pseudo_count == 14,
        "GODS_LAWS.md L-40: selector_pseudo_vocabulary.hpp's simple-pseudo list changed - "
        "this fatia's own service order names exactly 14, update it or this count");

    std::size_t swept = 0;
    for (const std::string_view name : k_simple_pseudo_names) {
        const std::string text = ":" + std::string(name);
        const auto result = parse_selector_list(text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
            const auto &simples = result.value.selectors.front().head.simple_selectors;
            GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
            if (simples.size() == 1) {
                GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::pseudo_class);
                GLINTFX_CHECK(simples[0].name == name);
            }
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_simple_pseudo_count);
    std::println(
        "gltfx_gfss_parse_selector_list_recognizes_every_simple_pseudo_class: {} pseudo-class(es) "
        "checked",
        swept);
}

// ENUMERATION: the four combinators selector_ast.hpp's own
// gfss_combinator_table names, swept directly - a "5th combinator"
// added to that table with no matching case here fails to compile
// (the static_assert below), never passes silently with a stale count.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_combinator) {
    static_assert(gfss_combinator_count == 4,
                  "GODS_LAWS.md L-40: selector_ast.hpp's combinator table changed - update this "
                  "sweep to match");

    std::size_t swept = 0;
    for (const auto &entry : gfss_combinator_table) {
        const std::string text = std::string("a") + entry.delimiter + "b";
        const auto result = parse_selector_list(text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
            const auto &complex_selector = result.value.selectors.front();
            GLINTFX_CHECK_EQ(complex_selector.rest.size(), static_cast<std::size_t>(1));
            if (complex_selector.rest.size() == 1) {
                GLINTFX_CHECK(complex_selector.rest[0].combinator == entry.combinator);
            }
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gfss_combinator_count);
    std::println("gltfx_gfss_parse_selector_list_recognizes_every_combinator: {} combinator(s) "
                 "checked",
                 swept);
}

// The four SIMPLE shapes (tag/.class/#id/*) - a CLOSED, grammar-fixed
// set (selector_ast.hpp's own header comment explains why this one is
// NOT X-macro'd, unlike the two pseudo-class lists above: there is no
// "a 5th shape was added and the count forgot to follow" risk class
// here, the shapes are fixed by the parser's own dispatch structure,
// not an open catalog of names).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_simple_selector_shape) {
    const auto tag = parse_selector_list("button");
    GLINTFX_CHECK(tag.ok);
    if (tag.ok) {
        const auto &simples = tag.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::type);
        GLINTFX_CHECK(simples[0].name == std::string_view{"button"});
    }

    const auto class_selector = parse_selector_list(".primary");
    GLINTFX_CHECK(class_selector.ok);
    if (class_selector.ok) {
        const auto &simples = class_selector.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::class_selector);
        GLINTFX_CHECK(simples[0].name == std::string_view{"primary"});
    }

    const auto id_selector = parse_selector_list("#ok");
    GLINTFX_CHECK(id_selector.ok);
    if (id_selector.ok) {
        const auto &simples = id_selector.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::id_selector);
        GLINTFX_CHECK(simples[0].name == std::string_view{"ok"});
    }

    const auto universal = parse_selector_list("*");
    GLINTFX_CHECK(universal.ok);
    if (universal.ok) {
        const auto &simples = universal.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::universal);
        GLINTFX_CHECK(simples[0].name.empty());
    }
    std::println("gltfx_gfss_parse_selector_list_recognizes_every_simple_selector_shape: 4 "
                 "shape(s) checked");
}

// ENUMERATION: the five functional pseudo-classes, swept from
// selector_pseudo_vocabulary.hpp's own k_functional_pseudo_names - the
// SAME closed-list technique the simple-pseudo test above uses. The
// round-trip proof asked for by this fatia's own service order is
// TWO checks per entry, not one: the raw argument text is byte-exact
// (not merely non-empty), AND it is a genuine view INTO the caller's
// own source buffer (its pointer range falls within `text`'s own),
// never a copy or a dangling pointer past the tokenizer's own scratch
// state.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_functional_pseudo_with_raw_argument) {
    struct functional_sample {
        std::string_view name;
        std::string_view argument;
    };
    static constexpr functional_sample k_samples[] = {
        {"nth-child", "2n+1"},     {"nth-last-child", "odd"}, {"nth-of-type", "3"},
        {"nth-last-of-type", "1"}, {"not", ".foo"},
    };
    static_assert(std::size(k_samples) == k_functional_pseudo_count,
                  "GODS_LAWS.md L-40: selector_pseudo_vocabulary.hpp's functional-pseudo list "
                  "changed - update this sweep to match");

    std::size_t swept = 0;
    for (const auto &sample : k_samples) {
        const std::string text =
            ":" + std::string(sample.name) + "(" + std::string(sample.argument) + ")";
        const auto result = parse_selector_list(text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            const auto &simples = result.value.selectors.front().head.simple_selectors;
            GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
            if (simples.size() == 1) {
                GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::pseudo_function);
                GLINTFX_CHECK(simples[0].name == sample.name);
                // Round trip, byte-exact - not merely "not empty".
                GLINTFX_CHECK(simples[0].raw_argument == sample.argument);
                // Round trip, genuine view into `text` - not a copy.
                GLINTFX_CHECK(simples[0].raw_argument.data() >= text.data());
                GLINTFX_CHECK(simples[0].raw_argument.data() + simples[0].raw_argument.size() <=
                              text.data() + text.size());
            }
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_functional_pseudo_count);
    std::println("gltfx_gfss_parse_selector_list_recognizes_every_functional_pseudo_with_raw_"
                 "argument: {} functional pseudo-class(es) checked",
                 swept);
}

// CONTROL NEGATIVO (this fatia's own service order): an unknown
// pseudo-class, of EITHER shape (bare ident or function), reproves
// with a diagnostic - never accepted in silence (GODS_LAWS.md L-28's
// own "linha aceita em silencio e o defeito que o lider mandou
// eliminar").
GLINTFX_TEST(gltfx_gfss_parse_selector_list_rejects_unknown_pseudo_class) {
    const auto unknown_simple = parse_selector_list(":bogus");
    GLINTFX_CHECK(!unknown_simple.ok);
    GLINTFX_CHECK(unknown_simple.diagnostic.expected == k_expected_known_pseudo_class);

    const auto unknown_function = parse_selector_list(":bogus(1)");
    GLINTFX_CHECK(!unknown_function.ok);
    GLINTFX_CHECK(unknown_function.diagnostic.expected == k_expected_known_pseudo_function);
}

// HOSTILE INPUT, ENUMERATED (GODS_LAWS.md L-40: "enumere o espaco
// pequeno, nao busque dentro dele") - every case this fatia's own
// service order names by name, each checked against the EXACT
// diagnostic identifier it is expected to produce (never merely
// "result.ok is false"), proving the parser fails CLOSED with a
// legible reason, never a crash and never silent acceptance.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_rejects_hostile_input_with_the_right_diagnostic) {
    struct hostile_sample {
        std::string_view text;
        std::string_view expected_diagnostic;
        std::string_view label;
    };
    const hostile_sample k_samples[] = {
        {"> a", k_expected_simple_selector, "combinator at the start"},
        {"a >", k_expected_simple_selector, "combinator at the end"},
        {"a > > b", k_expected_simple_selector, "two combinators in a row"},
        {"a,", k_expected_simple_selector, "dangling comma"},
        {":nth-child(2n+1", k_expected_closing_parenthesis, "unclosed parenthesis"},
        {"#", k_expected_simple_selector, "'#' with no name"},
        {".", k_expected_identifier_after_dot, "'.' with no name"},
        {":", k_expected_identifier_after_colon, "':' with no name"},
        {"a,,b", k_expected_simple_selector, "empty chain between two commas"},
        {"", k_expected_simple_selector, "only whitespace (empty string)"},
        {"   ", k_expected_simple_selector, "only whitespace (real spaces)"},
        {"a)", k_expected_comma_or_end_of_selector_list, "stray token after a complex selector"},
    };

    std::size_t swept = 0;
    for (const auto &sample : k_samples) {
        const auto result = parse_selector_list(std::string(sample.text));
        GLINTFX_CHECK(!result.ok);
        GLINTFX_CHECK(result.diagnostic.expected == sample.expected_diagnostic);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, static_cast<std::size_t>(12));
    std::println("gltfx_gfss_parse_selector_list_rejects_hostile_input_with_the_right_diagnostic: "
                 "{} hostile case(s) checked",
                 swept);
}

// ANINHAMENTO PROFUNDO (this fatia's own service order names this
// explicitly) - capture_functional_argument() (selector_parse.cpp) is
// an ITERATIVE scan over the token vector, never recursion, so an
// arbitrarily deep chain of nested parentheses inside a raw argument
// costs stack space proportional to ZERO (only the token vector's own
// O(n) memory) - this proves BOTH directions: a BALANCED deep chain
// resolves correctly (no crash, no hang, correct raw text), and an
// UNBALANCED one still reproves cleanly with closing_parenthesis,
// never a stack overflow or a hang.
GLINTFX_TEST(
    gltfx_gfss_parse_selector_list_handles_deeply_nested_functional_argument_without_recursing) {
    constexpr int k_depth = 5000;
    std::string balanced_argument(static_cast<std::size_t>(k_depth), '(');
    balanced_argument.append(static_cast<std::size_t>(k_depth), ')');
    const std::string balanced_text = ":not(" + balanced_argument + ")";
    const auto balanced_result = parse_selector_list(balanced_text);
    GLINTFX_CHECK(balanced_result.ok);
    if (balanced_result.ok) {
        const auto &simples = balanced_result.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        if (simples.size() == 1) {
            GLINTFX_CHECK(simples[0].raw_argument == std::string_view{balanced_argument});
        }
    }

    const std::string unbalanced_argument(static_cast<std::size_t>(k_depth), '(');
    const std::string unbalanced_text = ":not(" + unbalanced_argument;
    const auto unbalanced_result = parse_selector_list(unbalanced_text);
    GLINTFX_CHECK(!unbalanced_result.ok);
    GLINTFX_CHECK(unbalanced_result.diagnostic.expected == k_expected_closing_parenthesis);

    std::println("gltfx_gfss_parse_selector_list_handles_deeply_nested_functional_argument_without_"
                 "recursing: depth {} checked both ways",
                 k_depth);
}

// SIX OF THE TWELVE, PRODUCED FOR REAL BY THIS LAYER (GODS_LAWS.md
// L-40 - gfss_tokenizer_test.cpp's own T2 proves format for the WHOLE
// shared list and production for the tokenizer's own original four;
// closing_parenthesis is REUSED here, not re-proven, since that
// identifier's own production is already that file's job; GFSS-
// VALUE's own two, component_value/known_length_unit, are gfss_value_
// test.cpp's job, a layer this parser does not touch). An identifier
// this parser starts using with no directed row here fails silently at
// review time, not at compile time - the compile-time floor is the
// static_assert below, tied to the SAME shared count gfss_tokenizer_
// test.cpp's own static_assert already checks, so the two can never
// silently disagree about how many entries the list has.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_diagnostics_are_produced_from_the_shared_vocabulary) {
    static_assert(k_expected_vocabulary_count == 12,
                  "GODS_LAWS.md L-40: diagnostic_vocabulary.hpp's list changed - update the "
                  "directed-production coverage below to match (this is the SAME shared list "
                  "gfss_tokenizer_test.cpp's own static_assert checks)");

    GLINTFX_CHECK(parse_selector_list("").diagnostic.expected == k_expected_simple_selector);
    GLINTFX_CHECK(parse_selector_list(".").diagnostic.expected == k_expected_identifier_after_dot);
    GLINTFX_CHECK(parse_selector_list(":").diagnostic.expected ==
                  k_expected_identifier_after_colon);
    GLINTFX_CHECK(parse_selector_list(":bogus").diagnostic.expected ==
                  k_expected_known_pseudo_class);
    GLINTFX_CHECK(parse_selector_list(":bogus(1)").diagnostic.expected ==
                  k_expected_known_pseudo_function);
    GLINTFX_CHECK(parse_selector_list(":not(a").diagnostic.expected ==
                  k_expected_closing_parenthesis);
    GLINTFX_CHECK(parse_selector_list("a)").diagnostic.expected ==
                  k_expected_comma_or_end_of_selector_list);
}

// NO DUPLICATE WORD IN THE CONSOLIDATED LIST (project leader's
// decision of 27/08/2026, GODS_LAWS.md L-27 - the measured reason a
// third, gfss-selector-only vocabulary was rejected in favor of this
// single one): a real defect motivated this - the adversarial review
// of the sibling GFSS-COLOR-PARSE fatia found that its OWN separate
// list and diagnostic_vocabulary.hpp had, independently, both chosen
// "closing_parenthesis" for two DIFFERENT conditions, and NOTHING
// detected it, because neither list checked itself against anything
// but itself. This sweep closes exactly that gap for THIS list: O(n^2)
// pairwise comparison over ten short strings is not a performance
// concern, and enumerating the whole small space beats a targeted
// search for a suspected pair (GODS_LAWS.md L-40's own "enumere o
// espaco pequeno, nao busque dentro dele").
//
// SCOPE, STATED HONESTLY (GODS_LAWS.md L-27, marked INFERENCE - a
// decision made HERE, not itself ordered): this sweep proves the
// shared list (diagnostic_vocabulary.hpp) has no duplicate WITHIN
// itself. It does NOT sweep color_diagnostic_vocabulary.hpp - GFSS-
// COLOR-PARSE's own separate list is untouched by this fatia (its
// review is reproved on other grounds and its own consolidation, if
// any, is that fatia's author's work) - so the ORIGINAL finding that
// motivated this whole change (this list's own "closing_parenthesis"
// colliding with color's) is NOT re-checked live here; it is a
// pre-existing, already-reported fact this test does not reopen. A
// test that swept color's file too would fail red for a defect this
// fatia has no authority to fix, and a red gate for someone else's
// open finding is not a gate, it is noise (GODS_LAWS.md L-12's own
// "relatorio de agente nao e prova" cuts the other way here too: a
// gate is not proof if it can never turn green by fixing the code it
// claims to own).
GLINTFX_TEST(diagnostic_vocabulary_has_no_duplicate_word) {
    std::size_t compared = 0;
    for (std::size_t i = 0; i < k_expected_vocabulary.size(); ++i) {
        for (std::size_t j = i + 1; j < k_expected_vocabulary.size(); ++j) {
            GLINTFX_CHECK(k_expected_vocabulary[i] != k_expected_vocabulary[j]);
            ++compared;
        }
    }
    // GODS_LAWS.md L-40: zero compared is a floor violation, never a pass.
    GLINTFX_CHECK(compared > 0);
    GLINTFX_CHECK_EQ(compared, k_expected_vocabulary_count * (k_expected_vocabulary_count - 1) / 2);
    std::println(
        "diagnostic_vocabulary_has_no_duplicate_word: {} pair(s) compared, {} identifier(s)",
        compared, k_expected_vocabulary_count);
}
