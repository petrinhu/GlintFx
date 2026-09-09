// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/gfss/keyword.hpp>
#include <glintfx/gfss/property.hpp>

#include "gfss/property_value_contract.hpp"
#include "gfss/refused_property_names.hpp"
#include "gfss/shorthand_name_vocabulary.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_declaration_registry_doc_oracle_test.cpp - GFSS-DECL-PARSE
// review remediation (TODO.md wave W5, GODS_LAWS.md L-17/L-20/L-27/
// L-36/L-40, achado CRITICO da revisao adversarial de 9266b31): the
// THIRD role's own witness that k_property_value_contracts (104
// rows), k_shorthand_names (11) and k_refused_property_names's own
// `detail` field (10) each say what docs/gfss-property-registry-v1.md
// §6 (and, for six refused shorthands the registry's own sheet-name
// prefixes cannot mechanically resolve, real CSS spec fact) says -
// NEVER what the SAME production table already says about itself.
//
// THE DEFECT THIS FILE EXISTS TO CLOSE, PROVED BY MUTATION AGAINST
// 9266b31 (achado CRITICO): gfss_declaration_parse_test.cpp's own
// eleven_accepted_shorthands_and_ten_refused_names_are_closed_lists
// and every_property_has_exactly_one_value_contract_and_categories_
// are_counted compare the PRODUCTION table against ITSELF - swapping
// `outline` for a name that does not even exist in the registry
// (shorthand_name_vocabulary.hpp), halving `transition`'s own
// `detail` from four names to two (refused_property_names.hpp), or
// swapping one accepted word of `justify-content` for another of the
// SAME cardinality (property_value_contract.hpp) all left that
// existing suite 21/21 green. A test that reads its own expected value
// from the code under test proves only that the code agrees with
// itself.
//
// WHY THE DOC IS READ AT TEST TIME, NEVER RETYPED INTO A SECOND C++
// TABLE HERE (project leader's own order relayed through the team
// lead, GODS_LAWS.md L-27): gfss_property_registry_test.cpp's own
// k_expected_table precedent (retype 104 rows independently, compare
// against k_property_table) does not apply here - THAT table's five
// columns are short, uncorrelated literals; THIS registry's own §6
// "tipo" column is exactly the same free text an agent reading the
// SAME doc would transcribe into a SECOND property_value_contract.hpp,
// carrying the same transcription-error risk as the ORIGINAL, not an
// independent one. Reading docs/gfss-property-registry-v1.md itself,
// the same file property_value_contract.hpp's own header comment
// already names as its extraction source, is the only oracle that
// cannot silently agree with a mutated production table. The doc's
// absolute path arrives as GLTFX_GFSS_REGISTRY_DOC_SOURCE (tests/
// CMakeLists.txt), the SAME "${PROJECT_SOURCE_DIR}/..." compile
// definition + std::ifstream idiom log_field_test.cpp's own read_
// header() already established in this suite for reading a REAL
// committed file's text at test run time, rather than trusting it
// from a distance.
//
// WHAT THIS ORACLE DOES NOT ATTEMPT, DECLARED RATHER THAN HIDDEN
// (GODS_LAWS.md L-27): it parses §6's own "tipo" cell into the SAME
// seven-way shape property_value_contract.hpp itself carries (raw /
// color / time-list / natures bitmask / keyword set) - not the min/max
// arity a `(1 a 4)` cell names, not the number range a `[0, 1]` cell
// names, not `initial`/`inherited`. Those remain read-only prose a
// human reviewer checks; a wrong keyword, a wrong nature, a row
// mis-typed as raw/color/time-list, or a wrong refused-shorthand
// `detail` is what this file catches mechanically.
//
// SIX OF THE TEN REFUSED-NAME `detail` VALUES ARE NOT DERIVABLE FROM
// OUR OWN DOC AT ALL (marked INFERENCE, GODS_LAWS.md L-27): `place-
// content`/`place-items`/`place-self`/`flex`/`inset`/`white-space`
// each expand to real CSS longhands whose names do not share a sheet-
// name PREFIX with the shorthand itself (unlike `transition-*`/
// `animation-*`/`background-*`/`border-image-*`, all four of which
// this file derives MECHANICALLY by sweeping §6 for that literal
// prefix) - the six below are pinned against real CSS spec fact,
// external to both this registry doc and its own implementation,
// citing the same sources refused_property_names.hpp's own header
// comment and docs/gfss-property-registry-v1.md §9 already name (CSS
// Flexible Box Layout Module Level 1 for `flex`; CSS Logical
// Properties/Position for `inset`; CSS Box Alignment Level 3,
// css-align-3, for the three `place-*`; CSS Text Level 4, css-text-4,
// for `white-space`'s kept half).

