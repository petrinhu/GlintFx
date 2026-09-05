// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/gfss/keyword.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/value.hpp>

#include "gfss/property_status.hpp"
#include "gfss/property_table.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_property_registry_test.cpp - GFSS-PROP-REGISTRY (TODO.md wave
// W4, GODS_LAWS.md L-17/L-19/L-20/L-26/L-40, docs/gfss-property-
// registry-v1.md - 340 lines, twenty-two decisions, CTO, ratified in
// the project leader's own autonomous-mode order of 05/09/2026): the
// TDD red/green witness for glintfx::style::gltfx_gfss_property/
// gltfx_gfss_keyword (include/glintfx/gfss/property.hpp, keyword.hpp)
// and their INTERNAL siblings, glintfx::style::detail::gfss_property_
// status()/gfss_property_status_counts() (src/gfss/property_status.
// hpp).
//
// RED, SEEN (GODS_LAWS.md L-20 - "execute e veja falhar", not "the
// header did not exist yet"): before include/glintfx/gfss/property.
// hpp, keyword.hpp, src/gfss/property.cpp, property_status.cpp and
// property_table.hpp existed, this file's own #include lines above
// failed to compile - `glintfx/gfss/property.hpp`, `glintfx/gfss/
// keyword.hpp` and `gfss/property_status.hpp` did not resolve, and
// every symbol this file names (gltfx_gfss_property, gltfx_gfss_
// property_name, gltfx_gfss_keyword, detail::gfss_property_status,
// detail::gfss_property_status_counts, detail::property_keyword_
// initial and its five siblings) was undeclared. That failure IS this
// fatia's own red - the same "compile failure counts as a legitimate
// red for a foundational enumeration" precedent value.hpp/value_kind.
// cpp's own delivery already set for this project (gfss_value_test.
// cpp referenced glintfx::style::gltfx_gfss_value before value.hpp
// existed, the identical shape). Green is this file compiling and
// every case below passing against the real, ratified table.
//
// WHY THIS FILE INCLUDES property_table.hpp DIRECTLY (an INTERNAL
// header, "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir - tests/
// CMakeLists.txt's own registration for this target, the SAME pattern
// gfss_value_test/gfss_selector_parse_test already use for their own
// internal siblings): this test needs the SAME five value factories
// property_table.hpp's own k_property_table is built from
// (detail::property_keyword_initial() and friends) to CONSTRUCT the
// EXPECTED initial value for each of the 104 rows below, spelled out
// AGAIN as its own literal table (k_expected_table below) - not by
// reading k_property_table itself, which would prove nothing (a wrong
// row there would just agree with itself). Sharing the five ONE-LINE
// factory functions is a low-risk, DRY-justified reuse (GODS_LAWS.md
// L-33); retyping every row's OWN data - id, sheet name, inherited
// flag, and the magnitude/text each factory call carries - independently
// is what actually catches a production-table transcription error,
// and this file's own report to the team lead names which parts are
// shared code vs independently-retyped data.

