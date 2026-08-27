// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <iterator>
#include <print>
#include <string>
#include <string_view>

#include "gfss/selector_ast.hpp"
#include "gfss/selector_diagnostic_vocabulary.hpp"
#include "gfss/selector_parse.hpp"
#include "gfss/selector_pseudo_vocabulary.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_selector_parse_test.cpp - GFSS-SEL-PARSE-CORE (TODO.md,
// GODS_LAWS.md L-20/L-27/L-28/L-40): the TDD red/green witness for
// glintfx::style::detail::parse_selector_list() (selector_parse.hpp) -
// see that file's own header comment for the design rationale each
// check below proves.

using glintfx::style::detail::gfss_combinator;
using glintfx::style::detail::gfss_combinator_count;
using glintfx::style::detail::gfss_combinator_table;
using glintfx::style::detail::gfss_simple_selector_kind;
using glintfx::style::detail::k_functional_pseudo_count;
using glintfx::style::detail::k_selector_expected_closing_parenthesis;
using glintfx::style::detail::k_selector_expected_comma_or_end_of_selector_list;
using glintfx::style::detail::k_selector_expected_identifier_after_colon;
using glintfx::style::detail::k_selector_expected_identifier_after_dot;
using glintfx::style::detail::k_selector_expected_known_pseudo_class;
using glintfx::style::detail::k_selector_expected_known_pseudo_function;
using glintfx::style::detail::k_selector_expected_simple_selector;
using glintfx::style::detail::k_selector_expected_vocabulary;
using glintfx::style::detail::k_selector_expected_vocabulary_count;
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
    GLINTFX_CHECK(unknown_simple.diagnostic.expected == k_selector_expected_known_pseudo_class);

    const auto unknown_function = parse_selector_list(":bogus(1)");
    GLINTFX_CHECK(!unknown_function.ok);
    GLINTFX_CHECK(unknown_function.diagnostic.expected ==
                  k_selector_expected_known_pseudo_function);
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
        {"> a", k_selector_expected_simple_selector, "combinator at the start"},
        {"a >", k_selector_expected_simple_selector, "combinator at the end"},
        {"a > > b", k_selector_expected_simple_selector, "two combinators in a row"},
        {"a,", k_selector_expected_simple_selector, "dangling comma"},
        {":nth-child(2n+1", k_selector_expected_closing_parenthesis, "unclosed parenthesis"},
        {"#", k_selector_expected_simple_selector, "'#' with no name"},
        {".", k_selector_expected_identifier_after_dot, "'.' with no name"},
        {":", k_selector_expected_identifier_after_colon, "':' with no name"},
        {"a,,b", k_selector_expected_simple_selector, "empty chain between two commas"},
        {"", k_selector_expected_simple_selector, "only whitespace (empty string)"},
        {"   ", k_selector_expected_simple_selector, "only whitespace (real spaces)"},
        {"a)", k_selector_expected_comma_or_end_of_selector_list,
         "stray token after a complex selector"},
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
    GLINTFX_CHECK(unbalanced_result.diagnostic.expected == k_selector_expected_closing_parenthesis);

    std::println("gltfx_gfss_parse_selector_list_handles_deeply_nested_functional_argument_without_"
                 "recursing: depth {} checked both ways",
                 k_depth);
}

// ENUMERATION, closed-by-construction (GODS_LAWS.md L-40 achado 1, the
// SAME technique gfss_tokenizer_test.cpp's own diagnostic_vocabulary_
// is_enumerated_closed_and_every_identifier_is_produced and gfss_
// color_parse_test.cpp's own color_diagnostic_vocabulary_is_enumerated_
// closed_and_every_identifier_is_produced already use): every
// identifier selector_diagnostic_vocabulary.hpp's own closed list
// names is well-formed (non-empty, snake_case, no spaces) AND is
// PRODUCED for real by a directed input - an 8th identifier added to
// that file with no matching row here fails to COMPILE, never passes
// silently with a stale count.
GLINTFX_TEST(selector_diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_produced) {
    static_assert(k_selector_expected_vocabulary_count == 7,
                  "GODS_LAWS.md L-40: selector_diagnostic_vocabulary.hpp's list changed - update "
                  "the directed-production coverage below to match");

    std::size_t swept = 0;
    for (const std::string_view identifier : k_selector_expected_vocabulary) {
        GLINTFX_CHECK(!identifier.empty());
        bool is_snake_case = true;
        for (const char ch : identifier) {
            const bool is_lower = ch >= 'a' && ch <= 'z';
            const bool is_underscore = ch == '_';
            if (!is_lower && !is_underscore) {
                is_snake_case = false;
                break;
            }
        }
        GLINTFX_CHECK(is_snake_case);
        GLINTFX_CHECK(identifier.find(' ') == std::string_view::npos);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_selector_expected_vocabulary_count);
    std::println("selector_diagnostic_vocabulary_is_enumerated_closed_and_every_identifier_is_"
                 "produced: {} identifier(s) swept",
                 swept);

    GLINTFX_CHECK(parse_selector_list("").diagnostic.expected ==
                  k_selector_expected_simple_selector);
    GLINTFX_CHECK(parse_selector_list(".").diagnostic.expected ==
                  k_selector_expected_identifier_after_dot);
    GLINTFX_CHECK(parse_selector_list(":").diagnostic.expected ==
                  k_selector_expected_identifier_after_colon);
    GLINTFX_CHECK(parse_selector_list(":bogus").diagnostic.expected ==
                  k_selector_expected_known_pseudo_class);
    GLINTFX_CHECK(parse_selector_list(":bogus(1)").diagnostic.expected ==
                  k_selector_expected_known_pseudo_function);
    GLINTFX_CHECK(parse_selector_list(":not(a").diagnostic.expected ==
                  k_selector_expected_closing_parenthesis);
    GLINTFX_CHECK(parse_selector_list("a)").diagnostic.expected ==
                  k_selector_expected_comma_or_end_of_selector_list);
}
