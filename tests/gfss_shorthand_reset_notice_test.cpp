// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

#include "gfss/declaration_ast.hpp"
#include "gfss/declaration_list_parse.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/property_table.hpp"
#include "gfss/shorthand_longhand_table.hpp"
#include "gfss/shorthand_reset_notice.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_shorthand_reset_notice_test.cpp - GFSS-SHORTHAND, S-3b (TODO.md
// wave W6, GODS_LAWS.md L-20/L-40, docs/plano-w6-folha-de-estilo.md
// S-3b/D-W6-14, decisao do lider D4, 15/09/2026): the TDD red/green
// witness for the "reset acidental" warning - a shorthand SILENTLY
// erasing a longhand the SAME block already wrote, explicitly, before
// it (dossie desta onda, SS1 item 3.1, a dor numero um dos atalhos).
// THE BEHAVIOR DOES NOT CHANGE (D-W6-4/D-W6-3, tests/gfss_shorthand_
// expand_test.cpp's own 66-cell matrix stays green, untouched by this
// file): a shorthand still wins by POSITION and an omitted longhand
// still resets to the registry's own initial value. This file only
// proves the NEW, ADDITIVE fact - declaration_list_parse_result::
// notices (D-W6-13) - never re-proves S-3's own invariants.

namespace {

using namespace glintfx::style;
using namespace glintfx::style::detail;

[[nodiscard]] declaration_list_parse_result parse(std::string_view source) {
    return parse_declaration_list(gltfx_gfss_cursor{.source = source});
}

} // namespace

// === the single case the fatia's own plan names as its vermelho de
// estreia (docs/plano-w6-folha-de-estilo.md S-3b) ========================

GLINTFX_TEST(shorthand_notices_the_longhand_it_silently_resets) {
    const declaration_list_parse_result result = parse("margin-top: 8px; margin: 0px;");
    GLINTFX_CHECK_EQ(result.notices.size(), 1u);
    GLINTFX_CHECK_EQ(result.notice_count, result.notices.size());
    GLINTFX_CHECK(result.notices[0].expected ==
                  k_expected_longhand_not_overridden_by_later_shorthand);
    GLINTFX_CHECK(result.notices[0].detail == "margin-top");
}

// === S-3b's own closed matrix: 11 shorthands x {longhand written
// BEFORE (warns), written AFTER (never warns), absent (never warns)} =
// 33, contagem impressa ====================================================

GLINTFX_TEST(shorthand_reset_notice_matrix_eleven_by_position) {
    struct row {
        std::string_view name;
        std::string_view value_text;   // valid for every longhand of the family, 1-value form
        std::string_view marker_value; // valid for entry.longhands[0] alone, written directly
    };
    static constexpr std::array<row, 11> k_rows{{
        {"margin", "1px", "9px"},
        {"padding", "1px", "9px"},
        {"border-width", "1px", "9px"},
        {"border-style", "solid", "none"},
        {"border-color", "red", "blue"},
        {"border-radius", "1px", "9px"},
        {"gap", "1px", "9px"},
        {"overflow", "hidden", "visible"},
        {"border", "solid", "9px"},
        {"outline", "solid", "9px"},
        {"flex-flow", "row", "column"},
    }};

    int cells_checked = 0;
    for (const row &r : k_rows) {
        const shorthand_longhand_entry *entry = find_shorthand_longhand_entry(r.name);
        GLINTFX_CHECK(entry != nullptr);
        const property_entry *marker_property = find_property_entry(entry->longhands[0]);
        GLINTFX_CHECK(marker_property != nullptr);

        // longhand escrito ANTES do atalho - avisa (D-W6-14)
        {
            const std::string source = std::string(marker_property->sheet_name) + ": " +
                                       std::string(r.marker_value) + "; " + std::string(r.name) +
                                       ": " + std::string(r.value_text) + ";";
            const declaration_list_parse_result result = parse(source);
            GLINTFX_CHECK_EQ(result.notices.size(), 1u);
            GLINTFX_CHECK(result.notices[0].expected ==
                          k_expected_longhand_not_overridden_by_later_shorthand);
            GLINTFX_CHECK(result.notices[0].detail == marker_property->sheet_name);
            ++cells_checked;
        }

        // longhand escrito DEPOIS do atalho - "ultima vence" e o
        // desenho, nunca acidente (D-W6-14): nunca avisa
        {
            const std::string source = std::string(r.name) + ": " + std::string(r.value_text) +
                                       "; " + std::string(marker_property->sheet_name) + ": " +
                                       std::string(r.marker_value) + ";";
            const declaration_list_parse_result result = parse(source);
            GLINTFX_CHECK(result.notices.empty());
            ++cells_checked;
        }

        // atalho sozinho, sem longhand explicito no bloco - nunca avisa
        {
            const std::string source = std::string(r.name) + ": " + std::string(r.value_text) + ";";
            const declaration_list_parse_result result = parse(source);
            GLINTFX_CHECK(result.notices.empty());
            ++cells_checked;
        }
    }

    std::println("shorthand_reset_notice_matrix_eleven_by_position: {} cell(s) checked "
                 "(expected 33)",
                 cells_checked);
    GLINTFX_CHECK_EQ(cells_checked, 33);
}