using namespace glintfx::style;
using namespace glintfx::style::detail;

namespace {

// --- reading the real, committed doc (log_field_test.cpp's own
// read_header() idiom, GODS_LAWS.md L-36: real file, not a copy) -----

[[nodiscard]] std::string read_registry_doc() {
    const std::ifstream in{std::string(GLTFX_GFSS_REGISTRY_DOC_SOURCE)};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// --- tiny, hand-rolled markdown-table reading (GODS_LAWS.md L-07: no
// <regex>, matching this project's own hand-written parser house
// style already set by tokenizer.cpp/selector_parse.cpp) ------------

[[nodiscard]] std::string trim(std::string_view s) {
    const std::size_t begin = s.find_first_not_of(" \t\r");
    if (begin == std::string_view::npos) {
        return {};
    }
    const std::size_t end = s.find_last_not_of(" \t\r");
    return std::string(s.substr(begin, end - begin + 1));
}

[[nodiscard]] bool is_ascii_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// Splits one markdown table row on unescaped `|`, folding `\|` back
// into a literal `|` inside the cell it belongs to (the doc's own
// escaping for a cell that itself lists alternatives, e.g. `row \|
// row-reverse`) - then drops the empty leading/trailing cells the
// line's own opening and closing `|` always produce.
[[nodiscard]] std::vector<std::string> split_table_row(std::string_view line) {
    std::vector<std::string> cols;
    std::string current;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|') {
            current += '|';
            ++i;
            continue;
        }
        if (line[i] == '|') {
            cols.push_back(trim(current));
            current.clear();
            continue;
        }
        current += line[i];
    }
    cols.push_back(trim(current));
    if (!cols.empty() && cols.front().empty()) {
        cols.erase(cols.begin());
    }
    if (!cols.empty() && cols.back().empty()) {
        cols.pop_back();
    }
    return cols;
}

[[nodiscard]] std::string strip_backticks(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        if (c != '`') {
            out += c;
        }
    }
    return out;
}

// Every backtick-delimited span in `text`, in order - a §6 "tipo" cell
// with several independent backtick groups (`` `auto`, comprimento, %,
// `contain \| cover` ``) yields one group per pair, never merged.
[[nodiscard]] std::vector<std::string> extract_backtick_groups(std::string_view text) {
    std::vector<std::string> groups;
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '`') {
            const std::size_t close = text.find('`', i + 1);
            if (close == std::string_view::npos) {
                break;
            }
            groups.emplace_back(text.substr(i + 1, close - i - 1));
            i = close + 1;
        } else {
            ++i;
        }
    }
    return groups;
}

// A backtick group's own alternatives - split_table_row() already
// folded the doc's own `\|` escaping back into a literal `|`, so a
// plain `|` here is always an alternative separator, never a column
// boundary.
[[nodiscard]] std::vector<std::string> split_alternatives(std::string_view group) {
    std::vector<std::string> words;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= group.size(); ++i) {
        if (i == group.size() || group[i] == '|') {
            std::string word = trim(group.substr(start, i - start));
            if (!word.empty()) {
                words.push_back(std::move(word));
            }
            start = i + 1;
        }
    }
    return words;
}

// --- §6, one row per property (id is the position, §4/§6's own rule)

struct registry_doc_row {
    int id = -1;
    std::string sheet_name; // hyphen form, e.g. "margin-top"
    std::string tipo;       // the "tipo" cell, `\|` already un-escaped
};