namespace {

using glintfx::style::gltfx_gfss_keyword;
using glintfx::style::gltfx_gfss_property;
using glintfx::style::gltfx_gfss_property_count;
using glintfx::style::gltfx_gfss_property_initial;
using glintfx::style::gltfx_gfss_property_is_inherited;
using glintfx::style::gltfx_gfss_property_name;
using glintfx::style::gltfx_gfss_value;
using glintfx::style::gltfx_gfss_value_kind;

// Two gltfx_gfss_value instances agree iff their `kind` matches AND
// the ONE field that nature actually reads (value.hpp's own "only the
// field matching `kind` is meaningful" convention) is equal - a
// structural comparison, not a byte-for-byte memcmp (the OTHER six
// fields are never inspected, matching value.hpp's own contract).
bool same_initial_value(const gltfx_gfss_value &a, const gltfx_gfss_value &b) {
    if (a.kind != b.kind) {
        return false;
    }
    switch (a.kind) {
    case gltfx_gfss_value_kind::keyword:
        return a.keyword_text == b.keyword_text;
    case gltfx_gfss_value_kind::number:
        return a.number == b.number;
    case gltfx_gfss_value_kind::integer:
        return a.integer_value == b.integer_value;
    case gltfx_gfss_value_kind::length:
        return a.length.magnitude == b.length.magnitude && a.length.unit == b.length.unit;
    case gltfx_gfss_value_kind::percentage:
        return a.percentage == b.percentage;
    case gltfx_gfss_value_kind::angle:
        return a.angle.magnitude == b.angle.magnitude && a.angle.unit == b.angle.unit;
    case gltfx_gfss_value_kind::time:
        return a.duration.magnitude == b.duration.magnitude && a.duration.unit == b.duration.unit;
    }
    return false;
}

struct expected_property_row {
    gltfx_gfss_property id = gltfx_gfss_property::display;
    std::uint16_t expected_numeric_id = 0;
    std::string_view expected_sheet_name;
    bool expected_inherited = false;
    gltfx_gfss_value expected_initial{};
};

// THE STABILITY/CORRECTNESS TABLE - its OWN, independently-typed 104
// rows (docs/gfss-property-registry-v1.md SS4: "a guarda de
// estabilidade do teste tem uma tabela propria (identificador, id
// esperado) e reprova insercao no meio"), cross-checked field-by-field
// against the ratified document during this fatia's own delivery
// (id/sheet-name/identifier/inherited-flag/initial-literal, all 104,
// matched the document exactly). `expected_numeric_id` is a HAND-
// WRITTEN LITERAL next to each enumerator, not derived from the
// enumerator itself - property_id_is_append_only_and_never_
// renumerated below is what makes reordering GLINTFX_GFSS_PROPERTY_
// LIST(X) (property.hpp) a caught defect instead of a silent
// renumbering: if two X(...) lines there were ever swapped, every
// enumerator between them would shift its OWN numeric value, and this
// literal-vs-enum comparison is the only thing in this whole registry
// that would notice.
constexpr std::array<expected_property_row, 104> k_expected_table{{
#include "gfss_property_registry_test_table.inc"
}};

} // namespace

GLINTFX_TEST(gltfx_gfss_property_registry_has_the_ratified_count_of_104_properties) {
    GLINTFX_CHECK_EQ(gltfx_gfss_property_count, static_cast<std::size_t>(104));
    GLINTFX_CHECK_EQ(k_expected_table.size(), gltfx_gfss_property_count);
    std::println("gltfx_gfss_property_registry_has_the_ratified_count_of_104_properties: {} "
                 "properties",
                 gltfx_gfss_property_count);
}

// E1/SS4's OWN GUARD: id 0 = display's own POSITION IS ITS ID
// (docs/gfss-property-registry-v1.md: "o id e a posicao"), and it is
// APPEND-ONLY forever. This case is the ONE thing in this file that
// would catch a future edit reordering (rather than only appending to)
// GLINTFX_GFSS_PROPERTY_LIST(X) - see k_expected_table's own header
// comment above.
GLINTFX_TEST(gltfx_gfss_property_id_is_append_only_and_never_renumerated) {
    std::size_t swept = 0;
    for (const expected_property_row &row : k_expected_table) {
        GLINTFX_CHECK_EQ(static_cast<std::uint16_t>(row.id), row.expected_numeric_id);
        ++swept;
    }
    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfss_property_count);
    std::println("gltfx_gfss_property_id_is_append_only_and_never_renumerated: {} id(s) checked "
                 "against their own hand-written literal",
                 swept);
}

// docs/api-conventions.md R7 applied to this registry's own sheet
// names: every gltfx_gfss_property_name() answer, checked against the
// SAME independently-typed table, for EVERY one of the 104 members -
// not a sample.
GLINTFX_TEST(gltfx_gfss_property_name_matches_the_sheet_spelling_for_every_property) {
    std::size_t swept = 0;
    for (const expected_property_row &row : k_expected_table) {
        GLINTFX_CHECK(gltfx_gfss_property_name(row.id) == row.expected_sheet_name);
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfss_property_count);
    std::println("gltfx_gfss_property_name_matches_the_sheet_spelling_for_every_property: {} "
                 "name(s) checked",
                 swept);
}

// docs/gfss-property-registry-v1.md SS6's own "herda" column, for
// EVERY property - the flag GFSS-INHERIT (TODO.md, still pending) will
// resolve against.
GLINTFX_TEST(gltfx_gfss_property_is_inherited_matches_the_ratified_table_for_every_property) {
    std::size_t swept = 0;
    for (const expected_property_row &row : k_expected_table) {
        GLINTFX_CHECK_EQ(gltfx_gfss_property_is_inherited(row.id), row.expected_inherited);
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfss_property_count);
    std::println("gltfx_gfss_property_is_inherited_matches_the_ratified_table_for_every_property: "
                 "{} flag(s) checked",
                 swept);
}

