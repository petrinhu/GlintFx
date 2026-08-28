// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// gfss/value.hpp - GFSS-VALUE (TODO.md, GODS_LAWS.md L-17/L-19/L-20/
// L-22/L-27/L-28/L-40, ESCOPO.md SS2 decision 5, ratified by the
// project leader 27/08/2026 with the list shown to him item by item -
// "aceito todas"): the closed representation every gfss declaration
// value decodes into, and the closed enum of length units gfss
// recognizes. [PMU] - this layout froze the moment the project leader
// ratified it (see this header's own two comments below for the
// FIVE natures and the length-unit enum's own provenance); GFSS-PROP-
// REGISTRY (TODO.md) already depends on this file to store a
// property's own `initial` value.
//
// FIVE NATURES, NEVER CONVERTED INTO ONE ANOTHER AT THIS LAYER
// (ESCOPO.md SS2 decision 5, verbatim: "elas NUNCA se convertem umas
// nas outras na leitura"): a keyword, a bare <number> (flex-grow/
// flex-shrink are pure numbers, not lengths), a bare <integer>
// (distinct from <number> - a slot expecting a count silently
// receiving a decimal would be the exact defect the decision's own
// text names), a <length> (a number WITH a unit), and a <percentage>
// that STAYS a percentage - it never becomes a length during decode.
// gltfx_gfss_value below is a plain, FLAT aggregate carrying one field
// per nature (never a std::variant/union) - the SAME "always carries
// every field regardless of kind" shape token.hpp's own
// gltfx_gfss_token already uses (that type's `lexeme, line, column,
// diagnostic` are all present whatever `kind` is): trivially copyable,
// no owned resource (CONTRACT.md SS2.2's Rule of Zero), safe to pass
// and return by value across the .so/.dll boundary the SAME way
// gltfx_gfss_token/gltfx_gfss_diagnostic already are - keeping this
// shape instead of a tagged union avoids the union-of-non-trivial-
// members plumbing (a std::string_view member forces every OTHER
// union member to lose its own default member initializer) for a
// value type that is never on this project's own measured hot path
// (GODS_LAWS.md "profilear antes de otimizar" - CORE-COLOR/CORE-ERROR
// both made the identical "flat over packed" call already, for the
// identical reason).
//
// WHY THIS IS NOT std::variant<...> (design tension, deliberately
// declared, not silently picked - GODS_LAWS.md L-27): gltfx_rslt<T>'s
// own internal std::variant (docs/api-conventions.md R1) is safe
// because gltfx_rslt<T> is a TEMPLATE, instantiated fresh in the
// CONSUMER's own compiled code every time - std::variant never
// crosses the .so/.dll boundary as a fixed, pre-compiled layout there.
// gltfx_gfss_value is not a template: GFSS-PROP-REGISTRY's own row
// (TODO.md) means a FIXED-LAYOUT array of these lives inside glintfx's
// own compiled registry table, so this type's byte-for-byte layout has
// to be something every compiler on all five platforms lays out
// identically - a plain aggregate of scalar/string_view fields is that
// (the same guarantee gltfx_gfss_token/gltfx_gfss_diagnostic already
// rely on); std::variant's own layout is implementation-defined, not
// specified to be byte-identical across compilers.
//
// KEYWORD CARRIES RAW TEXT, NO SPECIAL-CASING OF THE THREE UNIVERSAL
// KEYWORDS (ESCOPO.md SS2 decision 5: "incluindo as tres universais de
// herdar, voltar ao valor de fabrica e desfazer" - GFSS-DECL-PARSE's
// own row, TODO.md: "a semantica vive em GFSS-INHERIT; a
// representacao, em GFSS-VALUE"): `inherit`/`initial`/`unset` decode
// through this SAME kind::keyword branch as any property-specific
// keyword (`block`, `border-box`) - this file only records THAT an
// ident was read, never WHAT it means. keyword_text is the token's own
// RAW lexeme (token.hpp's own "LEXEME IS THE RAW SOURCE SPAN" scope
// decision, inherited here on purpose, not re-litigated): escape
// sequences inside the ident are not resolved.
//
// LENGTH UNITS - THE ENUM'S OWN PROVENANCE, AND A DISCREPANCY THIS
// FATIA FOUND AND DOES NOT SILENTLY RESOLVE (GODS_LAWS.md L-27, marked
// as this fatia's own finding, not a fact copied without checking):
// ESCOPO.md SS2 decision 5 calls this "as treze unidades" and then
// itemizes them by category - de tela (px, dp); relativas a letra (em,
// rem, ex); relativas a janela (vw, vh); fisicas (in, cm, mm, pt, pc).
// Counted mechanically (2+3+2+5), that itemized list has TWELVE
// entries, not thirteen. TODO.md's own pre-rewrite GFSS-VALUE row text
// (predating the 26/08/2026 rewrite that split percentage into its own
// nature, decisions 6/9/10) DID list a 13th item inside that same
// enumeration: `%`. The most likely provenance, recorded here instead
// of guessed at silently: "treze" is a leftover word from BEFORE
// percentage was pulled out into its own nature - the itemized list
// (12) is what survived the rewrite; the word next to it was not
// updated to match. This header implements the ITEMIZED, mechanically
// countable list (12 members, below) - the one the project leader saw
// broken out category by category before ratifying - and gltfx_gfss_
// length_unit_count is derived FROM that list, never hand-copied,
// exactly the discipline GODS_LAWS.md L-40's achado 1 already forces
// on token.hpp's own gltfx_gfss_token_kind_count. Flagged to the
// project leader for confirmation in this fatia's own delivery; this
// header does not resolve the word/count mismatch on its own
// authority (GODS_LAWS.md's "lei das leis": no agent revises a
// project-leader-ratified decision without asking).
//
// PHYSICAL UNITS' OWN FIXED RATIO (ESCOPO.md SS2 decision 5: "96
// pontos de tela por polegada", GFSS-RESOLVE's own row, TODO.md:
// "fisicas 96dp=1in") is GFSS-RESOLVE's OWN job, not this file's -
// this header only names in/cm/mm/pt/pc as members of the closed unit
// enum; converting a gltfx_gfss_length whose unit is one of them into
// screen pixels is a RESOLUTION step this fatia's own service order
// explicitly keeps out of scope ("sem conversao no parse").

