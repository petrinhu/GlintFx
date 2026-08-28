// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/value.hpp>

#include <array>

// value_kind.cpp - GFSS-VALUE (TODO.md, GODS_LAWS.md L-17/L-40): name
// and value live in ONE table per enum, the SAME technique token_kind.
// cpp already established for gltfx_gfss_token_kind_name() (itself
// following core/err_code.cpp's own gltfx_err_code_name()). This file
// answers exactly two questions - "what identifier names this value
// nature?" and "what identifier names this length unit?" - nothing
// else.
//
// THE TWO static_asserts BELOW ARE THIS FATIA'S OWN "ADD WITHOUT
// REGISTERING IT, WATCH IT FAIL TO COMPILE" PROOF (GODS_LAWS.md L-40,
// order of service item 3 - "acrescente uma natureza ou uma unidade
// sem registrar na enumeracao e veja se algo quebra... prefira a forma
// que quebra a compilacao"): each table's own <..., N> size below is a
// HAND-WRITTEN literal (5 and 12), not gltfx_gfss_value_kind_count/
// gltfx_gfss_length_unit_count themselves - if it read the mechanical
// count instead, an enumerator added to value.hpp's own X-macro list
// without a matching row here would silently value-initialize a
// trailing table entry to {keyword/px, ""} instead of failing to
// build. Because the size is instead a literal that does NOT track the
// list automatically, the enum's own mechanically-derived count (which
// DOES track it) and this table's row count can disagree the moment
// value.hpp's own list grows - and static_assert makes that
// disagreement a compile error instead of a silently wrong runtime
// answer. This was verified live during this fatia's own delivery: a
// throwaway 6th nature/13th unit added to value.hpp's own X-macro
// lists, with NO row added here, reproved the build with this exact
// static_assert's own message - then reverted.