[[nodiscard]] bool is_registry_row(const std::vector<std::string> &cols) {
    return cols.size() == 8 && !cols[0].empty() &&
           std::ranges::all_of(cols[0], is_ascii_digit);
}

// grep -cE '^\| [0-9]+ \|' docs/gfss-property-registry-v1.md finds
// EXACTLY 104 matches in the whole file (measured, not assumed) - §6
// is the only pipe-table in the doc whose first cell is a bare
// integer, so a line-by-line sweep of the WHOLE doc, unbounded by a
// "between §6 and §7" heading search, already lands only on real
// property rows.
[[nodiscard]] std::vector<registry_doc_row> extract_registry_doc_rows(std::string_view doc_text) {
    std::vector<registry_doc_row> rows;
    std::size_t pos = 0;
    while (pos <= doc_text.size()) {
        const std::size_t newline = doc_text.find('\n', pos);
        const std::string_view line = doc_text.substr(
            pos, newline == std::string_view::npos ? std::string_view::npos : newline - pos);
        const std::vector<std::string> cols = split_table_row(line);
        if (is_registry_row(cols)) {
            registry_doc_row row;
            row.id = std::stoi(cols[0]);
            row.sheet_name = strip_backticks(cols[1]);
            row.tipo = cols[3];
            // "idem" (§6's own convention for "same as the row right
            // above" - measured: border-right/bottom/left-width, ids
            // 39-41, each point back at border-top-width's own tipo
            // cell) is not a value grammar of its own - resolve it to
            // the previous row's REAL tipo text before this row is
            // ever classified, or the three would silently read as an
            // empty, 0-nature/0-keyword ordinary row (the mechanical
            // trap this file's own top comment names for
            // transform-origin's prose, happening again here for a
            // different reason).
            if (trim(row.tipo) == "idem") {
                GLINTFX_CHECK(!rows.empty());
                row.tipo = rows.back().tipo;
            }
            rows.push_back(std::move(row));
        }
        if (newline == std::string_view::npos) {
            break;
        }
        pos = newline + 1;
    }
    return rows;
}

// --- classifying one "tipo" cell into property_value_contract.hpp's
// own shape (this file's own top comment: the seven-way split that
// file already documents, minus arity/range) -------------------------

[[nodiscard]] bool tipo_is_pure_color(std::string_view tipo) { return trim(tipo) == "cor"; }

[[nodiscard]] bool tipo_is_time_list(std::string_view tipo) {
    return strip_backticks(tipo) == "<time>#";
}

// Every raw_composite row's own "tipo" cell names a grammar this
// registry does NOT congeal (property_value_contract.hpp's own top
// comment) - each recognizable by one of four textual markers: a
// generic placeholder (`<image>`, `<position>`, `<shadow>#`, ...), an
// empty-argument function call (`inset()`, `circle()`, `ellipse()`),
// "lista de" (font-family's own prose), or transform-origin's own
// prose escape hatch ("as duas escalas") - the ONE row in the whole
// registry with no backtick and no `<...>` marker at all, named here
// rather than silently miscategorized as an ordinary 0-nature/
// 0-keyword row (which would otherwise look identical to a raw row on
// the code side).
[[nodiscard]] bool tipo_is_raw_composite(std::string_view tipo) {
    return tipo.find('<') != std::string_view::npos || tipo.find("()") != std::string_view::npos ||
           tipo.find("lista de") != std::string_view::npos ||
           tipo.find("escalas") != std::string_view::npos;
}

[[nodiscard]] std::uint8_t natures_from_tipo_text(std::string_view tipo) {
    std::uint8_t natures = 0;
    if (tipo.find("comprimento") != std::string_view::npos) {
        natures |= k_nature_length;
    }
    if (tipo.find('%') != std::string_view::npos) {
        natures |= k_nature_percentage;
    }
    if (tipo.find("número") != std::string_view::npos) {
        natures |= k_nature_number;
    }
    if (tipo.find("inteiro") != std::string_view::npos) {
        natures |= k_nature_integer;
    }
    return natures;
}