namespace glintfx::style {

// GLINTFX_GFSS_VALUE_KIND_LIST(X) - the closed set of value natures,
// generated the SAME X-macro way token.hpp's own GLINTFX_GFSS_
// TOKEN_KIND_LIST already is (GODS_LAWS.md L-40 achado 1: enum and
// mechanically-derived count can never drift apart because both come
// from this ONE list) - private to this header, defined and #undef'd
// immediately below.
#define GLINTFX_GFSS_VALUE_KIND_LIST(X)                                                            \
    X(keyword)                                                                                     \
    X(number)                                                                                      \
    X(integer)                                                                                     \
    X(length)                                                                                      \
    X(percentage)

enum class gltfx_gfss_value_kind : std::uint8_t {
#define GLINTFX_GFSS_VALUE_KIND_ENUMERATOR(name) name,
    GLINTFX_GFSS_VALUE_KIND_LIST(GLINTFX_GFSS_VALUE_KIND_ENUMERATOR)
#undef GLINTFX_GFSS_VALUE_KIND_ENUMERATOR
};

// Mechanically counted from GLINTFX_GFSS_VALUE_KIND_LIST above - never
// a hand-copied literal (see this header's own comment above, and
// value_kind.cpp's own k_value_kind_table/static_assert, which is what
// makes forgetting to register a 6th nature a COMPILE failure instead
// of a silent gap - GODS_LAWS.md L-40's "prefira a forma que quebra a
// compilacao").
inline constexpr std::size_t gltfx_gfss_value_kind_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_VALUE_KIND_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_VALUE_KIND_LIST(GLINTFX_GFSS_VALUE_KIND_COUNT_ONE)
#undef GLINTFX_GFSS_VALUE_KIND_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_VALUE_KIND_LIST

// Returns the IDENTIFIER of `kind` (e.g. "percentage"), never a
// sentence (docs/api-conventions.md R7) - defined in value_kind.cpp,
// alongside the hand-authored, static_assert-guarded table that is
// this header's own "add a nature without registering it and see it
// fail to compile" proof (GODS_LAWS.md L-40). noexcept, never
// undefined behavior: a `kind` outside this table returns "unknown",
// the SAME fallback gltfx_gfss_token_kind_name()/gltfx_err_code_name()
// already use.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_value_kind_name(gltfx_gfss_value_kind kind) noexcept;

// GLINTFX_GFSS_LENGTH_UNIT_LIST(X) - the closed set of length units
// (see this header's own comment above for the provenance of this
// list's 12-member count against ESCOPO.md SS2 decision 5's own prose
// "treze"). Grouped exactly as the decision's own prose groups them -
// screen (px, dp); font-relative (em, rem, ex); viewport-relative (vw,
// vh); physical, fixed-ratio (in, cm, mm, pt, pc) - purely for
// readability against that source while reviewing; the enum itself
// carries no group information.
#define GLINTFX_GFSS_LENGTH_UNIT_LIST(X)                                                           \
    X(px)                                                                                          \
    X(dp)                                                                                          \
    X(em)                                                                                          \
    X(rem)                                                                                         \
    X(ex)                                                                                          \
    X(vw)                                                                                          \
    X(vh)                                                                                          \
    X(in)                                                                                          \
    X(cm)                                                                                          \
    X(mm)                                                                                          \
    X(pt)                                                                                          \
    X(pc)

enum class gltfx_gfss_length_unit : std::uint8_t {
#define GLINTFX_GFSS_LENGTH_UNIT_ENUMERATOR(name) name,
    GLINTFX_GFSS_LENGTH_UNIT_LIST(GLINTFX_GFSS_LENGTH_UNIT_ENUMERATOR)
#undef GLINTFX_GFSS_LENGTH_UNIT_ENUMERATOR
};

// Mechanically counted, same discipline as gltfx_gfss_value_kind_count
// above.
inline constexpr std::size_t gltfx_gfss_length_unit_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_LENGTH_UNIT_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_LENGTH_UNIT_LIST(GLINTFX_GFSS_LENGTH_UNIT_COUNT_ONE)
#undef GLINTFX_GFSS_LENGTH_UNIT_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_LENGTH_UNIT_LIST

// Returns the IDENTIFIER of `unit` (e.g. "rem") - defined in
// value_kind.cpp, same table/static_assert technique as
// gltfx_gfss_value_kind_name() above, and this header's OTHER "add
// without registering, watch it fail to compile" proof.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_length_unit_name(gltfx_gfss_length_unit unit) noexcept;

// A number WITH a unit - the payload of gltfx_gfss_value::length
// below. Plain aggregate, same ABI-safety shape as every other value
// type in this track.
struct gltfx_gfss_length {
    double magnitude = 0.0;
    gltfx_gfss_length_unit unit = gltfx_gfss_length_unit::px;
};

// One decoded gfss value component - see this header's own top
// comment for why this is a flat aggregate, not a tagged union, and
// for the exact field that is meaningful per `kind`. Only the field
// matching `kind` is meaningful; the others hold their default value -
// the SAME "0/empty means never attached" convention docs/api-
// conventions.md R4 already establishes project-wide, read here as
// "not this value's own nature" rather than "never attached".
struct gltfx_gfss_value {
    gltfx_gfss_value_kind kind = gltfx_gfss_value_kind::keyword;