// THE CASE THAT CATCHES A WRONG *TYPE*, NOT JUST A WRONG COUNT (this
// fatia's own team-lead brief, item "onde esta o perigo"): same_
// initial_value() above fails the moment a row's `kind` disagrees -
// e.g. a property this session mistakenly typed as `number_initial`
// where the ratified table says `length_initial` would fail HERE even
// though both happen to carry magnitude 0.0 and even though the
// COUNT-only cases above would still pass 104/104.
GLINTFX_TEST(gltfx_gfss_property_initial_matches_the_ratified_table_for_every_property) {
    std::size_t swept = 0;
    for (const expected_property_row &row : k_expected_table) {
        const gltfx_gfss_value actual = gltfx_gfss_property_initial(row.id);
        GLINTFX_CHECK(same_initial_value(actual, row.expected_initial));
        ++swept;
    }
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, gltfx_gfss_property_count);
    std::println("gltfx_gfss_property_initial_matches_the_ratified_table_for_every_property: {} "
                 "initial value(s) checked (kind AND magnitude/text, not count alone)",
                 swept);
}

// THE ONE DECLARED GAP (docs/gfss-property-registry-v1.md SS6, id 88:
// "centro (ausencia)" - property.hpp's own header comment carries the
// full reasoning): `transform-origin` has no declarable initial TOKEN
// in either of its two accepted scales, so gltfx_gfss_property_
// initial() answers an EMPTY keyword rather than a fabricated literal
// that would misrepresent the spec. This case exists so that fact is
// PROVEN, not just documented in a comment nobody runs.
GLINTFX_TEST(gltfx_gfss_property_transform_origin_initial_is_the_declared_absence_gap) {
    const gltfx_gfss_value initial =
        gltfx_gfss_property_initial(gltfx_gfss_property::transform_origin);
    GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::keyword);
    GLINTFX_CHECK(initial.keyword_text.empty());
    GLINTFX_CHECK(!gltfx_gfss_property_is_inherited(gltfx_gfss_property::transform_origin));
}