// === `border` com os 12 longhands escritos antes = 12 avisos, um por
// longhand, na mesma ordem de shorthand_longhand_table.hpp's own entry
// (contado, D-W6-14's own "border com os 12 longhands antes = 12
// avisos") =================================================================

GLINTFX_TEST(border_with_all_twelve_longhands_written_before_warns_twelve_times) {
    const shorthand_longhand_entry *entry = find_shorthand_longhand_entry("border");
    GLINTFX_CHECK(entry != nullptr);
    GLINTFX_CHECK_EQ(entry->longhand_count, 12u);

    std::string source;
    for (std::uint8_t i = 0; i < entry->longhand_count; ++i) {
        const property_entry *longhand_entry = find_property_entry(entry->longhands[i]);
        GLINTFX_CHECK(longhand_entry != nullptr);
        // groups of four, F8's own order: width, then style, then color
        // (shorthand_longhand_table.hpp's own header comment)
        const std::string_view value_text = i < 4 ? "1px" : (i < 8 ? "solid" : "red");
        source += std::string(longhand_entry->sheet_name) + ": " + std::string(value_text) + "; ";
    }
    source += "border: solid;";

    const declaration_list_parse_result result = parse(source);
    GLINTFX_CHECK_EQ(result.notices.size(), 12u);
    GLINTFX_CHECK_EQ(result.notice_count, 12u);
    for (std::uint8_t i = 0; i < entry->longhand_count; ++i) {
        const property_entry *longhand_entry = find_property_entry(entry->longhands[i]);
        GLINTFX_CHECK(result.notices[i].detail == longhand_entry->sheet_name);
        GLINTFX_CHECK(result.notices[i].expected ==
                      k_expected_longhand_not_overridden_by_later_shorthand);
    }
    std::println("border_with_all_twelve_longhands_written_before_warns_twelve_times: {} "
                 "notice(s) (expected 12)",
                 result.notices.size());
}

// === um aviso POR longhand anulado, nunca um por atalho (D-W6-14's own
// mutation obrigatoria (ii), "one_notice_per_overridden_longhand") ======

GLINTFX_TEST(one_notice_per_overridden_longhand) {
    const declaration_list_parse_result result =
        parse("margin-top: 1px; margin-left: 2px; margin: 0px;");
    GLINTFX_CHECK_EQ(result.notices.size(), 2u);
    bool saw_top = false;
    bool saw_left = false;
    for (const gltfx_gfss_diagnostic &notice : result.notices) {
        GLINTFX_CHECK(notice.expected == k_expected_longhand_not_overridden_by_later_shorthand);
        if (notice.detail == "margin-top") {
            saw_top = true;
        }
        if (notice.detail == "margin-left") {
            saw_left = true;
        }
    }
    GLINTFX_CHECK(saw_top);
    GLINTFX_CHECK(saw_left);
}

// === o longhand escrito DEPOIS do atalho nunca e um reset acidental -
// "ultima vence" e o desenho da cascata (W10), nunca acidente (D-W6-14's
// own mutation obrigatoria (i), "longhand_after_the_shorthand_is_not_a_
// reset") ==================================================================

GLINTFX_TEST(longhand_after_the_shorthand_is_not_a_reset) {
    const declaration_list_parse_result result = parse("margin: 0px; margin-top: 8px;");
    GLINTFX_CHECK(result.notices.empty());
    GLINTFX_CHECK_EQ(result.notice_count, 0u);
}

