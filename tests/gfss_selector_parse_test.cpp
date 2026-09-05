// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <iterator>
#include <limits>
#include <print>
#include <string>
#include <string_view>

#include "gfss/anb.hpp"
#include "gfss/anb_parse.hpp"
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
// TWO SIBLING FATIAS ALSO LIVE IN THIS EXECUTABLE, 05/09/2026
// (GODS_LAWS.md L-17, marked INFERENCE - the orchestrator's own
// instruction for this pull, not a fact read from TODO.md itself):
// GFSS-SEL-PARSE-PSEUDO-ELEMENT extends parse_selector_list() itself
// (selector_parse.cpp's own parse_pseudo_element()) with the "::"
// sigil, so its own tests below sit alongside GFSS-SEL-PARSE-CORE's;
// GFSS-SEL-PARSE-NTH is a genuinely SEPARATE unit under test
// (glintfx::style::detail::parse_anb(), anb_parse.hpp) that happens to
// share this test BINARY rather than open a fourth one, because
// tests/CMakeLists.txt's own per-executable source wiring is outside
// both fatias' own file list for this pull (that file's "NAO TOQUE"
// boundary) and "prefira acrescentar casos a um binario que ja existe"
// was the orchestrator's own explicit instruction. Each test FUNCTION
// below stays its own atomic unit either way (GODS_LAWS.md L-17's own
// "uma unidade, um assunto" - a shared FILE is not a shared function).
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
using glintfx::style::detail::count_owned_by;
using glintfx::style::detail::gfss_attribute_operator;
using glintfx::style::detail::gfss_attribute_operator_count;
using glintfx::style::detail::gfss_attribute_operator_table;
using glintfx::style::detail::gfss_combinator;
using glintfx::style::detail::gfss_combinator_count;
using glintfx::style::detail::gfss_combinator_table;
using glintfx::style::detail::gfss_diagnostic_producer;
using glintfx::style::detail::gfss_simple_selector_kind;
using glintfx::style::detail::k_expected_anb_expression;
using glintfx::style::detail::k_expected_anb_offset;
using glintfx::style::detail::k_expected_attribute_name;
using glintfx::style::detail::k_expected_attribute_operator_or_close;
using glintfx::style::detail::k_expected_attribute_value;
using glintfx::style::detail::k_expected_closing_parenthesis;
using glintfx::style::detail::k_expected_closing_quote;
using glintfx::style::detail::k_expected_closing_square_bracket;
using glintfx::style::detail::k_expected_comma_or_end_of_selector_list;
using glintfx::style::detail::k_expected_end_of_anb_expression;
using glintfx::style::detail::k_expected_identifier_after_colon;
using glintfx::style::detail::k_expected_identifier_after_dot;
using glintfx::style::detail::k_expected_identifier_after_double_colon;
using glintfx::style::detail::k_expected_known_pseudo_class;
using glintfx::style::detail::k_expected_known_pseudo_element;
using glintfx::style::detail::k_expected_known_pseudo_function;
using glintfx::style::detail::k_expected_not_recursion_limit;
using glintfx::style::detail::k_expected_simple_selector;
using glintfx::style::detail::k_expected_vocabulary;
using glintfx::style::detail::k_expected_vocabulary_count;
using glintfx::style::detail::k_functional_pseudo_count;
using glintfx::style::detail::k_pseudo_element_count;
using glintfx::style::detail::k_pseudo_element_names;
using glintfx::style::detail::k_simple_pseudo_count;
using glintfx::style::detail::k_simple_pseudo_names;
using glintfx::style::detail::parse_anb;
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

// GFSS-SEL-PARSE-PSEUDO-ELEMENT (TODO.md, 05/09/2026, GODS_LAWS.md
// L-28 decision 11 of 26/08/2026: "::before e ::after entram na v1").
// ENUMERATION, not a directed sample (the SAME L-40/L-27 technique the
// simple/functional pseudo-class tests above already use): every one
// of the two pseudo-element names, swept from selector_pseudo_
// vocabulary.hpp's own k_pseudo_element_names - never a hand-copied
// second list that could drift from it.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_pseudo_element) {
    static_assert(k_pseudo_element_count == 2,
                  "GODS_LAWS.md L-40: selector_pseudo_vocabulary.hpp's pseudo-element list "
                  "changed - this fatia's own service order names exactly 2, update it or this "
                  "count");

    std::size_t swept = 0;
    for (const std::string_view name : k_pseudo_element_names) {
        const std::string text = "::" + std::string(name);
        const auto result = parse_selector_list(text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
            const auto &simples = result.value.selectors.front().head.simple_selectors;
            GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
            if (simples.size() == 1) {
                GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::pseudo_element);
                GLINTFX_CHECK(simples[0].name == name);
                GLINTFX_CHECK(simples[0].raw_argument.empty());
            }
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, k_pseudo_element_count);
    std::println("gltfx_gfss_parse_selector_list_recognizes_every_pseudo_element: {} pseudo-"
                 "element(s) checked",
                 swept);
}