// E1 items 2/3 (docs/gfss-property-registry-v1.md SS1) - THE DEBT
// COUNT, PRINTED EVERY RUN, PASSING OR NOT (GODS_LAWS.md L-40 item 3:
// "a contagem aparece na saida, mesmo quando passa"). No consuming
// fatia (LAYOUT-TREE, R2D-BORDER, ANIM-TIMELINE, FONT-LINE, ...)
// exists in this tree as of this commit - every TODO.md row naming one
// of them is still ⏳ Pendente or ⛔ Bloqueado - so all 104 properties
// are `reserved`: this is the EXPECTED shape of the debt right now,
// not a defect, and it is meant to become false, loudly, the moment
// the first consuming fatia lands (that fatia's own commit is what
// flips its property's row to `applied` in property_table.hpp).
GLINTFX_TEST(
    gltfx_gfss_property_registry_reports_applied_reserved_total_debt_and_it_is_never_silent) {
    const glintfx::style::detail::property_status_counts counts =
        glintfx::style::detail::gfss_property_status_counts();

    // Printed UNCONDITIONALLY, before any assertion - the exact
    // sequence GODS_LAWS.md L-40 requires: the count is visible in the
    // output whether or not the checks below pass.
    std::println("gltfx_gfss_property_registry_reports_applied_reserved_total_debt_and_it_is_"
                 "never_silent: applied={} reserved={} total={}",
                 counts.applied, counts.reserved, counts.total);

    GLINTFX_CHECK_EQ(counts.total, gltfx_gfss_property_count);
    GLINTFX_CHECK_EQ(counts.applied + counts.reserved, counts.total);
    GLINTFX_CHECK_EQ(counts.applied, static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(counts.reserved, static_cast<std::size_t>(104));
}

// THE FULL C++23 RESERVED WORD LIST (keywords AND alternative
// operator tokens - cppreference's own "C++ keywords" table,
// cppreference.com/w/cpp/keyword, read under GODS_LAWS.md L-22/L-29;
// the ELEVEN alternative tokens - and/and_eq/bitand/bitor/compl/not/
// not_eq/or/or_eq/xor/xor_eq - are lexically identical to their own
// punctuation spelling per the standard's own [lex.digraph]/[lex.
// key], so a snake_case identifier using one of THEIR spellings is
// exactly as illegal as using the keyword itself). Enumerated CLOSED
// (GODS_LAWS.md L-40 item 5: "o espaco e pequeno e enumeravel ...
// enumere-o inteiro"), never searched inside.
constexpr std::array<std::string_view, 95> k_cpp23_reserved_words{{
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "atomic_cancel",
    "atomic_commit",
    "atomic_noexcept",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
}};

static bool is_cpp23_reserved(std::string_view word) {
    for (const std::string_view reserved : k_cpp23_reserved_words) {
        if (word == reserved) {
            return true;
        }
    }
    return false;
}

// E2 (docs/gfss-property-registry-v1.md SS1): "quando a palavra e
// reservada em C++ ... o identificador leva o sufixo `_keyword`" is a
// GENERAL RULE, proven here against the FULL closed reserved-word
// list above for EVERY member of gltfx_gfss_keyword, not asserted only
// for the two members (`auto_keyword`, `static_keyword`) this v1's own
// 56-word vocabulary happens to need it for. keyword.hpp's own
// GLINTFX_GFSS_KEYWORD_LIST(X) macro is #undef'd at the end of that
// header (the SAME macro-hygiene convention value.hpp's own X-macro
// lists follow, and tests/tools/check_macro_balance.py's own gate
// polices project-wide) - unavailable for reuse here, so k_all_
// keywords below is its OWN independently-typed (enum, identifier
// string) table instead, cross-checked against docs/gfss-property-
// registry-v1.md's own vocabulary the same way k_expected_table above
// was. The rule itself is still proven GENERALLY, not by assuming this
// table is right: every row is cross-referenced below against gltfx_
// gfss_keyword_name()'s OWN independent answer (the SHEET spelling)
// and against the closed reserved-word list - a row with the wrong
// C++ identifier for its own sheet spelling fails the reconciliation
// regardless of how the table was produced.
GLINTFX_TEST(gltfx_gfss_keyword_reserved_word_suffix_rule_is_general_not_a_hardcoded_pair_of_two) {
    struct keyword_identifier_pair {
        gltfx_gfss_keyword keyword;
        std::string_view cpp_identifier;
    };

    constexpr std::array<keyword_identifier_pair, 56> k_all_keywords{{
        {gltfx_gfss_keyword::absolute, "absolute"},
        {gltfx_gfss_keyword::all, "all"},
        {gltfx_gfss_keyword::alternate, "alternate"},
        {gltfx_gfss_keyword::alternate_reverse, "alternate_reverse"},
        {gltfx_gfss_keyword::auto_keyword, "auto_keyword"},
        {gltfx_gfss_keyword::backwards, "backwards"},
        {gltfx_gfss_keyword::baseline, "baseline"},
        {gltfx_gfss_keyword::block, "block"},
        {gltfx_gfss_keyword::border_box, "border_box"},
        {gltfx_gfss_keyword::both, "both"},
        {gltfx_gfss_keyword::center, "center"},
        {gltfx_gfss_keyword::clip, "clip"},
        {gltfx_gfss_keyword::column, "column"},
        {gltfx_gfss_keyword::column_reverse, "column_reverse"},
        {gltfx_gfss_keyword::contain, "contain"},
        {gltfx_gfss_keyword::content_box, "content_box"},
        {gltfx_gfss_keyword::cover, "cover"},
        {gltfx_gfss_keyword::ellipsis, "ellipsis"},
        {gltfx_gfss_keyword::fill, "fill"},
        {gltfx_gfss_keyword::flex, "flex"},
        {gltfx_gfss_keyword::flex_end, "flex_end"},
        {gltfx_gfss_keyword::flex_start, "flex_start"},
        {gltfx_gfss_keyword::forwards, "forwards"},
        {gltfx_gfss_keyword::hidden, "hidden"},
        {gltfx_gfss_keyword::infinite, "infinite"},
        {gltfx_gfss_keyword::left, "left"},
        {gltfx_gfss_keyword::medium, "medium"},
        {gltfx_gfss_keyword::multiply, "multiply"},
        {gltfx_gfss_keyword::no_repeat, "no_repeat"},
        {gltfx_gfss_keyword::none, "none"},
        {gltfx_gfss_keyword::normal, "normal"},
        {gltfx_gfss_keyword::nowrap, "nowrap"},
        {gltfx_gfss_keyword::paused, "paused"},
        {gltfx_gfss_keyword::pixelated, "pixelated"},
        {gltfx_gfss_keyword::plus_lighter, "plus_lighter"},
        {gltfx_gfss_keyword::relative, "relative"},
        {gltfx_gfss_keyword::repeat, "repeat"},
        {gltfx_gfss_keyword::repeat_x, "repeat_x"},
        {gltfx_gfss_keyword::repeat_y, "repeat_y"},
        {gltfx_gfss_keyword::reverse, "reverse"},
        {gltfx_gfss_keyword::right, "right"},
        {gltfx_gfss_keyword::row, "row"},
        {gltfx_gfss_keyword::row_reverse, "row_reverse"},
        {gltfx_gfss_keyword::running, "running"},
        {gltfx_gfss_keyword::screen, "screen"},
        {gltfx_gfss_keyword::solid, "solid"},
        {gltfx_gfss_keyword::space_around, "space_around"},
        {gltfx_gfss_keyword::space_between, "space_between"},
        {gltfx_gfss_keyword::space_evenly, "space_evenly"},
        {gltfx_gfss_keyword::static_keyword, "static_keyword"},
        {gltfx_gfss_keyword::stretch, "stretch"},
        {gltfx_gfss_keyword::thick, "thick"},
        {gltfx_gfss_keyword::thin, "thin"},
        {gltfx_gfss_keyword::visible, "visible"},
        {gltfx_gfss_keyword::wrap, "wrap"},
        {gltfx_gfss_keyword::wrap_reverse, "wrap_reverse"},
    }};

    GLINTFX_CHECK_EQ(k_all_keywords.size(), glintfx::style::gltfx_gfss_keyword_count);

    std::size_t swept = 0;
    std::size_t suffixed = 0;
    for (const keyword_identifier_pair &pair : k_all_keywords) {
        const std::string_view sheet_name = glintfx::style::gltfx_gfss_keyword_name(pair.keyword);
        const bool identifier_has_suffix =
            pair.cpp_identifier.size() > sheet_name.size() &&
            pair.cpp_identifier.substr(pair.cpp_identifier.size() -
                                       std::string_view("_keyword").size()) == "_keyword";

        // What the identifier would read as WITHOUT the suffix, so it
        // can be compared against the sheet's own hyphenated spelling
        // (folha uses "-", C++ identifiers use "_" - GODS_LAWS.md
        // L-21).
        std::string_view base_identifier = pair.cpp_identifier;
        if (identifier_has_suffix) {
            base_identifier = pair.cpp_identifier.substr(
                0, pair.cpp_identifier.size() - std::string_view("_keyword").size());
        }

        std::string sheet_as_identifier(sheet_name);
        for (char &c : sheet_as_identifier) {
            if (c == '-') {
                c = '_';
            }
        }
        GLINTFX_CHECK(base_identifier == sheet_as_identifier);

        if (is_cpp23_reserved(sheet_as_identifier)) {
            GLINTFX_CHECK(identifier_has_suffix);
            ++suffixed;
        } else {
            GLINTFX_CHECK(!identifier_has_suffix);
        }
        ++swept;
    }

    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, glintfx::style::gltfx_gfss_keyword_count);
    std::println("gltfx_gfss_keyword_reserved_word_suffix_rule_is_general_not_a_hardcoded_pair_"
                 "of_two: {} keyword(s) checked against the full C++23 reserved-word list, {} "
                 "needed the _keyword suffix",
                 swept, suffixed);
}