[[nodiscard]] std::vector<std::string> keywords_from_tipo_text(std::string_view tipo) {
    std::vector<std::string> words;
    for (const std::string &group : extract_backtick_groups(tipo)) {
        for (std::string &word : split_alternatives(group)) {
            words.push_back(std::move(word));
        }
    }
    std::ranges::sort(words);
    return words;
}

[[nodiscard]] std::vector<std::string> keywords_from_contract(const property_value_contract &c) {
    std::vector<std::string> words;
    for (std::uint8_t i = 0; i < c.keyword_count; ++i) {
        words.emplace_back(gltfx_gfss_keyword_name(c.keywords[i]));
    }
    std::ranges::sort(words);
    return words;
}

[[nodiscard]] std::string join(const std::vector<std::string> &words) {
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) {
            out += ' ';
        }
        out += words[i];
    }
    return out;
}

// One row's own verdict - std::nullopt means the doc and the code
// agree; anything else is the mismatch this test prints and fails on.
[[nodiscard]] std::optional<std::string> compare_row(const registry_doc_row &doc_row,
                                                      const property_value_contract &code) {
    if (tipo_is_pure_color(doc_row.tipo)) {
        if (!code.is_color) {
            return std::format("id {} ({}): doc diz \"cor\", k_property_value_contracts nao "
                                "marca is_color",
                                doc_row.id, doc_row.sheet_name);
        }
        return std::nullopt;
    }
    if (tipo_is_time_list(doc_row.tipo)) {
        if (!code.comma_separated) {
            return std::format(
                "id {} ({}): doc diz \"<time>#\", k_property_value_contracts nao e "
                "comma_separated (contract_time_list)",
                doc_row.id, doc_row.sheet_name);
        }
        return std::nullopt;
    }
    if (tipo_is_raw_composite(doc_row.tipo)) {
        if (!code.raw_composite) {
            return std::format("id {} ({}): doc indica composto cru (\"{}\"), "
                                "k_property_value_contracts nao marca raw_composite",
                                doc_row.id, doc_row.sheet_name, doc_row.tipo);
        }
        return std::nullopt;
    }

    // Ordinary row: neither color, time-list nor raw on the doc side -
    // the code side must agree on that first, before natures/keywords
    // are even comparable.
    if (code.is_color || code.raw_composite || code.comma_separated) {
        return std::format("id {} ({}): doc parece ordinaria (\"{}\"), mas "
                            "k_property_value_contracts marca is_color/raw_composite/"
                            "comma_separated",
                            doc_row.id, doc_row.sheet_name, doc_row.tipo);
    }

    const std::uint8_t expected_natures = natures_from_tipo_text(doc_row.tipo);
    if (expected_natures != code.accepted_natures) {
        return std::format(
            "id {} ({}): natureza esperada do doc {:#04x}, k_property_value_contracts tem {:#04x}",
            doc_row.id, doc_row.sheet_name, expected_natures, code.accepted_natures);
    }

    const std::vector<std::string> expected_keywords = keywords_from_tipo_text(doc_row.tipo);
    const std::vector<std::string> code_keywords = keywords_from_contract(code);
    if (expected_keywords != code_keywords) {
        return std::format(
            "id {} ({}): palavras-chave do doc [{}], k_property_value_contracts tem [{}]",
            doc_row.id, doc_row.sheet_name, join(expected_keywords), join(code_keywords));
    }
    return std::nullopt;
}

// --- "As onze abreviações" (§6's own closing paragraph) -------------

[[nodiscard]] std::vector<std::string> extract_shorthand_names_from_doc(std::string_view doc_text) {
    // The bare phrase "As onze abreviações" also appears as prose in
    // §1 (before §6 even starts) - the "### " heading marker is what
    // makes this search land on §6's own closing subsection, not that
    // earlier mention (measured: `grep -n` found two hits for the bare
    // phrase, one for the heading form). The paragraph's OWN second
    // sentence ("Vivem em `GFSS-SHORTHAND`; ...") carries an unrelated
    // backtick-quoted name of its own - measured live, an unbounded
    // read up to the next `---` divider pulled "GFSS-SHORTHAND" in as
    // a false twelfth name. Bounding at "Vivem em" keeps only the
    // sentence that actually lists the eleven names.
    const std::string_view heading = "### As onze abreviações";
    const std::size_t heading_pos = doc_text.find(heading);
    if (heading_pos == std::string_view::npos) {
        return {};
    }
    const std::string_view sentence_end_marker = "Vivem em";
    const std::size_t sentence_end = doc_text.find(sentence_end_marker, heading_pos);
    const std::string_view paragraph =
        doc_text.substr(heading_pos, sentence_end == std::string_view::npos
                                          ? std::string_view::npos
                                          : sentence_end - heading_pos);
    return extract_backtick_groups(paragraph);
}

