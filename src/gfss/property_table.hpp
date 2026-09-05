// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

#include <glintfx/gfss/property.hpp>

#include "gfss/property_status.hpp"

// property_table.hpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_LAWS.
// md L-17/L-19/L-20/L-26/L-27/L-40, docs/gfss-property-registry-v1.md
// - 340 lines, twenty-two decisions, CTO, ratified 05/09/2026): the
// ONE master table of the v1 registry's own per-property metadata,
// shared, header-only, by BOTH property.cpp (the PUBLIC accessors
// property.hpp declares) and property_status.cpp (the INTERNAL
// accessor property_status.hpp declares) - see property_status.cpp's
// own header comment for WHY the two live in separate .cpp files
// despite reading the exact same table (a real link-time constraint,
// not a style preference).
//
// `inline constexpr` (C++17's own inline-variable rule) is what makes
// sharing this table across two translation units safe without a
// third .cpp of its own: every TU that includes this header gets the
// IDENTICAL definition, and ODR treats them as one - the same
// technique value.hpp's own gltfx_gfss_*_count constants already use,
// just applied to a bigger constexpr object instead of a std::size_t.
//
// SAME "hand-written literal size, not the mechanical count, as the
// static_assert's OWN left side" DISCIPLINE AS value_kind.cpp (read
// that file's own header comment for the full reasoning): the array
// below is declared `std::array<property_entry, 104>`, a HAND-WRITTEN
// literal, not `gltfx_gfss_property_count` - so a property added to
// property.hpp's own X-macro list without a matching row here is a
// COMPILE failure (the static_assert just below the table), never a
// silently value-initialized trailing row.
//
// THE 104 ROWS THEMSELVES ARE NOT RETYPED FREE-HAND (this fatia's own
// delivery report names the exact mechanism): docs/gfss-property-
// registry-v1.md's own SS6 markdown table was parsed mechanically
// (id, sheet name, C++ identifier, "herda" column, "inicial" column),
// cross-checked field-by-field against the ratified document itself
// (every one of the 104 ids/sheet-names/identifiers/inherited-flags/
// initial-literals matched exactly on the FIRST full cross-check;
// isolating an EARLIER hand-transcription mistake of THIS session's
// own scratch copy of the "tipo" column - not the ratified document -
// where an abbreviation had accidentally introduced a keyword that is
// not in the real spec, caught and fixed before this file existed),
// then rendered into the exact literal rows below. Reading a row here
// against docs/gfss-property-registry-v1.md's own SS6 table is meant
// to be a MECHANICAL, line-for-line comparison, not free interpretation.
//
// THE FIVE SMALL FACTORY FUNCTIONS BELOW (keyword_initial/length_
// initial/percentage_initial/number_initial/integer_initial/time_
// initial_seconds) exist because DRY's own "regra de 3" (GODS_LAWS.md
// L-33) is triggered by orders of magnitude here - 104 rows share
// seven possible value.hpp natures, so each factory is used far more
// than three times, and each is a ONE-LINE atom (GODS_LAWS.md L-17):
// it sets `kind` and exactly the one field that nature actually reads,
// following value.hpp's own convention verbatim ("value.kind = ...;
// value.<field> = ...;", never a designated-initializer literal a
// reader would have to cross-reference against value.hpp's own member
// order to parse).
//
// WHY EVERY ROW HERE IS property_status::reserved (E1, docs/gfss-
// property-registry-v1.md SS1): see property_status.hpp's own header
// comment - no consuming fatia (LAYOUT-TREE, R2D-BORDER, ANIM-
// TIMELINE, FONT-LINE, ...) exists in this tree yet. The day the FIRST
// one lands, that property's own row flips to property_status::
// applied IN THE SAME COMMIT (E1's own rule) - never a bulk, later
// pass.
//
// THE ONE DECLARED GAP, REPEATED HERE AT ITS OWN ROW (property.hpp's
// own header comment carries the full reasoning): transform_origin's
// row below reads `keyword_initial("")` - an EMPTY keyword, because
// SS6 itself names this property's own initial "centro (ausencia)",
// not a literal token. This is the one row in the table that is NOT a
// literal transcription of a real gfss keyword - flagged in this
// fatia's own delivery report, not hidden here.