// === the SAME fact, proved directly against the ATOM (shorthand_
// reset_notice.hpp), never through the full pipeline: fold_into_
// result() (declaration_list_parse.cpp) calls this atom RIGHT AFTER a
// shortcut's own expansion, before any LATER span of the block is even
// parsed - so a declarations vector handed to shorthand_reset_notices()
// can never, by construction, already hold a "later" entry when called
// from the real pipeline, and the end-to-end test above never actually
// exercises the atom's own `earlier < expansion_begin` boundary against
// a same-property entry sitting AFTER `expansion_begin +
// longhand_count`. Built by hand here so that boundary is exercised for
// real, isolated from the pipeline's own incremental call order.
GLINTFX_TEST(the_atom_itself_never_scans_past_its_own_expansion_range) {
    std::vector<gfss_declaration> declarations;
    gfss_declaration expanded_margin_top;
    expanded_margin_top.property = gltfx_gfss_property::margin_top;
    declarations.push_back(expanded_margin_top); // the shortcut's own longhand, index 0
    gfss_declaration later_margin_top;
    later_margin_top.property = gltfx_gfss_property::margin_top;
    declarations.push_back(later_margin_top); // written AFTER the shortcut, index 1

    const std::vector<gltfx_gfss_diagnostic> notices =
        shorthand_reset_notices(declarations, /*expansion_begin=*/0, /*longhand_count=*/1,
                                /*shorthand_line=*/1, /*shorthand_column=*/1);
    GLINTFX_CHECK(notices.empty());
}

// === `!important` no longhand anterior TAMBEM avisa - o valor some do
// bloco do mesmo jeito; quem decide entre `!important` e normal e a
// cascata (W10), nao esta fatia (D-W6-14's own row in docs/plano-w6-
// folha-de-estilo.md SS4) ===================================================

GLINTFX_TEST(important_on_the_earlier_longhand_still_warns) {
    const declaration_list_parse_result result = parse("margin-top: 1px !important; margin: 0px;");
    GLINTFX_CHECK_EQ(result.notices.size(), 1u);
    GLINTFX_CHECK(result.notices[0].detail == "margin-top");
}

// === o valor nao muda: a expansao continua identica com e sem o aviso -
// mutar o valor reprova a matriz de S-3 (tests/gfss_shorthand_expand_
// test.cpp's own shorthand_expansion_matrix_eleven_by_important_by_
// position), que esta fatia nunca toca. Aqui, uma prova direta: o
// longhand que RECEBE o aviso carrega o valor do ATALHO (0), nunca o
// valor anterior (8px) que o aviso apenas denuncia ter sido apagado =====

GLINTFX_TEST(the_notice_never_changes_the_resulting_value) {
    const declaration_list_parse_result result = parse("margin-top: 8px; margin: 0px;");
    GLINTFX_CHECK_EQ(result.notices.size(), 1u);

    // the notice never DELETES nor MUTATES the earlier, explicit
    // declaration (D-W6-14's own "o comportamento nao muda") - BOTH
    // survive in the list, in source order: the author's own 8px,
    // untouched, then the shorthand's own 0 (D-W6-3's "ultima vence" by
    // POSITION, never by this atom rewriting or dropping the earlier
    // one).
    std::vector<double> margin_top_magnitudes;
    for (const gfss_declaration &declaration : result.declarations) {
        if (declaration.property == gltfx_gfss_property::margin_top) {
            GLINTFX_CHECK(declaration.form == gfss_declaration_value_form::values);
            GLINTFX_CHECK_EQ(declaration.values.size(), 1u);
            GLINTFX_CHECK(declaration.values[0].kind == gltfx_gfss_value_kind::length);
            margin_top_magnitudes.push_back(declaration.values[0].length.magnitude);
        }
    }
    GLINTFX_CHECK_EQ(margin_top_magnitudes.size(), 2u);
    GLINTFX_CHECK_EQ(margin_top_magnitudes[0], 8.0);
    GLINTFX_CHECK_EQ(margin_top_magnitudes[1], 0.0);
}

// === texto inline recebe o MESMO aviso (D-W6-13: a lista nasce no
// resultado do BLOCO, nao da folha - parse_declaration_list e a MESMA
// funcao tests/gfss_declaration_parse_test.cpp's own inline_style_text_
// parses_with_the_same_function ja usa para o caminho inline; nenhum
// leitor de folha esta envolvido aqui) =====================================

GLINTFX_TEST(inline_style_text_also_receives_the_reset_notice) {
    const declaration_list_parse_result result = parse("color: red; margin-top: 8px; margin: 0px");
    // color + the ORIGINAL margin-top (8px, never deleted, D-W6-3) + the
    // 4 longhands the shorthand's own expansion produced
    GLINTFX_CHECK_EQ(result.declarations.size(), 6u);
    GLINTFX_CHECK_EQ(result.notices.size(), 1u);
    GLINTFX_CHECK(result.notices[0].detail == "margin-top");
}
