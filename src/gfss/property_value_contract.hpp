// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glintfx/gfss/keyword.hpp>
#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/value.hpp>

// property_value_contract.hpp - GFSS-DECL-PARSE, DP-5/D-DP-4 (TODO.md
// wave W5, GODS_LAWS.md L-17/L-19/L-20/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-4, docs/gfss-
// property-registry-v1.md SS6): the "coluna tipo" the registry's own
// SS4 says exists on the internal table (property_table.hpp) but does
// NOT (measured: property_table.hpp's own property_entry has only
// `id, sheet_name, inherited, initial, status` - see property_table.hpp
// itself). This file is where that column is BORN, extracted
// MECHANICALLY, row by row, from docs/gfss-property-registry-v1.md's
// own SS6 "tipo" cell for every one of the 104 properties - the SAME
// method that file's own property_table.hpp already used for its five
// columns (this header's own top comment on property_table.hpp
// documents that method; this file repeats it for the one column that
// table never carried).
//
// SEVEN CATEGORIES, MECHANICALLY COUNTED FROM THIS TABLE, NOT FROM THE
// PLAN THAT OPENED THIS FATIA (GODS_LAWS.md L-27: the plan's own SS4.2
// names "color=9"; a line-by-line re-count of docs/gfss-property-
// registry-v1.md SS6 against THIS project's own 56-word keyword.hpp
// finds eight color-typed properties, not nine - `color`, the four
// `border-*-color`, `background-color`, `outline-color`,
// `gltfx-Text_Outline_Color` - see this file's own test for the swept
// count and the delivery report for the discrepancy against the plan):
// `raw_composite` (a composite grammar SS4 itself declares "does not
// congeal" here - `<shadow>`, `<transform-list>`, `<easing>`,
// `<filter-function-list>`, `<image>`, `<position>`, `<razao>`, a
// `@keyframes` name, `N / <time>`, a font-name list, `content` - kept
// as RAW tokens, D-DP-4's own "composto fica cru"), `color` (resolved
// through GFSS-COLOR-PARSE/D-DP-6, `currentColor` accepted first), and
// five ordinary shapes built from a CLOSED keyword subset plus/or a
// closed set of value.hpp's own natures (length, percentage, number,
// integer, time).
//
// A DECLARED SIMPLIFICATION FOR THE FOUR `<time>#` PROPERTIES (D-DP-4's
// own "tempo sem unidade = ms"; GODS_LAWS.md L-27, marked INFERENCE):
// SS6 spells `transition-duration`/`transition-delay`/`animation-
// duration`/`animation-delay` as `<time>#` - a COMMA-SEPARATED LIST of
// times (one duration per transitioned/animated property). This fatia's
// own declaration_value_check.cpp validates each comma-separated item
// as its own <time> (or a bare unitless number, folded to milliseconds
// - the project leader's own 28/08/2026 decision), up to an
// unbounded-in-practice count (`max_count = 255`, this file's own
// stand-in for "no arity ceiling", never the real spec's own
// "one-per-transitioned-property" cross-check, which needs the OTHER
// property's own value and is out of THIS registry's declared scope,
// SS4's own "o que NAO congela").

namespace glintfx::style::detail {

// Bitmask over value.hpp's own gltfx_gfss_value_kind - which decoded
// NATURES (never a keyword; keywords are `accepted_keywords` below) a
// property's own value may hold. `angle` is declared but unused by any
// v1 property (mechanically confirmed: no SS6 row names an <angle>) -
// kept for the SAME reason value.hpp itself keeps the nature as its own
// closed member, completeness over present use.
inline constexpr std::uint8_t k_nature_length = 1U << 0;
inline constexpr std::uint8_t k_nature_percentage = 1U << 1;
inline constexpr std::uint8_t k_nature_number = 1U << 2;
inline constexpr std::uint8_t k_nature_integer = 1U << 3;
inline constexpr std::uint8_t k_nature_angle = 1U << 4;
inline constexpr std::uint8_t k_nature_time = 1U << 5;

struct property_value_contract {
    gltfx_gfss_property id = gltfx_gfss_property::display;

    // Up to 8 accepted simple keywords (align-content/justify-content's
    // own 7-word set, docs/gfss-property-registry-v1.md SS6, is the
    // largest in the v1 registry - mechanically the ceiling this array
    // needs).
    std::array<gltfx_gfss_keyword, 8> keywords{};
    std::uint8_t keyword_count = 0;