namespace glintfx::style::detail {

// --- small, reusable gltfx_gfss_value factories (see this file's own
// header comment for why these exist) -----------------------------

constexpr gltfx_gfss_value property_keyword_initial(std::string_view text) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::keyword;
    value.keyword_text = text;
    return value;
}

// px is this project's own screen-length default unit (value.hpp's
// own gltfx_gfss_length::unit default) - every zero-length initial in
// the table below (margin/padding/border-radius/outline-offset/...)
// is unit-agnostic AT MAGNITUDE 0 (0px == 0dp == 0in numerically), so
// picking px here never loses information; font-size's own 16px row
// is the one non-zero use, and IS the unit SS6 itself names.
constexpr gltfx_gfss_value property_length_initial(double magnitude_px) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::length;
    value.length.magnitude = magnitude_px;
    value.length.unit = gltfx_gfss_length_unit::px;
    return value;
}

constexpr gltfx_gfss_value property_percentage_initial(double magnitude) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::percentage;
    value.percentage = magnitude;
    return value;
}

constexpr gltfx_gfss_value property_number_initial(double magnitude) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::number;
    value.number = magnitude;
    return value;
}

constexpr gltfx_gfss_value property_integer_initial(long long magnitude) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::integer;
    value.integer_value = magnitude;
    return value;
}

// SS6's own zero-duration initials (`transition-duration`/`-delay`,
// `animation-duration`/`-delay`) are all literally "0s" - the unit the
// spec itself names, not the "no unit means milliseconds" authoring
// shortcut (that rule governs a SHEET AUTHOR omitting a unit, docs/
// gfss-property-registry-v1.md SS5 item 3 - it has no bearing on this
// registry's own frozen initial, which already carries an explicit
// unit). Magnitude 0 either way makes the unit numerically moot.
constexpr gltfx_gfss_value property_time_initial_seconds(double magnitude_s) {
    gltfx_gfss_value value{};
    value.kind = gltfx_gfss_value_kind::time;
    value.duration.magnitude = magnitude_s;
    value.duration.unit = gltfx_gfss_time_unit::s;
    return value;
}

// --- the master table ----------------------------------------------

struct property_entry {
    gltfx_gfss_property id = gltfx_gfss_property::display;
    std::string_view sheet_name;
    bool inherited = false;
    gltfx_gfss_value initial{};
    property_status status = property_status::reserved;
};