// COMBINED WITH A TYPE SELECTOR, THE REALISTIC SHAPE (this fatia's own
// service order): "li::after" is ONE compound selector, two simple
// selectors - the type "li" then the pseudo-element "after", glued
// with no combinator between them (selector_ast.hpp's own compound
// definition).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_type_then_pseudo_element) {
    const auto result = parse_selector_list("li::after");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(2));
    if (simples.size() == 2) {
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::type);
        GLINTFX_CHECK(simples[0].name == std::string_view{"li"});
        GLINTFX_CHECK(simples[1].kind == gfss_simple_selector_kind::pseudo_element);
        GLINTFX_CHECK(simples[1].name == std::string_view{"after"});
    }
}

// CONTROL NEGATIVO, PSEUDO-ELEMENT (this fatia's own service order):
// an unknown "::name" reproves with known_pseudo_element, and "::"
// with no adjacent ident at all reproves with identifier_after_double_
// colon - the SAME two-diagnostic shape the single-colon forms already
// have (identifier_after_colon/known_pseudo_class), never a crash and
// never silent acceptance.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_rejects_unknown_pseudo_element) {
    const auto unknown = parse_selector_list("::bogus");
    GLINTFX_CHECK(!unknown.ok);
    GLINTFX_CHECK(unknown.diagnostic.expected == k_expected_known_pseudo_element);

    const auto empty = parse_selector_list("::");
    GLINTFX_CHECK(!empty.ok);
    GLINTFX_CHECK(empty.diagnostic.expected == k_expected_identifier_after_double_colon);
}

// REGRESSION, SINGLE-COLON FORMS DO NOT REGRESS (the orchestrator's own
// explicit instruction: "rode a suite de seletor antes e depois" -
// pinned here as a permanent assertion, not just a one-time manual
// run). ":before"/":after" are the LEGACY CSS2.1 single-colon spelling
// this fatia does NOT add (selector_pseudo_vocabulary.hpp's own
// GLINTFX_GFSS_SIMPLE_PSEUDO_LIST still has no "before"/"after" row) -
// they must keep failing exactly as they did before this fatia, with
// known_pseudo_class (the single-colon dispatch path), never silently
// promoted to a pseudo_element and never confused with the double-
// colon known_pseudo_element diagnostic above. ":hover" is the
// existing control - still a real pseudo_class, unchanged.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_single_colon_forms_do_not_regress) {
    const auto legacy_before = parse_selector_list(":before");
    GLINTFX_CHECK(!legacy_before.ok);
    GLINTFX_CHECK(legacy_before.diagnostic.expected == k_expected_known_pseudo_class);

    const auto legacy_after = parse_selector_list(":after");
    GLINTFX_CHECK(!legacy_after.ok);
    GLINTFX_CHECK(legacy_after.diagnostic.expected == k_expected_known_pseudo_class);

    const auto still_hover = parse_selector_list(":hover");
    GLINTFX_CHECK(still_hover.ok);
    if (still_hover.ok) {
        const auto &simples = still_hover.value.selectors.front().head.simple_selectors;
        GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
        if (simples.size() == 1) {
            GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::pseudo_class);
        }
    }
}

// ======================================================================
// GFSS-SEL-PARSE-ATTR (TODO.md, 05/09/2026) - the attribute selector,
// "[foo]" (presence) and "[foo<op>value]" (one of six comparison
// operators). See selector_ast.hpp's own header comment on gfss_
// attribute_operator for the "7 operadores" discrepancy this fatia's
// own TODO.md row carries (six named operators, presence named
// separately in the same sentence) and selector_parse.cpp's own top
// comment for the grammar this block proves.
// ======================================================================

// FIRST ASSERTION for this fatia (GODS_LAWS.md L-20 "veja o teste
// falhar"): the bare presence form carries no operator and no value -
// `has_attribute_value` stays false, and `attribute_operator`/
// `attribute_value` are never read (selector_ast.hpp's own header
// comment on gfss_simple_selector explains why they are left at their
// defaults rather than checked here).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_attribute_presence) {
    const auto result = parse_selector_list("[foo]");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
    if (simples.size() == 1) {
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::attribute);
        GLINTFX_CHECK(simples[0].name == std::string_view{"foo"});
        GLINTFX_CHECK(!simples[0].has_attribute_value);
    }
}

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado") - every one of the six
// operators selector_ast.hpp's own gfss_attribute_operator_table
// lists, swept directly: a 7th operator added to that table with no
// matching case here fails to compile (the static_assert below), never
// passes silently with a stale count. The unquoted-ident value form
// ("bar") is used for every entry here; the quoted-string form gets its
// own dedicated proof below (gltfx_gfss_parse_selector_list_reads_
// attribute_value_as_quoted_string), since this sweep is about the
// OPERATOR, not the value grammar.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_recognizes_every_attribute_operator) {
    static_assert(gfss_attribute_operator_count == 6,
                  "GODS_LAWS.md L-40: selector_ast.hpp's attribute-operator table changed - "
                  "update this sweep to match");

    std::size_t swept = 0;
    for (const auto &entry : gfss_attribute_operator_table) {
        const std::string prefix =
            (entry.prefix == '\0') ? std::string{} : std::string(1, entry.prefix);
        const std::string text = "[foo" + prefix + "=bar]";
        const auto result = parse_selector_list(text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            const auto &simples = result.value.selectors.front().head.simple_selectors;
            GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
            if (simples.size() == 1) {
                GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::attribute);
                GLINTFX_CHECK(simples[0].name == std::string_view{"foo"});
                GLINTFX_CHECK(simples[0].has_attribute_value);
                GLINTFX_CHECK(simples[0].attribute_operator == entry.op);
                GLINTFX_CHECK(simples[0].attribute_value == std::string_view{"bar"});
            }
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gfss_attribute_operator_count);
    std::println("gltfx_gfss_parse_selector_list_recognizes_every_attribute_operator: {} "
                 "operator(s) checked",
                 swept);
}