    std::uint8_t accepted_natures =
        0; // bitmask of k_nature_*, 0 for a pure-keyword or color contract
    std::uint8_t min_count = 1;
    std::uint8_t max_count = 1;
    bool comma_separated = false;

    bool is_color = false;
    bool raw_composite = false;

    bool has_range = false;
    double range_min = 0.0;
    double range_max = 0.0;
    bool clamp_range =
        false; // true: prende ao limite (opacity); false: recusa fora da faixa (velocity)

    bool time_unitless_ms = false;
};

// --- small, reusable factories (GODS_LAWS.md L-33: DRY's "regra de 3",
// triggered by orders of magnitude here - see property_table.hpp's own
// header comment for the identical justification it gives for ITS five
// factories) --------------------------------------------------------

constexpr property_value_contract contract_color(gltfx_gfss_property id) {
    property_value_contract c{};
    c.id = id;
    c.is_color = true;
    return c;
}

constexpr property_value_contract contract_raw(gltfx_gfss_property id) {
    property_value_contract c{};
    c.id = id;
    c.raw_composite = true;
    return c;
}

// Pure keyword set, no numeric nature at all - `words` holds at most 8
// entries (this file's own top comment).
constexpr property_value_contract
contract_keywords(gltfx_gfss_property id, std::initializer_list<gltfx_gfss_keyword> words) {
    property_value_contract c{};
    c.id = id;
    for (gltfx_gfss_keyword w : words) {
        c.keywords[c.keyword_count] = w;
        ++c.keyword_count;
    }
    return c;
}

// A closed nature bitmask, plus an OPTIONAL keyword set (e.g. `auto` on
// top of length/percentage) - the shape most of the registry's own
// numeric properties take.
constexpr property_value_contract
contract_natures(gltfx_gfss_property id, std::uint8_t natures,
                 std::initializer_list<gltfx_gfss_keyword> words = {}) {
    property_value_contract c{};
    c.id = id;
    c.accepted_natures = natures;
    for (gltfx_gfss_keyword w : words) {
        c.keywords[c.keyword_count] = w;
        ++c.keyword_count;
    }
    return c;
}

// Same as contract_natures(), with an arity range (`border-image-*`'s
// own "1 a 4" cells) instead of the implicit single value.
constexpr property_value_contract
contract_natures_arity(gltfx_gfss_property id, std::uint8_t natures, std::uint8_t min_count,
                       std::uint8_t max_count,
                       std::initializer_list<gltfx_gfss_keyword> words = {}) {
    property_value_contract c = contract_natures(id, natures, words);
    c.min_count = min_count;
    c.max_count = max_count;
    return c;
}

// `<time>#` - D-DP-4's own declared simplification (this file's own top
// comment), unitless authoring folds to milliseconds.
constexpr property_value_contract contract_time_list(gltfx_gfss_property id) {
    property_value_contract c{};
    c.id = id;
    c.accepted_natures = k_nature_time | k_nature_integer |
                         k_nature_number; // a bare unitless number is legal (folds to ms)
    c.min_count = 1;
    c.max_count = 255;
    c.comma_separated = true;
    c.time_unitless_ms = true;
    return c;
}

// A number with a closed range - `clamp` true prende (opacity, CSS
// Color 4 SS3.3's own "the used value ... MUST be clamped" - the SAME
// convention color_parse.hpp's own out-of-range component clamp
// already applies), false recusa (gltfx-Velocity, D14).
constexpr property_value_contract contract_number_range(gltfx_gfss_property id, double range_min,
                                                        double range_max, bool clamp) {
    property_value_contract c{};
    c.id = id;
    c.accepted_natures = k_nature_number;
    c.has_range = true;
    c.range_min = range_min;
    c.range_max = range_max;
    c.clamp_range = clamp;
    return c;
}

// --- the master table -------------------------------------------------

// 104 rows, property.hpp's own id order - see this file's own top
// comment for the extraction method and the color-count discrepancy it
// found against the plan.
inline constexpr std::array<property_value_contract, 104> k_property_value_contracts{{
    // Group A - Caixa e fluxo (ids 0-25)
    contract_keywords(
        gltfx_gfss_property::display,
        {gltfx_gfss_keyword::block, gltfx_gfss_keyword::flex, gltfx_gfss_keyword::none}),
    contract_keywords(gltfx_gfss_property::box_sizing,
                      {gltfx_gfss_keyword::border_box, gltfx_gfss_keyword::content_box}),
    contract_natures(gltfx_gfss_property::width, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::height, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::min_width, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::min_height, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::max_width, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::none}),
    contract_natures(gltfx_gfss_property::max_height, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::none}),
    contract_raw(gltfx_gfss_property::aspect_ratio), // <razao> - D-DP-4's own raw list
    contract_natures(gltfx_gfss_property::margin_top, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::margin_right, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::margin_bottom, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::margin_left, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::padding_top, k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::padding_right, k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::padding_bottom, k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::padding_left, k_nature_length | k_nature_percentage),
    contract_keywords(gltfx_gfss_property::position,
                      {gltfx_gfss_keyword::static_keyword, gltfx_gfss_keyword::relative,
                       gltfx_gfss_keyword::absolute}),
    contract_natures(gltfx_gfss_property::top, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::right, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::bottom, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::left, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_natures(gltfx_gfss_property::z_index, k_nature_integer,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_keywords(gltfx_gfss_property::visibility,
                      {gltfx_gfss_keyword::visible, gltfx_gfss_keyword::hidden}),
    contract_keywords(gltfx_gfss_property::overflow_x,
                      {gltfx_gfss_keyword::visible, gltfx_gfss_keyword::hidden}),
    contract_keywords(gltfx_gfss_property::overflow_y,
                      {gltfx_gfss_keyword::visible, gltfx_gfss_keyword::hidden}),

    // Group B - Caixas lado a lado / flex (ids 26-37)
    contract_keywords(gltfx_gfss_property::flex_direction,
                      {gltfx_gfss_keyword::row, gltfx_gfss_keyword::row_reverse,
                       gltfx_gfss_keyword::column, gltfx_gfss_keyword::column_reverse}),
    contract_keywords(
        gltfx_gfss_property::flex_wrap,
        {gltfx_gfss_keyword::nowrap, gltfx_gfss_keyword::wrap, gltfx_gfss_keyword::wrap_reverse}),
    contract_natures(gltfx_gfss_property::flex_grow, k_nature_number),
    contract_natures(gltfx_gfss_property::flex_shrink, k_nature_number),
    contract_natures(gltfx_gfss_property::flex_basis, k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::auto_keyword}),
    contract_keywords(gltfx_gfss_property::justify_content,
                      {gltfx_gfss_keyword::flex_start, gltfx_gfss_keyword::flex_end,
                       gltfx_gfss_keyword::center, gltfx_gfss_keyword::space_between,
                       gltfx_gfss_keyword::space_around, gltfx_gfss_keyword::space_evenly}),
    contract_keywords(gltfx_gfss_property::align_items,
                      {gltfx_gfss_keyword::stretch, gltfx_gfss_keyword::flex_start,
                       gltfx_gfss_keyword::flex_end, gltfx_gfss_keyword::center,
                       gltfx_gfss_keyword::baseline}),
    contract_keywords(gltfx_gfss_property::align_self,
                      {gltfx_gfss_keyword::auto_keyword, gltfx_gfss_keyword::stretch,
                       gltfx_gfss_keyword::flex_start, gltfx_gfss_keyword::flex_end,
                       gltfx_gfss_keyword::center, gltfx_gfss_keyword::baseline}),
    contract_keywords(gltfx_gfss_property::align_content,
                      {gltfx_gfss_keyword::stretch, gltfx_gfss_keyword::flex_start,
                       gltfx_gfss_keyword::flex_end, gltfx_gfss_keyword::center,
                       gltfx_gfss_keyword::space_between, gltfx_gfss_keyword::space_around,
                       gltfx_gfss_keyword::space_evenly}),
    contract_natures(gltfx_gfss_property::order, k_nature_integer),
    contract_natures(gltfx_gfss_property::row_gap, k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::column_gap, k_nature_length | k_nature_percentage),

    // Group C - Borda, fundo e casca (ids 38-68)
    contract_natures(
        gltfx_gfss_property::border_top_width, k_nature_length,
        {gltfx_gfss_keyword::thin, gltfx_gfss_keyword::medium, gltfx_gfss_keyword::thick}),
    contract_natures(
        gltfx_gfss_property::border_right_width, k_nature_length,
        {gltfx_gfss_keyword::thin, gltfx_gfss_keyword::medium, gltfx_gfss_keyword::thick}),
    contract_natures(
        gltfx_gfss_property::border_bottom_width, k_nature_length,
        {gltfx_gfss_keyword::thin, gltfx_gfss_keyword::medium, gltfx_gfss_keyword::thick}),
    contract_natures(
        gltfx_gfss_property::border_left_width, k_nature_length,
        {gltfx_gfss_keyword::thin, gltfx_gfss_keyword::medium, gltfx_gfss_keyword::thick}),
    contract_keywords(gltfx_gfss_property::border_top_style,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::solid}),
    contract_keywords(gltfx_gfss_property::border_right_style,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::solid}),
    contract_keywords(gltfx_gfss_property::border_bottom_style,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::solid}),
    contract_keywords(gltfx_gfss_property::border_left_style,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::solid}),
    contract_color(gltfx_gfss_property::border_top_color),
    contract_color(gltfx_gfss_property::border_right_color),
    contract_color(gltfx_gfss_property::border_bottom_color),
    contract_color(gltfx_gfss_property::border_left_color),
    contract_natures(gltfx_gfss_property::border_top_left_radius,
                     k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::border_top_right_radius,
                     k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::border_bottom_right_radius,
                     k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::border_bottom_left_radius,
                     k_nature_length | k_nature_percentage),
    contract_color(gltfx_gfss_property::background_color),
    contract_raw(gltfx_gfss_property::background_image), // <image>
    contract_keywords(gltfx_gfss_property::background_repeat,
                      {gltfx_gfss_keyword::repeat, gltfx_gfss_keyword::repeat_x,
                       gltfx_gfss_keyword::repeat_y, gltfx_gfss_keyword::no_repeat}),
    contract_natures(
        gltfx_gfss_property::background_size, k_nature_length | k_nature_percentage,
        {gltfx_gfss_keyword::auto_keyword, gltfx_gfss_keyword::contain, gltfx_gfss_keyword::cover}),
    contract_raw(gltfx_gfss_property::background_position), // <position>
    contract_raw(gltfx_gfss_property::border_image_source), // <image>
    contract_natures_arity(gltfx_gfss_property::border_image_slice,
                           k_nature_number | k_nature_percentage, 1, 4, {gltfx_gfss_keyword::fill}),
    contract_natures_arity(gltfx_gfss_property::border_image_width,
                           k_nature_length | k_nature_percentage | k_nature_number, 1, 4,
                           {gltfx_gfss_keyword::auto_keyword}),
    contract_natures_arity(gltfx_gfss_property::border_image_outset,
                           k_nature_length | k_nature_number, 1, 4),
    contract_keywords(gltfx_gfss_property::border_image_repeat,
                      {gltfx_gfss_keyword::stretch, gltfx_gfss_keyword::repeat}),
    contract_natures(
        gltfx_gfss_property::outline_width, k_nature_length,
        {gltfx_gfss_keyword::thin, gltfx_gfss_keyword::medium, gltfx_gfss_keyword::thick}),
    contract_keywords(gltfx_gfss_property::outline_style,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::solid}),
    contract_color(gltfx_gfss_property::outline_color),
    contract_natures(gltfx_gfss_property::outline_offset, k_nature_length),
    contract_keywords(gltfx_gfss_property::image_rendering,
                      {gltfx_gfss_keyword::auto_keyword, gltfx_gfss_keyword::pixelated}),

    // Group D - Texto (ids 69-80)
    contract_color(gltfx_gfss_property::color),
    contract_raw(gltfx_gfss_property::font_family), // lista de apelidos
    contract_natures(gltfx_gfss_property::font_size, k_nature_length | k_nature_percentage),
    contract_natures(gltfx_gfss_property::line_height,
                     k_nature_number | k_nature_length | k_nature_percentage,
                     {gltfx_gfss_keyword::normal}),
    contract_natures(gltfx_gfss_property::letter_spacing, k_nature_length,
                     {gltfx_gfss_keyword::normal}),
    contract_keywords(
        gltfx_gfss_property::text_align,
        {gltfx_gfss_keyword::left, gltfx_gfss_keyword::right, gltfx_gfss_keyword::center}),
    contract_keywords(gltfx_gfss_property::text_wrap_mode,
                      {gltfx_gfss_keyword::wrap, gltfx_gfss_keyword::nowrap}),
    contract_keywords(gltfx_gfss_property::text_overflow,
                      {gltfx_gfss_keyword::clip, gltfx_gfss_keyword::ellipsis}),
    contract_raw(gltfx_gfss_property::text_shadow), // <shadow>#
    contract_color(gltfx_gfss_property::gltfx_text_outline_color),
    contract_natures(gltfx_gfss_property::gltfx_text_outline_width, k_nature_length),
    contract_raw(gltfx_gfss_property::content),

    // Group E - Pintura e efeitos (ids 81-89)
    contract_number_range(gltfx_gfss_property::opacity, 0.0, 1.0, /*clamp=*/true),
    contract_keywords(gltfx_gfss_property::mix_blend_mode,
                      {gltfx_gfss_keyword::normal, gltfx_gfss_keyword::plus_lighter,
                       gltfx_gfss_keyword::multiply, gltfx_gfss_keyword::screen}),
    contract_raw(gltfx_gfss_property::filter),           // <filter-function-list>
    contract_raw(gltfx_gfss_property::backdrop_filter),  // <filter-function-list>
    contract_raw(gltfx_gfss_property::box_shadow),       // <shadow>#
    contract_raw(gltfx_gfss_property::clip_path),        // inset()/circle()/ellipse()
    contract_raw(gltfx_gfss_property::transform),        // <transform-list>
    contract_raw(gltfx_gfss_property::transform_origin), // duas escalas, SS5 item 7
    contract_keywords(gltfx_gfss_property::pointer_events,
                      {gltfx_gfss_keyword::auto_keyword, gltfx_gfss_keyword::none}),

    // Group F - Animacao (ids 90-103)
    contract_raw(
        gltfx_gfss_property::transition_property), // <nome de propriedade>#, vocabulario aberto
    contract_time_list(gltfx_gfss_property::transition_duration),
    contract_raw(gltfx_gfss_property::transition_timing_function), // <easing>#
    contract_time_list(gltfx_gfss_property::transition_delay),
    contract_raw(gltfx_gfss_property::animation_name), // <nome de @keyframes> / gltfx-<preset>
    contract_time_list(gltfx_gfss_property::animation_duration),
    contract_raw(gltfx_gfss_property::animation_timing_function), // <easing>#
    contract_time_list(gltfx_gfss_property::animation_delay),
    contract_raw(
        gltfx_gfss_property::animation_iteration_count), // numero|infinite, com "/ <time>" opcional
    contract_keywords(gltfx_gfss_property::animation_direction,
                      {gltfx_gfss_keyword::normal, gltfx_gfss_keyword::reverse,
                       gltfx_gfss_keyword::alternate, gltfx_gfss_keyword::alternate_reverse}),
    contract_keywords(gltfx_gfss_property::animation_fill_mode,
                      {gltfx_gfss_keyword::none, gltfx_gfss_keyword::forwards,
                       gltfx_gfss_keyword::backwards, gltfx_gfss_keyword::both}),
    contract_keywords(gltfx_gfss_property::animation_play_state,
                      {gltfx_gfss_keyword::running, gltfx_gfss_keyword::paused}),
    contract_number_range(gltfx_gfss_property::gltfx_velocity, 0.0, 3.0, /*clamp=*/false),
    contract_natures(gltfx_gfss_property::gltfx_chaos_seed, k_nature_integer,
                     {gltfx_gfss_keyword::auto_keyword}),
}};

static_assert(k_property_value_contracts.size() == gltfx_gfss_property_count,
              "GODS_LAWS.md L-40: k_property_value_contracts must carry exactly one row per "
              "property.hpp's own registry - a property added there without a matching contract "
              "row here must not compile silently");

// Returns the contract for `property` - `property.hpp`'s own id IS the
// row's own position (property_table.hpp's own precedent), so this is a
// direct index, never a search.
[[nodiscard]] constexpr const property_value_contract &
property_value_contract_for(gltfx_gfss_property property) noexcept {
    return k_property_value_contracts[static_cast<std::size_t>(property)];
}

} // namespace glintfx::style::detail