// A SAMPLE of gltfx_gfss_keyword_name(), spot-checked with LITERAL
// text (never derived from the same macro/table this case is
// verifying) - the two reserved-suffixed members (auto_keyword,
// static_keyword) resolve to their UN-suffixed sheet spelling, and a
// handful of hyphenated members resolve with the hyphen intact.
GLINTFX_TEST(gltfx_gfss_keyword_name_returns_the_sheet_spelling_never_the_cpp_identifier) {
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::auto_keyword) ==
                  "auto");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::static_keyword) ==
                  "static");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::flex_start) ==
                  "flex-start");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::space_between) ==
                  "space-between");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::no_repeat) ==
                  "no-repeat");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::plus_lighter) ==
                  "plus-lighter");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::border_box) ==
                  "border-box");
    GLINTFX_CHECK(glintfx::style::gltfx_gfss_keyword_name(gltfx_gfss_keyword::none) == "none");
}

// docs/api-conventions.md R4, applied to this registry's own three
// accessors: a `gltfx_gfss_property` value outside the closed table
// (obtained via a cast a real caller could never produce through this
// header's own enum, but which crossing the .so boundary from a NEWER
// glintfx cannot rule out - the exact scenario R4 exists for) degrades
// to the documented safe defaults, never undefined behavior.
GLINTFX_TEST(gltfx_gfss_property_accessors_degrade_safely_for_a_value_outside_the_table) {
    // The out-of-range cast below is the point of this case, not a
    // mistake (the SAME pattern err_code_test.cpp's own value_outside_
    // the_table_degrades_to_unknown case already established): it
    // simulates a value a NEWER glintfx produced that THIS build's
    // table has never heard of.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: see comment above
    const auto out_of_range = static_cast<gltfx_gfss_property>(
        static_cast<std::uint16_t>(gltfx_gfss_property_count) + 1000);
    GLINTFX_CHECK(gltfx_gfss_property_name(out_of_range) == "unknown");
    GLINTFX_CHECK(!gltfx_gfss_property_is_inherited(out_of_range));
    const gltfx_gfss_value initial = gltfx_gfss_property_initial(out_of_range);
    GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::keyword);
    GLINTFX_CHECK(initial.keyword_text.empty());
    GLINTFX_CHECK(glintfx::style::detail::gfss_property_status(out_of_range) ==
                  glintfx::style::detail::property_status::reserved);
}