// THE CSS-CONVENTION DEFAULT THIS FATIA'S OWN SERVICE ORDER REGISTERS
// "PARA VETO" (not blocking, but named as a decision the leader can
// still revisit): an unquoted ident and a quoted string are BOTH
// accepted as the value, and a quoted string's own surrounding quote
// characters are stripped from `attribute_value` (selector_parse.cpp's
// own parse_attribute_value()) - internal whitespace inside the quotes
// survives verbatim, proving the stripping is exactly the two
// delimiter bytes, never a general trim.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_attribute_value_as_quoted_string) {
    const auto result = parse_selector_list("[foo=\"bar baz\"]");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
    if (simples.size() == 1) {
        GLINTFX_CHECK(simples[0].has_attribute_value);
        GLINTFX_CHECK(simples[0].attribute_value == std::string_view{"bar baz"});
    }
}

// COMBINED WITH A TYPE SELECTOR, THE REALISTIC SHAPE (the SAME "li::
// after" precedent gltfx_gfss_parse_selector_list_reads_type_then_
// pseudo_element above already established for pseudo-elements):
// "button[foo]" is ONE compound selector, two simple selectors - the
// type "button" then the attribute "foo", glued with no combinator
// between them.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_type_then_attribute) {
    const auto result = parse_selector_list("button[foo]");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(2));
    if (simples.size() == 2) {
        GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::type);
        GLINTFX_CHECK(simples[0].name == std::string_view{"button"});
        GLINTFX_CHECK(simples[1].kind == gfss_simple_selector_kind::attribute);
        GLINTFX_CHECK(simples[1].name == std::string_view{"foo"});
    }
}

// WHITESPACE PERMITTED AROUND EVERY PART (CSS2.1's own "attrib" grammar
// - selector_parse.cpp's own top comment cites it) - proves the parser
// does not require the tight, no-space spelling the operator sweep
// above happens to use.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_allows_whitespace_inside_attribute_brackets) {
    const auto result = parse_selector_list("[ foo = bar ]");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
    if (simples.size() == 1) {
        GLINTFX_CHECK(simples[0].name == std::string_view{"foo"});
        GLINTFX_CHECK(simples[0].has_attribute_value);
        GLINTFX_CHECK(simples[0].attribute_operator == gfss_attribute_operator::equals);
        GLINTFX_CHECK(simples[0].attribute_value == std::string_view{"bar"});
    }
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
        // GFSS-SEL-PARSE-ATTR (TODO.md, 05/09/2026) - the six hostile
        // cases this fatia's own service order names by name.
        {"[]", k_expected_attribute_name, "empty attribute ('[]')"},
        {"[foo", k_expected_attribute_operator_or_close,
         "unclosed bracket, presence form (no operator at all)"},
        {"[foo=bar", k_expected_closing_square_bracket,
         "unclosed bracket, after a complete comparison"},
        {"[foo %= bar]", k_expected_attribute_operator_or_close, "invented operator ('%=')"},
        {"[foo=]", k_expected_attribute_value, "missing value ('[foo=]')"},
        {"[foo='bar", k_expected_closing_quote, "unterminated quotes in the value"},
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
    GLINTFX_CHECK_EQ(swept, static_cast<std::size_t>(18));
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
//
// "nth-child", NOT "not" (GODS_LAWS.md L-17's own "o gemeo" - this test
// predates GFSS-SEL-PARSE-NOT and originally used ":not(...)" as its own
// vehicle, since capture_functional_argument() does not care WHICH
// functional pseudo-class it serves): now that "not" specifically
// RECURSES into its own argument as real selector grammar
// (attach_not_argument(), selector_parse.cpp), a raw argument made of
// nothing but bare parentheses is no longer a case this test can use
// "not" for - it would fail as a malformed selector list (correctly -
// see gltfx_gfss_parse_selector_list_rejects_hostile_not_argument_input
// below for THAT proof) rather than exercising the CAPTURE mechanism in
// isolation. "nth-child" still only captures its raw argument, unread
// (GFSS-SEL-PARSE-NTH's own An+B microparser is a standalone utility,
// never wired into this file - anb_parse.hpp's own header comment), so
// it is the one that still isolates capture_functional_argument()'s own
// depth counter from any content validation.
GLINTFX_TEST(
    gltfx_gfss_parse_selector_list_handles_deeply_nested_functional_argument_without_recursing) {
    constexpr int k_depth = 5000;
    std::string balanced_argument(static_cast<std::size_t>(k_depth), '(');
    balanced_argument.append(static_cast<std::size_t>(k_depth), ')');
    const std::string balanced_text = ":nth-child(" + balanced_argument + ")";
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
    const std::string unbalanced_text = ":nth-child(" + unbalanced_argument;
    const auto unbalanced_result = parse_selector_list(unbalanced_text);
    GLINTFX_CHECK(!unbalanced_result.ok);
    GLINTFX_CHECK(unbalanced_result.diagnostic.expected == k_expected_closing_parenthesis);

    std::println("gltfx_gfss_parse_selector_list_handles_deeply_nested_functional_argument_without_"
                 "recursing: depth {} checked both ways",
                 k_depth);
}