// --- refused shorthand `detail` - four of ten are a mechanical §6
// sweep (this file's own top comment); the other six are real CSS
// spec fact, external to this project's own doc and code alike -----

[[nodiscard]] std::string
detail_by_sheet_name_prefix(const std::vector<registry_doc_row> &doc_rows, std::string_view prefix) {
    std::vector<std::string> names;
    for (const registry_doc_row &row : doc_rows) {
        if (row.sheet_name.starts_with(prefix)) {
            names.push_back(row.sheet_name);
        }
    }
    return join(names);
}

} // namespace

GLINTFX_TEST(registry_doc_extraction_finds_the_whole_closed_vocabulary_first) {
    // GODS_LAWS.md L-36/L-40: the piso de varredura nao-vazia, checked
    // BEFORE any row-by-row comparison below trusts the extraction at
    // all - a broken parser that silently returns zero rows must fail
    // loud here, never read as "104 clean rows, nothing to report".
    const std::string doc_text = read_registry_doc();
    GLINTFX_CHECK(!doc_text.empty());

    const std::vector<registry_doc_row> rows = extract_registry_doc_rows(doc_text);
    const std::vector<std::string> shorthand_names = extract_shorthand_names_from_doc(doc_text);

    std::println("registry_doc_extraction_finds_the_whole_closed_vocabulary_first: {} property "
                 "row(s), {} shorthand name(s) extracted from {}",
                 rows.size(), shorthand_names.size(), GLTFX_GFSS_REGISTRY_DOC_SOURCE);

    GLINTFX_CHECK(rows.size() == 104);
    GLINTFX_CHECK(shorthand_names.size() == 11);
}

GLINTFX_TEST(contract_table_matches_the_property_registry_doc) {
    const std::string doc_text = read_registry_doc();
    const std::vector<registry_doc_row> doc_rows = extract_registry_doc_rows(doc_text);
    GLINTFX_CHECK(doc_rows.size() == 104);

    std::vector<std::string> mismatches;
    int checked = 0;
    for (const registry_doc_row &doc_row : doc_rows) {
        GLINTFX_CHECK(doc_row.id >= 0 &&
                     static_cast<std::size_t>(doc_row.id) < k_property_value_contracts.size());
        const auto property = static_cast<gltfx_gfss_property>(doc_row.id);
        // Ties the doc's own id to the REAL registered property, not
        // just position - a transposed row would otherwise still pass
        // every natures/keyword check against the wrong id's contract.
        GLINTFX_CHECK(gltfx_gfss_property_name(property) == doc_row.sheet_name);

        const property_value_contract &code = property_value_contract_for(property);
        GLINTFX_CHECK(static_cast<int>(code.id) == doc_row.id);
        if (const std::optional<std::string> mismatch = compare_row(doc_row, code)) {
            mismatches.push_back(*mismatch);
        }
        ++checked;
    }

    for (const std::string &mismatch : mismatches) {
        std::println(stderr, "contract_table_matches_the_property_registry_doc: {}", mismatch);
    }
    std::println("contract_table_matches_the_property_registry_doc: {} row(s) checked, {} "
                 "mismatch(es)",
                 checked, mismatches.size());
    GLINTFX_CHECK(checked == 104);
    GLINTFX_CHECK(mismatches.empty());
}