    // Valid iff kind == keyword. Raw ident lexeme (see this header's
    // own top comment on why the three universal keywords are not
    // special-cased here).
    std::string_view keyword_text;

    // Valid iff kind == number. A bare <number>, no unit attached
    // (flex-grow/flex-shrink's own domain).
    double number = 0.0;

    // Valid iff kind == integer. A bare <integer>, distinct from
    // `number` above by CSS Syntax Module Level 3's own 4.3.12 type
    // flag (a decimal point or an exponent marker makes it `number`
    // instead) - never reached by rounding/casting a decoded double,
    // always parsed as its own integer literal (value_parse.cpp's own
    // decode_integer_lexeme()).
    long long integer_value = 0;

    // Valid iff kind == length. A number WITH one of the 12 units
    // above.
    gltfx_gfss_length length{};

    // Valid iff kind == percentage. The RAW magnitude before the '%'
    // sign - "50%" decodes to 50.0, never 0.5 and never a
    // gltfx_gfss_length with an implied unit (ESCOPO.md SS2 decision
    // 5's own "porcentagem... nao vira comprimento na leitura").
    // Resolving it into an absolute value against a parent box's own
    // size is GFSS-RESOLVE's job (TODO.md's own declared boundary: "%
    // ... atravessam preservados para o gfui"), never this type's.
    double percentage = 0.0;
};

} // namespace glintfx::style