// ======================================================================
// GFSS-SEL-PARSE-NOT (TODO.md, 05/09/2026) - `:not(s1, s2, ...)` with a
// LIST of COMPLEX selectors, recursion on this SAME parser
// (selector_parse.cpp's own parse_not_argument()/attach_not_argument()),
// and the anti-DoS depth limit that keeps that recursion finite.
// ======================================================================

// FIRST ASSERTION for this fatia (GODS_LAWS.md L-20 "veja o teste
// falhar"): `:not(.foo, #bar)` recursively parses into TWO complex
// selectors, each ONE compound of ONE simple selector - proving the
// comma inside the argument is read as a LIST separator (the SAME
// grammar the outer parser already uses for its own top-level list, now
// applied one level down) rather than being left as part of the raw
// text. `raw_argument` itself stays byte-exact and unmodified
// (gfss_selector_parse_test.cpp's own pre-existing round-trip proof for
// EVERY functional pseudo-class - selector_ast.hpp's own header comment
// on why both fields coexist).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_not_with_selector_list_argument) {
    const auto result = parse_selector_list(":not(.foo, #bar)");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(1));
    if (simples.size() != 1) {
        return;
    }
    const auto &not_selector = simples[0];
    GLINTFX_CHECK(not_selector.kind == gfss_simple_selector_kind::pseudo_function);
    GLINTFX_CHECK(not_selector.name == std::string_view{"not"});
    GLINTFX_CHECK(not_selector.raw_argument == std::string_view{".foo, #bar"});

    GLINTFX_CHECK_EQ(not_selector.not_selectors.size(), static_cast<std::size_t>(2));
    if (not_selector.not_selectors.size() != 2) {
        return;
    }
    const auto &first = not_selector.not_selectors[0].head.simple_selectors;
    GLINTFX_CHECK_EQ(first.size(), static_cast<std::size_t>(1));
    if (first.size() == 1) {
        GLINTFX_CHECK(first[0].kind == gfss_simple_selector_kind::class_selector);
        GLINTFX_CHECK(first[0].name == std::string_view{"foo"});
    }
    const auto &second = not_selector.not_selectors[1].head.simple_selectors;
    GLINTFX_CHECK_EQ(second.size(), static_cast<std::size_t>(1));
    if (second.size() == 1) {
        GLINTFX_CHECK(second[0].kind == gfss_simple_selector_kind::id_selector);
        GLINTFX_CHECK(second[0].name == std::string_view{"bar"});
    }
}

// THE FORMAT'S OWN DOC EXAMPLE, VERIFIED (this fatia's own service
// order quotes it verbatim): `div:not(:nth-child(2), p > *)` - a COMPLEX
// selector ("p > *", compound "p" then a child combinator then the
// universal selector) and a FUNCTIONAL pseudo-class ("nth-child(2)")
// both inside the SAME `:not()` argument list, proving the recursion
// calls the FULL parser (core + nth + attr, this fatia's own service
// order says by name), not a cut-down "simple selectors only" reading.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_reads_the_format_doc_not_example) {
    const auto result = parse_selector_list("div:not(:nth-child(2), p > *)");
    GLINTFX_CHECK(result.ok);
    if (!result.ok) {
        return;
    }
    const auto &simples = result.value.selectors.front().head.simple_selectors;
    GLINTFX_CHECK_EQ(simples.size(), static_cast<std::size_t>(2));
    if (simples.size() != 2) {
        return;
    }
    GLINTFX_CHECK(simples[0].kind == gfss_simple_selector_kind::type);
    GLINTFX_CHECK(simples[0].name == std::string_view{"div"});
    const auto &not_selector = simples[1];
    GLINTFX_CHECK(not_selector.kind == gfss_simple_selector_kind::pseudo_function);
    GLINTFX_CHECK(not_selector.name == std::string_view{"not"});
    GLINTFX_CHECK_EQ(not_selector.not_selectors.size(), static_cast<std::size_t>(2));
    if (not_selector.not_selectors.size() != 2) {
        return;
    }

    const auto &nth_child = not_selector.not_selectors[0].head.simple_selectors;
    GLINTFX_CHECK_EQ(nth_child.size(), static_cast<std::size_t>(1));
    if (nth_child.size() == 1) {
        GLINTFX_CHECK(nth_child[0].kind == gfss_simple_selector_kind::pseudo_function);
        GLINTFX_CHECK(nth_child[0].name == std::string_view{"nth-child"});
        GLINTFX_CHECK(nth_child[0].raw_argument == std::string_view{"2"});
    }

    const auto &p_then_universal = not_selector.not_selectors[1];
    GLINTFX_CHECK_EQ(p_then_universal.head.simple_selectors.size(), static_cast<std::size_t>(1));
    if (p_then_universal.head.simple_selectors.size() == 1) {
        GLINTFX_CHECK(p_then_universal.head.simple_selectors[0].kind ==
                      gfss_simple_selector_kind::type);
        GLINTFX_CHECK(p_then_universal.head.simple_selectors[0].name == std::string_view{"p"});
    }
    GLINTFX_CHECK_EQ(p_then_universal.rest.size(), static_cast<std::size_t>(1));
    if (p_then_universal.rest.size() == 1) {
        GLINTFX_CHECK(p_then_universal.rest[0].combinator == gfss_combinator::child);
        GLINTFX_CHECK_EQ(p_then_universal.rest[0].compound.simple_selectors.size(),
                         static_cast<std::size_t>(1));
        if (p_then_universal.rest[0].compound.simple_selectors.size() == 1) {
            GLINTFX_CHECK(p_then_universal.rest[0].compound.simple_selectors[0].kind ==
                          gfss_simple_selector_kind::universal);
        }
    }
}