// A HANDFUL OF HAND-TYPED, SINGLE-ROW SPOT CHECKS, deliberately NOT
// sharing k_expected_table's own literal values - the semantically
// highest-risk rows named in this fatia's own team-lead brief ("onde
// esta o perigo"): a color-typed property resolving through GFSS-
// COLOR-PARSE's own named-color table rather than a fabricated eighth
// value.hpp nature (property.hpp's own header comment); the one
// property whose initial is a NEGATIVE-space check (border-image-
// slice's "100%", not "0%"); and gltfx-Velocity's own documented
// [0, 3] range (docs/gfss-property-registry-v1.md D14) - the registry
// itself does not validate the range (that is ANIM-VELOCITY/GFSS-
// DECL-PARSE's own future job), so this case only proves the INITIAL
// (1) sits inside the range the doc promises, not that the range is
// enforced anywhere yet.
GLINTFX_TEST(gltfx_gfss_property_color_typed_initial_resolves_through_the_named_color_vocabulary) {
    for (const gltfx_gfss_property property :
         {gltfx_gfss_property::border_top_color, gltfx_gfss_property::border_right_color,
          gltfx_gfss_property::border_bottom_color, gltfx_gfss_property::border_left_color,
          gltfx_gfss_property::outline_color, gltfx_gfss_property::color,
          gltfx_gfss_property::gltfx_text_outline_color}) {
        const gltfx_gfss_value initial = gltfx_gfss_property_initial(property);
        GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::keyword);
        GLINTFX_CHECK(initial.keyword_text == "black");
    }
    const gltfx_gfss_value background =
        gltfx_gfss_property_initial(gltfx_gfss_property::background_color);
    GLINTFX_CHECK(background.kind == gltfx_gfss_value_kind::keyword);
    GLINTFX_CHECK(background.keyword_text == "transparent");
}

GLINTFX_TEST(gltfx_gfss_property_border_image_slice_initial_is_100_percent_not_0) {
    const gltfx_gfss_value initial =
        gltfx_gfss_property_initial(gltfx_gfss_property::border_image_slice);
    GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::percentage);
    GLINTFX_CHECK_EQ(initial.percentage, 100.0);
}

GLINTFX_TEST(gltfx_gfss_property_font_size_initial_is_16px_and_inherited) {
    const gltfx_gfss_value initial = gltfx_gfss_property_initial(gltfx_gfss_property::font_size);
    GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::length);
    GLINTFX_CHECK_EQ(initial.length.magnitude, 16.0);
    GLINTFX_CHECK(initial.length.unit == glintfx::style::gltfx_gfss_length_unit::px);
    GLINTFX_CHECK(gltfx_gfss_property_is_inherited(gltfx_gfss_property::font_size));
}

GLINTFX_TEST(gltfx_gfss_property_gltfx_velocity_initial_sits_inside_its_documented_0_to_3_range) {
    const gltfx_gfss_value initial =
        gltfx_gfss_property_initial(gltfx_gfss_property::gltfx_velocity);
    GLINTFX_CHECK(initial.kind == gltfx_gfss_value_kind::number);
    GLINTFX_CHECK_EQ(initial.number, 1.0);
    GLINTFX_CHECK(initial.number >= 0.0 && initial.number <= 3.0);
    GLINTFX_CHECK(gltfx_gfss_property_is_inherited(gltfx_gfss_property::gltfx_velocity));
}