namespace glintfx::style {

namespace {

struct value_kind_entry {
    gltfx_gfss_value_kind kind = gltfx_gfss_value_kind::keyword;
    std::string_view name;
};

// THE table - 7 rows, one per ESCOPO.md SS2 decision 5's own five
// original natures PLUS the two GFSS-VALUE-2 (28/08/2026) added -
// angle and time. Order matches the enum declaration.
constexpr std::array<value_kind_entry, 7> k_value_kind_table{{
    {gltfx_gfss_value_kind::keyword, "keyword"},
    {gltfx_gfss_value_kind::number, "number"},
    {gltfx_gfss_value_kind::integer, "integer"},
    {gltfx_gfss_value_kind::length, "length"},
    {gltfx_gfss_value_kind::percentage, "percentage"},
    {gltfx_gfss_value_kind::angle, "angle"},
    {gltfx_gfss_value_kind::time, "time"},
}};

static_assert(k_value_kind_table.size() == gltfx_gfss_value_kind_count,
              "GODS_LAWS.md L-40: k_value_kind_table's row count must track value.hpp's own "
              "gltfx_gfss_value_kind_count - a nature added to the enum without a name row here "
              "must not compile silently");

struct length_unit_entry {
    gltfx_gfss_length_unit unit = gltfx_gfss_length_unit::px;
    std::string_view name;
};

// THE table - 16 rows, one per ESCOPO.md SS2 decision 5's own itemized
// length units, GROWN BY FOUR (ch, lh, vmin, vmax) in the GFSS-VALUE-2
// (28/08/2026) emenda - see value.hpp's own header comment for the
// "treze"/12/16 provenance. Order matches the enum declaration.
constexpr std::array<length_unit_entry, 16> k_length_unit_table{{
    {gltfx_gfss_length_unit::px, "px"},
    {gltfx_gfss_length_unit::dp, "dp"},
    {gltfx_gfss_length_unit::em, "em"},
    {gltfx_gfss_length_unit::rem, "rem"},
    {gltfx_gfss_length_unit::ex, "ex"},
    {gltfx_gfss_length_unit::ch, "ch"},
    {gltfx_gfss_length_unit::lh, "lh"},
    {gltfx_gfss_length_unit::vw, "vw"},
    {gltfx_gfss_length_unit::vh, "vh"},
    {gltfx_gfss_length_unit::vmin, "vmin"},
    {gltfx_gfss_length_unit::vmax, "vmax"},
    {gltfx_gfss_length_unit::in, "in"},
    {gltfx_gfss_length_unit::cm, "cm"},
    {gltfx_gfss_length_unit::mm, "mm"},
    {gltfx_gfss_length_unit::pt, "pt"},
    {gltfx_gfss_length_unit::pc, "pc"},
}};

static_assert(k_length_unit_table.size() == gltfx_gfss_length_unit_count,
              "GODS_LAWS.md L-40: k_length_unit_table's row count must track value.hpp's own "
              "gltfx_gfss_length_unit_count - a unit added to the enum without a name row here "
              "must not compile silently");

struct angle_unit_entry {
    gltfx_gfss_angle_unit unit = gltfx_gfss_angle_unit::deg;
    std::string_view name;
};

// THE table - 4 rows, one per ESCOPO.md SS2 GFSS-VALUE-2 decision 2
// (28/08/2026): the four angle units of CSS Values and Units Level 4's
// own <angle> production. Order matches the enum declaration.
constexpr std::array<angle_unit_entry, 4> k_angle_unit_table{{
    {gltfx_gfss_angle_unit::deg, "deg"},
    {gltfx_gfss_angle_unit::rad, "rad"},
    {gltfx_gfss_angle_unit::grad, "grad"},
    {gltfx_gfss_angle_unit::turn, "turn"},
}};

static_assert(k_angle_unit_table.size() == gltfx_gfss_angle_unit_count,
              "GODS_LAWS.md L-40: k_angle_unit_table's row count must track value.hpp's own "
              "gltfx_gfss_angle_unit_count - a unit added to the enum without a name row here "
              "must not compile silently");

struct time_unit_entry {
    gltfx_gfss_time_unit unit = gltfx_gfss_time_unit::ms;
    std::string_view name;
};

// THE table - 6 rows, one per ESCOPO.md SS2 GFSS-VALUE-2 decisions 1
// and 3 (28/08/2026): the five SI-symbol spellings plus this library's
// own `frames`. Order matches the enum declaration.
constexpr std::array<time_unit_entry, 6> k_time_unit_table{{
    {gltfx_gfss_time_unit::ms, "ms"},
    {gltfx_gfss_time_unit::s, "s"},
    {gltfx_gfss_time_unit::min, "min"},
    {gltfx_gfss_time_unit::h, "h"},
    {gltfx_gfss_time_unit::ns, "ns"},
    {gltfx_gfss_time_unit::frames, "frames"},
}};

static_assert(k_time_unit_table.size() == gltfx_gfss_time_unit_count,
              "GODS_LAWS.md L-40: k_time_unit_table's row count must track value.hpp's own "
              "gltfx_gfss_time_unit_count - a unit added to the enum without a name row here "
              "must not compile silently");

} // namespace

std::string_view gltfx_gfss_value_kind_name(gltfx_gfss_value_kind kind) noexcept {
    for (const value_kind_entry &entry : k_value_kind_table) {
        if (entry.kind == kind) {
            return entry.name;
        }
    }
    // A raw value this build's table does not recognize (docs/api-
    // conventions.md R4's graceful-degradation convention): never
    // undefined behavior.
    return "unknown";
}

std::string_view gltfx_gfss_length_unit_name(gltfx_gfss_length_unit unit) noexcept {
    for (const length_unit_entry &entry : k_length_unit_table) {
        if (entry.unit == unit) {
            return entry.name;
        }
    }
    return "unknown";
}

std::string_view gltfx_gfss_angle_unit_name(gltfx_gfss_angle_unit unit) noexcept {
    for (const angle_unit_entry &entry : k_angle_unit_table) {
        if (entry.unit == unit) {
            return entry.name;
        }
    }
    return "unknown";
}

std::string_view gltfx_gfss_time_unit_name(gltfx_gfss_time_unit unit) noexcept {
    for (const time_unit_entry &entry : k_time_unit_table) {
        if (entry.unit == unit) {
            return entry.name;
        }
    }
    return "unknown";
}

} // namespace glintfx::style