// THE DIAGNOSTIC POINTS INSIDE THE ARGUMENT, NOT AT THE OUTER ":not("
// (this fatia's own service order, verbatim: "a coluna tem de apontar
// dentro do argumento, nao no :not de fora - senao o autor da folha
// procura o erro no lugar errado"). ":not(a, .)" - a malformed second
// selector, a bare '.' with no class name after it - must reprove with
// the EXACT diagnostic identifier_after_dot already names for that
// defect at the TOP level (control: the second check below), AND its
// own line/column must land on the '.' INSIDE the argument (column 4 of
// "a, ." - "a"=1, ","=2, " "=3, "."=4 - gltfx_gfss_tokenize() re-numbers
// column from 1 for whatever buffer it receives, parse_not_argument()'s
// own header comment explains why), never column 1 (where the outer
// ":not(" token itself starts).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_not_diagnostic_points_inside_the_argument) {
    const auto result = parse_selector_list(":not(a, .)");
    GLINTFX_CHECK(!result.ok);
    GLINTFX_CHECK(result.diagnostic.expected == k_expected_identifier_after_dot);
    GLINTFX_CHECK_EQ(result.diagnostic.line, static_cast<std::uint32_t>(1));
    GLINTFX_CHECK_EQ(result.diagnostic.column, static_cast<std::uint32_t>(4));

    const auto control = parse_selector_list(".");
    GLINTFX_CHECK(!control.ok);
    GLINTFX_CHECK(control.diagnostic.expected == k_expected_identifier_after_dot);
    GLINTFX_CHECK_EQ(control.diagnostic.column, static_cast<std::uint32_t>(1));
}

// HOSTILE INPUT, ENUMERATED (GODS_LAWS.md L-40: "enumere o espaco
// pequeno, nao busque dentro dele") - `:not()`'s own argument grammar is
// the exact same complex-selector-list grammar the outer parser already
// has, so every hostile case below is a NEW combination of failure modes
// this fatia's own recursion introduces, never a duplicate of the outer
// sweep above: an EMPTY argument (never a valid production - the SAME
// "no production is the empty string" rule anb_parse.cpp's own top
// comment already names for a different grammar), a dangling comma
// inside the argument, and an unknown pseudo-class INSIDE the argument
// (proving the recursive call fails CLOSED on the SAME diagnostic
// vocabulary the outer parser uses, never a silently-accepted unknown
// name just because it is one level down).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_rejects_hostile_not_argument_input) {
    struct hostile_sample {
        std::string_view text;
        std::string_view expected_diagnostic;
        std::string_view label;
    };
    const hostile_sample k_samples[] = {
        {":not()", k_expected_simple_selector, "empty argument"},
        {":not(.foo,)", k_expected_simple_selector, "dangling comma inside the argument"},
        {":not(:bogus)", k_expected_known_pseudo_class, "unknown pseudo-class inside the argument"},
        {":not(a", k_expected_closing_parenthesis, "unclosed outer parenthesis"},
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
    GLINTFX_CHECK_EQ(swept, static_cast<std::size_t>(4));
    std::println("gltfx_gfss_parse_selector_list_rejects_hostile_not_argument_input: {} hostile "
                 "case(s) checked",
                 swept);
}

// THE DEPTH LIMIT ITSELF, TESTED ONE STEP PAST ITS OWN BOUNDARY
// (GODS_LAWS.md L-43's own "testar sempre um passo alem da fronteira
// recem-alargada" - applied here to a boundary being INTRODUCED, not
// widened, but the same discipline: prove the LAST accepted depth AND
// the first rejected one, never only one side). selector_parse.cpp's
// own k_max_not_nesting_depth is 10 and INTERNAL (not exposed through
// any header - GFSS-API, wave W10, decides what of this ever becomes
// public), so this test does not reference it by name; it instead
// builds the two strings that bracket whatever that bound is FROM THE
// OUTSIDE, the same way a consumer of this library would have to.
GLINTFX_TEST(gltfx_gfss_parse_selector_list_enforces_not_nesting_depth_limit) {
    auto build_not_chain = [](int nesting_count) {
        std::string text;
        for (int i = 0; i < nesting_count; ++i) {
            text += ":not(";
        }
        text += "a";
        for (int i = 0; i < nesting_count; ++i) {
            text += ")";
        }
        return text;
    };

    const std::string at_limit = build_not_chain(10);
    const auto at_limit_result = parse_selector_list(at_limit);
    GLINTFX_CHECK(at_limit_result.ok);

    const std::string one_past_limit = build_not_chain(11);
    const auto one_past_limit_result = parse_selector_list(one_past_limit);
    GLINTFX_CHECK(!one_past_limit_result.ok);
    GLINTFX_CHECK(one_past_limit_result.diagnostic.expected == k_expected_not_recursion_limit);

    std::println("gltfx_gfss_parse_selector_list_enforces_not_nesting_depth_limit: boundary proven "
                 "both ways (10 nesting levels ok, 11 reproved)");
}

