// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>
#include <glintfx/gfss/value.hpp>

// property.hpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_LAWS.md
// L-17/L-19/L-20/L-21/L-26/L-27/L-40, docs/gfss-property-registry-v1.
// md - 340 lines, twenty-two decisions, CTO, ratified in the project
// leader's own autonomous-mode order of 05/09/2026): the closed,
// PUBLIC identity of every gfss style property the v1 registry ships
// - [PMU], ONE-WAY DOOR (docs/gfss-property-registry-v1.md's own SS1
// E1: "o registro nasce COMPLETO ... e a partir dai so cresce pelo
// fim"). This header freezes exactly four things per property, no
// more (that file's own SS4: "o que NAO congela" is everything else -
// a composite type's own grammar, e.g. <shadow>/<transform-list>, is
// the READING fatia's job, never this one's):
//
//   1. THAT the property exists, as a member of gltfx_gfss_property.
//   2. ITS ID - the enumerator's own numeric value, equal to its
//      POSITION in GLINTFX_GFSS_PROPERTY_LIST(X) below, which is the
//      SS6 table's own row order (docs/gfss-property-registry-v1.md,
//      "o id e a posicao"). APPEND-ONLY, forever (project leader's own
//      verbatim, 28/08/2026, quoted in that file's own E1: "Sempre no
//      fim, nunca renumerar") - a property never moves once shipped,
//      and a new one is always added as the LAST X(...) line, never
//      inserted between two existing ones. gfss_property_registry_
//      test.cpp's own property_id_is_append_only_and_never_
//      renumerated case is the guard: an OWN, independently-authored
//      (identifier, expected id) table, checked against every single
//      enumerator's numeric value - reordering GLINTFX_GFSS_PROPERTY_
//      LIST(X) below shifts every enumerator after the moved line
//      SILENTLY (nothing in an unscoped enum's own numbering notices),
//      so the guard against that has to live OUTSIDE this file, in a
//      table nothing here can accidentally keep in sync with itself.
//   3. ITS NAME ON THE SHEET (gltfx_gfss_property_name()) - the exact
//      spelling a gfss author writes ("margin-top", "gltfx-Text_
//      Outline_Color" - the "gltfx-Iniciais_Maiusculas" shape is the
//      SHEET's own convention for this library's OWN, non-standard
//      additions, docs/gfss-property-registry-v1.md SS8; the C++
//      identifier is always plain snake_case, GODS_LAWS.md L-21).
//   4. WHETHER IT INHERITS, and ITS INITIAL VALUE - the only two
//      pieces of per-property metadata docs/gfss-property-registry-
//      v1.md's own SS4 makes PUBLIC ("so inherited e o initial sao
//      consultaveis publicamente ... porque GFSS-INHERIT e o
//      consumidor precisam deles"). Every other column of the SS6
//      table (accepted type, the reading fatia's own name, applied/
//      reserved status) lives in the INTERNAL table property.cpp
//      owns, never here.
//
// WHY gltfx_gfss_value IS THE INITIAL'S OWN RETURN TYPE, NOT A NEW
// TAGGED UNION (design continuity, not a fresh decision - GODS_LAWS.md
// L-27): value.hpp's own gltfx_gfss_value already carries "one decoded
// value component, whichever of its seven natures", including
// `keyword` with a RAW TEXT payload ("keyword carries raw text, no
// special-casing" - that file's own header comment). A property whose
// declared type is a NAMED COLOR (`border-top-color`, `color`, ...)
// never needed an eighth "color" nature added here: GFSS-COLOR-PARSE
// (src/gfss/color_parse.hpp, src/gfss/named_colors.cpp) already
// resolves a bare identifier lexeme like "black" or "transparent" the
// SAME way it resolves any other named color - so this registry's own
// initial for a color-typed property is simply gltfx_gfss_value{kind
// = keyword, keyword_text = "black"} (or "transparent" for `background
// -color`), exactly the shape GFSS-COLOR-PARSE's own keyword path
// already expects to receive, never a value this file has to invent a
// new nature for. Verified against the real table before relying on
// it (docs/gfss-property-registry-v1.md SS9's own "medido em casa"):
// src/gfss/named_colors.cpp carries both "black" and "transparent" as
// real table rows.
//
// THE ONE DECLARED GAP - `transform-origin` (id 88) - docs/gfss-
// property-registry-v1.md's own SS6 row for it names the initial
// "centro (ausencia)": CENTER IS NOT A DECLARABLE TOKEN in either of
// the two scales that property accepts (SS5 item 7) - it is what the
// ABSENCE of a declaration resolves to, in a two-axis <position>-
// shaped grammar this registry does not congeal (SS4's own "o que NAO
// congela"). No single gltfx_gfss_value honestly represents that -
// unlike `background-position` (id 58), whose own "0% 0%" initial
// collapses losslessly into ONE gltfx_gfss_value{kind = percentage,
// percentage = 0.0} because BOTH of its axes already agree on the same
// number. gltfx_gfss_property_initial(gltfx_gfss_property::transform_
// origin) instead returns gltfx_gfss_value{kind = keyword, keyword_
// text = ""} - an EMPTY keyword, never a fabricated literal token that
// would misrepresent what SS6 itself declares has no token - documented
// here, at the one call site that returns it, not silently. This is an
// INTERPRETATION this fatia made where docs/gfss-property-registry-v1.
// md itself does not pick a C++ representation (it only names the
// design tension in prose) - flagged to the CTO/team lead in this
// fatia's own delivery report, not decided unilaterally as if it were
// obvious.