// 104 rows, SS6's own order (id 0 = display ... id 103 = gltfx-
// Chaos_Seed) - see this file's own header comment for how these rows
// were produced and cross-checked, never retyped free-hand against
// the ratified document a second time.
inline constexpr std::array<property_entry, 104> k_property_table{{
    {gltfx_gfss_property::display, "display", false, property_keyword_initial("block"),
     property_status::reserved},
    {gltfx_gfss_property::box_sizing, "box-sizing", false, property_keyword_initial("border-box"),
     property_status::reserved},
    {gltfx_gfss_property::width, "width", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::height, "height", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::min_width, "min-width", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::min_height, "min-height", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::max_width, "max-width", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::max_height, "max-height", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::aspect_ratio, "aspect-ratio", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::margin_top, "margin-top", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::margin_right, "margin-right", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::margin_bottom, "margin-bottom", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::margin_left, "margin-left", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::padding_top, "padding-top", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::padding_right, "padding-right", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::padding_bottom, "padding-bottom", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::padding_left, "padding-left", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::position, "position", false, property_keyword_initial("static"),
     property_status::reserved},
    {gltfx_gfss_property::top, "top", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::right, "right", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::bottom, "bottom", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::left, "left", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::z_index, "z-index", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::visibility, "visibility", true, property_keyword_initial("visible"),
     property_status::reserved},
    {gltfx_gfss_property::overflow_x, "overflow-x", false, property_keyword_initial("visible"),
     property_status::reserved},
    {gltfx_gfss_property::overflow_y, "overflow-y", false, property_keyword_initial("visible"),
     property_status::reserved},
    {gltfx_gfss_property::flex_direction, "flex-direction", false, property_keyword_initial("row"),
     property_status::reserved},
    {gltfx_gfss_property::flex_wrap, "flex-wrap", false, property_keyword_initial("nowrap"),
     property_status::reserved},
    {gltfx_gfss_property::flex_grow, "flex-grow", false, property_number_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::flex_shrink, "flex-shrink", false, property_number_initial(1.0),
     property_status::reserved},
    {gltfx_gfss_property::flex_basis, "flex-basis", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::justify_content, "justify-content", false,
     property_keyword_initial("flex-start"), property_status::reserved},
    {gltfx_gfss_property::align_items, "align-items", false, property_keyword_initial("stretch"),
     property_status::reserved},
    {gltfx_gfss_property::align_self, "align-self", false, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::align_content, "align-content", false,
     property_keyword_initial("stretch"), property_status::reserved},
    {gltfx_gfss_property::order, "order", false, property_integer_initial(0),
     property_status::reserved},
    {gltfx_gfss_property::row_gap, "row-gap", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::column_gap, "column-gap", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::border_top_width, "border-top-width", false,
     property_keyword_initial("medium"), property_status::reserved},
    {gltfx_gfss_property::border_right_width, "border-right-width", false,
     property_keyword_initial("medium"), property_status::reserved},
    {gltfx_gfss_property::border_bottom_width, "border-bottom-width", false,
     property_keyword_initial("medium"), property_status::reserved},
    {gltfx_gfss_property::border_left_width, "border-left-width", false,
     property_keyword_initial("medium"), property_status::reserved},
    {gltfx_gfss_property::border_top_style, "border-top-style", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::border_right_style, "border-right-style", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::border_bottom_style, "border-bottom-style", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::border_left_style, "border-left-style", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::border_top_color, "border-top-color", false,
     property_keyword_initial("black"), property_status::reserved},
    {gltfx_gfss_property::border_right_color, "border-right-color", false,
     property_keyword_initial("black"), property_status::reserved},
    {gltfx_gfss_property::border_bottom_color, "border-bottom-color", false,
     property_keyword_initial("black"), property_status::reserved},
    {gltfx_gfss_property::border_left_color, "border-left-color", false,
     property_keyword_initial("black"), property_status::reserved},
    {gltfx_gfss_property::border_top_left_radius, "border-top-left-radius", false,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::border_top_right_radius, "border-top-right-radius", false,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::border_bottom_right_radius, "border-bottom-right-radius", false,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::border_bottom_left_radius, "border-bottom-left-radius", false,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::background_color, "background-color", false,
     property_keyword_initial("transparent"), property_status::reserved},
    {gltfx_gfss_property::background_image, "background-image", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::background_repeat, "background-repeat", false,
     property_keyword_initial("repeat"), property_status::reserved},
    {gltfx_gfss_property::background_size, "background-size", false,
     property_keyword_initial("auto"), property_status::reserved},
    {gltfx_gfss_property::background_position, "background-position", false,
     property_percentage_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::border_image_source, "border-image-source", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::border_image_slice, "border-image-slice", false,
     property_percentage_initial(100.0), property_status::reserved},
    {gltfx_gfss_property::border_image_width, "border-image-width", false,
     property_length_initial(1.0), property_status::reserved},
    {gltfx_gfss_property::border_image_outset, "border-image-outset", false,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::border_image_repeat, "border-image-repeat", false,
     property_keyword_initial("stretch"), property_status::reserved},
    {gltfx_gfss_property::outline_width, "outline-width", false, property_keyword_initial("medium"),
     property_status::reserved},
    {gltfx_gfss_property::outline_style, "outline-style", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::outline_color, "outline-color", false, property_keyword_initial("black"),
     property_status::reserved},
    {gltfx_gfss_property::outline_offset, "outline-offset", false, property_length_initial(0.0),
     property_status::reserved},
    {gltfx_gfss_property::image_rendering, "image-rendering", true,
     property_keyword_initial("auto"), property_status::reserved},
    {gltfx_gfss_property::color, "color", true, property_keyword_initial("black"),
     property_status::reserved},
    {gltfx_gfss_property::font_family, "font-family", true, property_keyword_initial("sans-serif"),
     property_status::reserved},
    {gltfx_gfss_property::font_size, "font-size", true, property_length_initial(16.0),
     property_status::reserved},
    {gltfx_gfss_property::line_height, "line-height", true, property_keyword_initial("normal"),
     property_status::reserved},
    {gltfx_gfss_property::letter_spacing, "letter-spacing", true,
     property_keyword_initial("normal"), property_status::reserved},
    {gltfx_gfss_property::text_align, "text-align", true, property_keyword_initial("left"),
     property_status::reserved},
    {gltfx_gfss_property::text_wrap_mode, "text-wrap-mode", true, property_keyword_initial("wrap"),
     property_status::reserved},
    {gltfx_gfss_property::text_overflow, "text-overflow", false, property_keyword_initial("clip"),
     property_status::reserved},
    {gltfx_gfss_property::text_shadow, "text-shadow", true, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::gltfx_text_outline_color, "gltfx-Text_Outline_Color", true,
     property_keyword_initial("black"), property_status::reserved},
    {gltfx_gfss_property::gltfx_text_outline_width, "gltfx-Text_Outline_Width", true,
     property_length_initial(0.0), property_status::reserved},
    {gltfx_gfss_property::content, "content", false, property_keyword_initial("normal"),
     property_status::reserved},
    {gltfx_gfss_property::opacity, "opacity", false, property_number_initial(1.0),
     property_status::reserved},
    {gltfx_gfss_property::mix_blend_mode, "mix-blend-mode", false,
     property_keyword_initial("normal"), property_status::reserved},
    {gltfx_gfss_property::filter, "filter", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::backdrop_filter, "backdrop-filter", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::box_shadow, "box-shadow", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::clip_path, "clip-path", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::transform, "transform", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::transform_origin, "transform-origin", false, property_keyword_initial(""),
     property_status::reserved},
    {gltfx_gfss_property::pointer_events, "pointer-events", true, property_keyword_initial("auto"),
     property_status::reserved},
    {gltfx_gfss_property::transition_property, "transition-property", false,
     property_keyword_initial("all"), property_status::reserved},
    {gltfx_gfss_property::transition_duration, "transition-duration", false,
     property_time_initial_seconds(0.0), property_status::reserved},
    {gltfx_gfss_property::transition_timing_function, "transition-timing-function", false,
     property_keyword_initial("ease"), property_status::reserved},
    {gltfx_gfss_property::transition_delay, "transition-delay", false,
     property_time_initial_seconds(0.0), property_status::reserved},
    {gltfx_gfss_property::animation_name, "animation-name", false, property_keyword_initial("none"),
     property_status::reserved},
    {gltfx_gfss_property::animation_duration, "animation-duration", false,
     property_time_initial_seconds(0.0), property_status::reserved},
    {gltfx_gfss_property::animation_timing_function, "animation-timing-function", false,
     property_keyword_initial("ease"), property_status::reserved},
    {gltfx_gfss_property::animation_delay, "animation-delay", false,
     property_time_initial_seconds(0.0), property_status::reserved},
    {gltfx_gfss_property::animation_iteration_count, "animation-iteration-count", false,
     property_number_initial(1.0), property_status::reserved},
    {gltfx_gfss_property::animation_direction, "animation-direction", false,
     property_keyword_initial("normal"), property_status::reserved},
    {gltfx_gfss_property::animation_fill_mode, "animation-fill-mode", false,
     property_keyword_initial("none"), property_status::reserved},
    {gltfx_gfss_property::animation_play_state, "animation-play-state", false,
     property_keyword_initial("running"), property_status::reserved},
    {gltfx_gfss_property::gltfx_velocity, "gltfx-Velocity", true, property_number_initial(1.0),
     property_status::reserved},
    {gltfx_gfss_property::gltfx_chaos_seed, "gltfx-Chaos_Seed", false,
     property_keyword_initial("auto"), property_status::reserved},
}};

static_assert(k_property_table.size() == gltfx_gfss_property_count,
              "GODS_LAWS.md L-40: k_property_table's row count must track property.hpp's own "
              "gltfx_gfss_property_count - a property added to the X-macro list without a matching "
              "row here must not compile silently");

// Shared lookup, used by both property.cpp (public accessors) and
// property_status.cpp (internal status accessor) - one atom, one job
// (GODS_LAWS.md L-17), so neither .cpp writes its own copy of the same
// linear scan.
inline const property_entry *find_property_entry(gltfx_gfss_property property) noexcept {
    for (const property_entry &entry : k_property_table) {
        if (entry.id == property) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace glintfx::style::detail