// TWELVE OF THE NINETEEN, PRODUCED FOR REAL BY THIS LAYER (GODS_LAWS.md
// L-40 - gfss_tokenizer_test.cpp's own T2 proves format for the WHOLE
// shared list and production for the tokenizer's own original four;
// closing_parenthesis and closing_quote are REUSED here, not re-proven,
// since each identifier's own production is already that file's job;
// GFSS-VALUE's own two, component_value/known_dimension_unit, are
// gfss_value_test.cpp's job, a layer this parser does not touch;
// anb_parse's own three - anb_expression/anb_offset/end_of_anb_
// expression - are this SAME file's own GFSS-SEL-PARSE-NTH block
// below, under a DIFFERENT producer tag, so they are not part of this
// table either).
//
// GFSS-VOCAB-BIND (TODO.md, GODS_LAWS.md L-40): this used to be
// static_assert(k_expected_vocabulary_count == 12, ...) - a human
// tripwire tied to the WHOLE list's size, not to THIS layer's own
// share. An identifier added under gfss_diagnostic_producer::
// selector_parse bumped the same shared "12" gfss_tokenizer_test.cpp's
// own static_assert already checked, so this file's own coverage below
// could silently fall behind while that unrelated static_assert still
// matched. k_selector_diagnostic_samples below is this layer's own
// directed-production table (the twelve it alone owns - closing_
// parenthesis and closing_quote, both reused above, are NOT among
// them; identifier_after_double_colon/known_pseudo_element are GFSS-
// SEL-PARSE-PSEUDO-ELEMENT's own two, 05/09/2026; attribute_name/
// attribute_operator_or_close/attribute_value/closing_square_bracket
// are GFSS-SEL-PARSE-ATTR's own four, the SAME day - all six added
// under this SAME producer since they are emitted by this SAME file,
// selector_parse.cpp); the static_assert ties its size to count_owned_
// by(selector_parse) - counted MECHANICALLY from diagnostic_
// vocabulary.hpp's own list - so an identifier added under this
// producer with no matching row here now FAILS TO COMPILE (this
// pull's own RED for GFSS-SEL-PARSE-ATTR: adding the four new rows to
// diagnostic_vocabulary.hpp's own list, with this table still at eight
// samples, reproved the build with exactly this static_assert - "the
// comparison reduces to '(8 == 12)'" - before parse_attribute_
// selector() existed to make them pass).
GLINTFX_TEST(gltfx_gfss_parse_selector_list_diagnostics_are_produced_from_the_shared_vocabulary) {
    struct diagnostic_sample {
        std::string_view source;
        std::string_view expected_identifier;
    };
    static constexpr diagnostic_sample k_selector_diagnostic_samples[] = {
        {"", k_expected_simple_selector},
        {".", k_expected_identifier_after_dot},
        {":", k_expected_identifier_after_colon},
        {":bogus", k_expected_known_pseudo_class},
        {":bogus(1)", k_expected_known_pseudo_function},
        {"a)", k_expected_comma_or_end_of_selector_list},
        {"::", k_expected_identifier_after_double_colon},
        {"::bogus", k_expected_known_pseudo_element},
        {"[]", k_expected_attribute_name},
        {"[foo", k_expected_attribute_operator_or_close},
        {"[foo=]", k_expected_attribute_value},
        {"[foo=bar", k_expected_closing_square_bracket},
        // GFSS-SEL-PARSE-NOT (TODO.md, 05/09/2026) - its own one row:
        // 11 nested ":not(" is one past this file's own chosen bound
        // (gltfx_gfss_parse_selector_list_enforces_not_nesting_depth_
        // limit below proves the boundary itself; this row only proves
        // the DIAGNOSTIC is produced from the shared vocabulary).
        {":not(:not(:not(:not(:not(:not(:not(:not(:not(:not(:not(a)))))))))))",
         k_expected_not_recursion_limit},
    };
    static_assert(std::size(k_selector_diagnostic_samples) ==
                      count_owned_by(gfss_diagnostic_producer::selector_parse),
                  "GODS_LAWS.md L-40 (GFSS-VOCAB-BIND): diagnostic_vocabulary.hpp's "
                  "selector_parse-owned identifiers changed - add a directed production row to "
                  "k_selector_diagnostic_samples above, this does not compile otherwise");

    std::size_t swept = 0;
    for (const diagnostic_sample &sample : k_selector_diagnostic_samples) {
        GLINTFX_CHECK(parse_selector_list(std::string(sample.source)).diagnostic.expected ==
                      sample.expected_identifier);
        ++swept;
    }
    GLINTFX_CHECK_EQ(swept, count_owned_by(gfss_diagnostic_producer::selector_parse));

    // closing_parenthesis and closing_quote are both TOKENIZER-owned,
    // reused here (this parser propagates each upward - the first from
    // an unterminated functional-pseudo argument, the second from an
    // unterminated string used as an attribute value) - proven as an
    // EXTRA check, not counted in k_selector_diagnostic_samples above,
    // since each identifier's own production is gfss_tokenizer_test.
    // cpp's job.
    GLINTFX_CHECK(parse_selector_list(":not(a").diagnostic.expected ==
                  k_expected_closing_parenthesis);
    GLINTFX_CHECK(parse_selector_list("[foo='bar").diagnostic.expected == k_expected_closing_quote);
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
// ======================================================================
// GFSS-SEL-PARSE-NTH (TODO.md, 05/09/2026) - the An+B microparser,
// glintfx::style::detail::parse_anb() (anb_parse.hpp). See that file's
// own header comment and anb_parse.cpp's own top comment for the
// source (CSS Syntax Module Level 3 SS6 "The An+B microsyntax",
// https://www.w3.org/TR/css-syntax-3/#anb-microsyntax, cross-checked
// against MDN's own :nth-child() page) and the 16-production grammar
// each sample below names by number.
// ======================================================================

// ENUMERATION, not a directed sample (GODS_LAWS.md L-40/L-27: "enumere
// o espaco pequeno quando ele for fechado") - one sample per
// PRODUCTION of the closed 16-alternative <an+b> grammar (anb_parse.cpp's
// own top comment numbers them 1-16), chosen so distinct samples
// exercise genuinely distinct TOKEN sequences (e.g. "3n+1" merges the
// sign into the number token itself - production 10 - while "3n + 1"
// keeps it a separate delim token - production 16 - even though both
// mean a=3,b=1) rather than two samples that happen to produce the
// same (a,b) by the same code path.
GLINTFX_TEST(gltfx_gfss_parse_anb_recognizes_every_production) {
    struct anb_sample {
        std::string_view text;
        long long expected_a{};
        long long expected_b{};
        std::string_view label;
    };
    static constexpr anb_sample k_samples[] = {
        {"odd", 0, 1, "production 1: odd"},
        {"even", 2, 0, "production 2: even"},
        {"5", 0, 5, "production 3: <integer>"},
        {"3n", 3, 0, "production 4: <n-dimension>"},
        {"n", 1, 0, "production 5: '+'? n (bare)"},
        {"-n", -1, 0, "production 6: -n"},
        {"3n-1", 3, -1, "production 7: <ndashdigit-dimension>"},
        {"n-1", 1, -1, "production 8: '+'? <ndashdigit-ident> (bare)"},
        {"-n-1", -1, -1, "production 9: <dashndashdigit-ident>"},
        {"3n+1", 3, 1, "production 10: <n-dimension> <signed-integer>"},
        {"n+1", 1, 1, "production 11: '+'? n <signed-integer> (bare)"},
        {"-n+1", -1, 1, "production 12: -n <signed-integer>"},
        {"3n- 1", 3, -1, "production 13: <ndash-dimension> <signless-integer>"},
        {"n- 1", 1, -1, "production 14: '+'? n- <signless-integer> (bare)"},
        {"-n- 1", -1, -1, "production 15: -n- <signless-integer>"},
        {"3n + 1", 3, 1, "production 16: <n-dimension> ['+'|'-'] <signless-integer>"},
    };
    static_assert(std::size(k_samples) == 16,
                  "CSS Syntax Module Level 3 SS6.2 names exactly 16 alternatives for the <an+b> "
                  "production - update this enumeration to match if that grammar is re-read");

    std::size_t swept = 0;
    for (const auto &sample : k_samples) {
        const auto result = parse_anb(sample.text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            GLINTFX_CHECK_EQ(result.value.a, sample.expected_a);
            GLINTFX_CHECK_EQ(result.value.b, sample.expected_b);
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, static_cast<std::size_t>(16));
    std::println("gltfx_gfss_parse_anb_recognizes_every_production: {} production(s) checked",
                 swept);
}

// EXTRA, NON-CANONICAL BUT VALID FORMS - the optional leading '+'
// (productions 5/8/14) and the whitespace flexibility CSS Syntax's own
// SS6.1 names ("whitespace is permitted on either side of the + or -
// that separates the An and B parts"), including production 16's OWN
// bare-"n" twin ("n + 1"), which the enumeration above deliberately
// picked the dimension form for instead (this file's own header
// comment on that test). Not tied to a fixed count - these are
// EXTRA coverage on top of the 16-production closure, not part of it.
GLINTFX_TEST(gltfx_gfss_parse_anb_recognizes_optional_plus_and_whitespace_variants) {
    struct anb_sample {
        std::string_view text;
        long long expected_a{};
        long long expected_b{};
    };
    static constexpr anb_sample k_samples[] = {
        {"+n", 1, 0},    {"+3n", 3, 0},   {"+n-1", 1, -1},  {"+n- 1", 1, -1},  {"3n +1", 3, 1},
        {"3n+ 1", 3, 1}, {"n + 1", 1, 1}, {"n - 1", 1, -1}, {"-n -1", -1, -1}, {"0", 0, 0},
    };

    std::size_t swept = 0;
    for (const auto &sample : k_samples) {
        const auto result = parse_anb(sample.text);
        GLINTFX_CHECK(result.ok);
        if (result.ok) {
            GLINTFX_CHECK_EQ(result.value.a, sample.expected_a);
            GLINTFX_CHECK_EQ(result.value.b, sample.expected_b);
        }
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, std::size(k_samples));
    std::println(
        "gltfx_gfss_parse_anb_recognizes_optional_plus_and_whitespace_variants: {} variant(s) "
        "checked",
        swept);
}

// HOSTILE INPUT, ENUMERATED (GODS_LAWS.md L-40: "enumere o espaco
// pequeno, nao busque dentro dele") - every case checked against the
// EXACT diagnostic identifier it is expected to produce, never merely
// "result.ok is false".
GLINTFX_TEST(gltfx_gfss_parse_anb_rejects_hostile_input_with_the_right_diagnostic) {
    struct hostile_sample {
        std::string_view text;
        std::string_view expected_diagnostic;
        std::string_view label;
    };
    static constexpr hostile_sample k_samples[] = {
        {"", k_expected_anb_expression, "empty string"},
        {"   ", k_expected_anb_expression, "whitespace only"},
        {"foo", k_expected_anb_expression, "unknown ident"},
        {"3.5n", k_expected_anb_expression, "non-integer coefficient dimension"},
        {"3.5", k_expected_anb_expression, "non-integer bare number"},
        {"-", k_expected_anb_expression, "lone minus sign"},
        {"\"str\"", k_expected_anb_expression, "a string token"},
        {"n-", k_expected_anb_offset, "'n-' with nothing after"},
        {"3n-", k_expected_anb_offset, "'3n-' with nothing after"},
        {"n+1.5", k_expected_anb_offset, "non-integer signed offset"},
        {"n+ 1.5", k_expected_anb_offset, "non-integer signless offset after separate sign"},
        {"n++1", k_expected_anb_offset, "double sign"},
        {"n+-1", k_expected_anb_offset, "opposite double sign"},
        {"n+", k_expected_anb_offset, "sign with nothing after"},
        {"3n extra", k_expected_end_of_anb_expression, "trailing garbage after complete value"},
        {"odd 1", k_expected_end_of_anb_expression, "trailing garbage after keyword"},
        {"5 5", k_expected_end_of_anb_expression, "trailing garbage after bare integer"},
        {"n 1", k_expected_end_of_anb_expression, "unsigned integer with no sign delim at all"},
    };

    std::size_t swept = 0;
    for (const auto &sample : k_samples) {
        const auto result = parse_anb(sample.text);
        GLINTFX_CHECK(!result.ok);
        GLINTFX_CHECK(result.diagnostic.expected == sample.expected_diagnostic);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, static_cast<std::size_t>(18));
    std::println("gltfx_gfss_parse_anb_rejects_hostile_input_with_the_right_diagnostic: {} hostile "
                 "case(s) checked",
                 swept);
}

// HOSTILE INPUT, OVERFLOW (ESCOPO.md SS2 decision 1, "the library
// never aborts the consumer's process on hostile input"): a digit run
// far past long long's own range saturates, rather than invoking
// undefined behavior or crashing - proves decode_anb_integer()
// (anb_parse.cpp) actually takes this path, not merely that it exists.
GLINTFX_TEST(gltfx_gfss_parse_anb_saturates_on_overflow) {
    const std::string huge_positive(40, '9');
    const auto positive_result = parse_anb(huge_positive);
    GLINTFX_CHECK(positive_result.ok);
    if (positive_result.ok) {
        GLINTFX_CHECK_EQ(positive_result.value.b, std::numeric_limits<long long>::max());
    }

    const std::string huge_negative = "-" + std::string(40, '9');
    const auto negative_result = parse_anb(huge_negative);
    GLINTFX_CHECK(negative_result.ok);
    if (negative_result.ok) {
        GLINTFX_CHECK_EQ(negative_result.value.b, std::numeric_limits<long long>::lowest());
    }
}

// GFSS-VOCAB-BIND (TODO.md, GODS_LAWS.md L-40): anb_parse's own THREE
// diagnostics (anb_expression/anb_offset/end_of_anb_expression) - the
// SAME per-producer directed-production discipline gltfx_gfss_parse_
// selector_list_diagnostics_are_produced_from_the_shared_vocabulary
// above already applies to selector_parse's own rows. The static_assert
// ties this table's size to count_owned_by(anb_parse) - counted
// MECHANICALLY from diagnostic_vocabulary.hpp's own list - so an
// identifier added under this producer with no matching row here now
// FAILS TO COMPILE.
GLINTFX_TEST(gltfx_gfss_parse_anb_diagnostics_are_produced_from_the_shared_vocabulary) {
    struct diagnostic_sample {
        std::string_view source;
        std::string_view expected_identifier;
    };
    static constexpr diagnostic_sample k_anb_diagnostic_samples[] = {
        {"", k_expected_anb_expression},
        {"n-", k_expected_anb_offset},
        {"3n extra", k_expected_end_of_anb_expression},
    };
    static_assert(std::size(k_anb_diagnostic_samples) ==
                      count_owned_by(gfss_diagnostic_producer::anb_parse),
                  "GODS_LAWS.md L-40 (GFSS-VOCAB-BIND): diagnostic_vocabulary.hpp's anb_parse-"
                  "owned identifiers changed - add a directed production row to "
                  "k_anb_diagnostic_samples above, this does not compile otherwise");

    std::size_t swept = 0;
    for (const diagnostic_sample &sample : k_anb_diagnostic_samples) {
        GLINTFX_CHECK(parse_anb(sample.source).diagnostic.expected == sample.expected_identifier);
        ++swept;
    }
    GLINTFX_CHECK_EQ(swept, count_owned_by(gfss_diagnostic_producer::anb_parse));
}

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