namespace glintfx::style {

// GLINTFX_GFSS_PROPERTY_LIST(X) - the 104 properties of the v1
// registry, in SS6's own row order (id 0 = display ... id 103 =
// gltfx-Chaos_Seed). APPEND-ONLY: see this header's own top comment,
// item 2. Every X(name) here is the C++ identifier from SS6's own "no
// codigo" column, copied mechanically from the ratified spec (docs/
// gfss-property-registry-v1.md) - not retyped free-hand row by row,
// which is exactly where a transcription slip would hide in a table
// this size (this fatia's own delivery report names the mechanical
// extraction/cross-check step that produced this list and the
// property.cpp table below from the SAME source rows).
#define GLINTFX_GFSS_PROPERTY_LIST(X)                                                              \
    X(display)                                                                                     \
    X(box_sizing)                                                                                  \
    X(width)                                                                                       \
    X(height)                                                                                      \
    X(min_width)                                                                                   \
    X(min_height)                                                                                  \
    X(max_width)                                                                                   \
    X(max_height)                                                                                  \
    X(aspect_ratio)                                                                                \
    X(margin_top)                                                                                  \
    X(margin_right)                                                                                \
    X(margin_bottom)                                                                               \
    X(margin_left)                                                                                 \
    X(padding_top)                                                                                 \
    X(padding_right)                                                                               \
    X(padding_bottom)                                                                              \
    X(padding_left)                                                                                \
    X(position)                                                                                    \
    X(top)                                                                                         \
    X(right)                                                                                       \
    X(bottom)                                                                                      \
    X(left)                                                                                        \
    X(z_index)                                                                                     \
    X(visibility)                                                                                  \
    X(overflow_x)                                                                                  \
    X(overflow_y)                                                                                  \
    X(flex_direction)                                                                              \
    X(flex_wrap)                                                                                   \
    X(flex_grow)                                                                                   \
    X(flex_shrink)                                                                                 \
    X(flex_basis)                                                                                  \
    X(justify_content)                                                                             \
    X(align_items)                                                                                 \
    X(align_self)                                                                                  \
    X(align_content)                                                                               \
    X(order)                                                                                       \
    X(row_gap)                                                                                     \
    X(column_gap)                                                                                  \
    X(border_top_width)                                                                            \
    X(border_right_width)                                                                          \
    X(border_bottom_width)                                                                         \
    X(border_left_width)                                                                           \
    X(border_top_style)                                                                            \
    X(border_right_style)                                                                          \
    X(border_bottom_style)                                                                         \
    X(border_left_style)                                                                           \
    X(border_top_color)                                                                            \
    X(border_right_color)                                                                          \
    X(border_bottom_color)                                                                         \
    X(border_left_color)                                                                           \
    X(border_top_left_radius)                                                                      \
    X(border_top_right_radius)                                                                     \
    X(border_bottom_right_radius)                                                                  \
    X(border_bottom_left_radius)                                                                   \
    X(background_color)                                                                            \
    X(background_image)                                                                            \
    X(background_repeat)                                                                           \
    X(background_size)                                                                             \
    X(background_position)                                                                         \
    X(border_image_source)                                                                         \
    X(border_image_slice)                                                                          \
    X(border_image_width)                                                                          \
    X(border_image_outset)                                                                         \
    X(border_image_repeat)                                                                         \
    X(outline_width)                                                                               \
    X(outline_style)                                                                               \
    X(outline_color)                                                                               \
    X(outline_offset)                                                                              \
    X(image_rendering)                                                                             \
    X(color)                                                                                       \
    X(font_family)                                                                                 \
    X(font_size)                                                                                   \
    X(line_height)                                                                                 \
    X(letter_spacing)                                                                              \
    X(text_align)                                                                                  \
    X(text_wrap_mode)                                                                              \
    X(text_overflow)                                                                               \
    X(text_shadow)                                                                                 \
    X(gltfx_text_outline_color)                                                                    \
    X(gltfx_text_outline_width)                                                                    \
    X(content)                                                                                     \
    X(opacity)                                                                                     \
    X(mix_blend_mode)                                                                              \
    X(filter)                                                                                      \
    X(backdrop_filter)                                                                             \
    X(box_shadow)                                                                                  \
    X(clip_path)                                                                                   \
    X(transform)                                                                                   \
    X(transform_origin)                                                                            \
    X(pointer_events)                                                                              \
    X(transition_property)                                                                         \
    X(transition_duration)                                                                         \
    X(transition_timing_function)                                                                  \
    X(transition_delay)                                                                            \
    X(animation_name)                                                                              \
    X(animation_duration)                                                                          \
    X(animation_timing_function)                                                                   \
    X(animation_delay)                                                                             \
    X(animation_iteration_count)                                                                   \
    X(animation_direction)                                                                         \
    X(animation_fill_mode)                                                                         \
    X(animation_play_state)                                                                        \
    X(gltfx_velocity)                                                                              \
    X(gltfx_chaos_seed)

// std::uint16_t is docs/gfss-property-registry-v1.md's own SS4
// verbatim base type ("enum class com base std::uint16_t") - NOT the
// smallest type that fits today's 104 members (clang-tidy's own
// performance-enum-size would suggest std::uint8_t). Left wider on
// purpose: this is the ONE numeric id in this registry that is
// APPEND-ONLY FOREVER and PUBLISHED as a contract (this header's own
// top comment, item 2) - std::uint8_t caps at 256 members total, ever,
// across the ENTIRE life of a library whose own consumer base is
// public and unknown (LEI ZERO, GODS_LAWS.md) - a ceiling 104 members
// already sits 40% of the way toward. The four extra bytes per row
// this costs property_table.hpp's own 104-row table is not on any
// measured hot path.
enum class gltfx_gfss_property : std::uint16_t { // NOLINT(performance-enum-size) reason: see the
                                                 // paragraph above
#define GLINTFX_GFSS_PROPERTY_ENUMERATOR(name) name,
    GLINTFX_GFSS_PROPERTY_LIST(GLINTFX_GFSS_PROPERTY_ENUMERATOR)
#undef GLINTFX_GFSS_PROPERTY_ENUMERATOR
};

// Mechanically counted from GLINTFX_GFSS_PROPERTY_LIST above - never a
// hand-copied literal (GODS_LAWS.md L-40 achado 1, same discipline as
// value.hpp's own gltfx_gfss_value_kind_count). property.cpp's own
// k_property_table carries a static_assert comparing its OWN row count
// against this constant, so a property added to the X-macro list
// without a matching table row fails to COMPILE.
inline constexpr std::size_t gltfx_gfss_property_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_PROPERTY_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_PROPERTY_LIST(GLINTFX_GFSS_PROPERTY_COUNT_ONE)
#undef GLINTFX_GFSS_PROPERTY_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_PROPERTY_LIST

// Returns the SHEET spelling of `property` (e.g. "margin-top",
// "gltfx-Text_Outline_Color") - never the C++ identifier, and never a
// sentence (docs/api-conventions.md R7). Defined in property.cpp.
// noexcept, never undefined behavior: a `property` outside the table
// returns "unknown" (docs/api-conventions.md R4).
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_property_name(gltfx_gfss_property property) noexcept;

// Whether `property` participates in CSS-style inheritance (docs/
// gfss-property-registry-v1.md SS6's own "herda" column) - the flag
// GFSS-INHERIT (TODO.md) resolves against, together with the initial
// value below, for the three universal keywords `inherit`/`initial`/
// `unset`. A `property` outside the table returns `false` (docs/api-
// conventions.md R4's "never undefined behavior" convention, applied
// here as the same fail-safe default gltfx_gfss_property_name() uses).
[[nodiscard]] GLINTFX_API bool
gltfx_gfss_property_is_inherited(gltfx_gfss_property property) noexcept;

// Returns the INITIAL value `property` resolves to when never declared
// (docs/gfss-property-registry-v1.md SS6's own "inicial" column) - the
// value GFSS-INHERIT falls back to for a non-inherited property, or
// for the root element of an inherited one. See this header's own top
// comment for why this is always a gltfx_gfss_value (never a new
// "color" nature) and for the one declared exception (`transform-
// origin`, id 88). A `property` outside the table returns a default-
// constructed gltfx_gfss_value (kind == keyword, empty text) - the
// same "never undefined behavior" convention as every other accessor
// in this file.
[[nodiscard]] GLINTFX_API gltfx_gfss_value
gltfx_gfss_property_initial(gltfx_gfss_property property) noexcept;

} // namespace glintfx::style