GLINTFX_TEST(shorthand_names_match_the_property_registry_doc) {
    const std::string doc_text = read_registry_doc();
    const std::vector<std::string> doc_names_unsorted = extract_shorthand_names_from_doc(doc_text);
    GLINTFX_CHECK(doc_names_unsorted.size() == 11);

    std::vector<std::string> doc_names = doc_names_unsorted;
    std::ranges::sort(doc_names);

    std::vector<std::string> code_names;
    for (const std::string_view name : k_shorthand_names) {
        code_names.emplace_back(name);
    }
    std::ranges::sort(code_names);

    std::println("shorthand_names_match_the_property_registry_doc: doc=[{}] code=[{}]",
                 join(doc_names), join(code_names));
    GLINTFX_CHECK(doc_names == code_names);
}

GLINTFX_TEST(refused_property_detail_matches_the_registry_doc_and_css_spec) {
    const std::string doc_text = read_registry_doc();
    const std::vector<registry_doc_row> doc_rows = extract_registry_doc_rows(doc_text);
    GLINTFX_CHECK(doc_rows.size() == 104);
    GLINTFX_CHECK(k_refused_property_names.size() == 10);

    // Mechanically derived from §6 itself: every sheet name sharing the
    // shorthand's own hyphenated prefix, in id order - reproduces
    // EXACTLY the achado CRITICO's own demonstrated mutation (transition's
    // `detail` cut from four names to two).
    const std::vector<std::pair<std::string_view, std::string>> prefix_derived{
        {"transition", detail_by_sheet_name_prefix(doc_rows, "transition-")},
        {"animation", detail_by_sheet_name_prefix(doc_rows, "animation-")},
        {"background", detail_by_sheet_name_prefix(doc_rows, "background-")},
        {"border-image", detail_by_sheet_name_prefix(doc_rows, "border-image-")},
    };

    // NOT derivable from §6 (this file's own top comment): real CSS
    // shorthand -> longhand fact, external to this project entirely.
    // `place-items`/`place-self` name only the ONE longhand this
    // registry actually ships (refused_property_names.hpp's own "A GAP
    // THIS FILE DECLARES" paragraph) - this table pins the SAME
    // declared gap, not the full CSS mapping.
    const std::vector<std::pair<std::string_view, std::string_view>> spec_derived{
        {"place-content", "align-content justify-content"},
        {"place-items", "align-items"},
        {"place-self", "align-self"},
        {"flex", "flex-grow flex-shrink flex-basis"},
        {"inset", "top right bottom left"},
        {"white-space", "text-wrap-mode"},
    };
    GLINTFX_CHECK(prefix_derived.size() + spec_derived.size() == 10);

    std::vector<std::string> mismatches;
    int checked = 0;
    for (const refused_property_name_entry &entry : k_refused_property_names) {
        std::optional<std::string_view> expected;
        for (const auto &[name, detail] : prefix_derived) {
            if (name == entry.name) {
                expected = detail;
            }
        }
        for (const auto &[name, detail] : spec_derived) {
            if (name == entry.name) {
                expected = detail;
            }
        }
        if (!expected) {
            mismatches.push_back(
                std::format("\"{}\": nome nao esta nem na derivacao por §6 nem na tabela de CSS "
                            "externa deste oraculo (ambas somam 10; k_refused_property_names "
                            "tambem tem 10 - um nome aqui nao bate com nenhuma das duas)",
                            entry.name));
            continue;
        }
        if (*expected != entry.detail) {
            mismatches.push_back(std::format("\"{}\": esperado \"{}\", k_refused_property_names "
                                             "tem \"{}\"",
                                             entry.name, *expected, entry.detail));
        }
        ++checked;
    }

    for (const std::string &mismatch : mismatches) {
        std::println(stderr, "refused_property_detail_matches_the_registry_doc_and_css_spec: {}",
                     mismatch);
    }
    std::println("refused_property_detail_matches_the_registry_doc_and_css_spec: {} entrie(s) "
                 "checked ({} derivada(s) do §6, {} do CSS externo), {} mismatch(es)",
                 checked, prefix_derived.size(), spec_derived.size(), mismatches.size());
    GLINTFX_CHECK(checked == 10);
    GLINTFX_CHECK(mismatches.empty());
}
