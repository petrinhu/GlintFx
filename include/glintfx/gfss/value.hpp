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
// TRACK FOUND, FLAGGED, AND THE PROJECT LEADER HAS SINCE RESOLVED
// (GODS_LAWS.md L-27): the original GFSS-VALUE delivery (d47dff7)
// found ESCOPO.md SS2 decision 5 calling this "as treze unidades" over
// an itemized, mechanically-countable list of only TWELVE - de tela
// (px, dp); relativas a letra (em, rem, ex); relativas a janela (vw,
// vh); fisicas (in, cm, mm, pt, pc) - and, following GODS_LAWS.md's
// "lei das leis" (no agent revises a project-leader-ratified decision
// without asking), implemented the itemized 12 and flagged the word/
// count mismatch instead of resolving it alone. THE PROJECT LEADER
// CONFIRMED IT WAS A COUNTING ERROR (ESCOPO.md SS2, GFSS-VALUE-2, CTO
// design note /var/tmp/glintfx-plan/valor-angulo-tempo.md SS0.2: "o
// treze... era erro de contagem... numero vigente: 16") IN THE SAME
// sitting where he grew the category by four more members - `ch`, `lh`
// (letter-relative, joining em/rem/ex), `vmin`, `vmax` (window-
// relative, joining vw/vh). This header now implements the FULL
// itemized list (16 members, below), and gltfx_gfss_length_unit_count
// is STILL derived mechanically FROM that list, never hand-copied -
// the SAME discipline GODS_LAWS.md L-40's achado 1 already forces on
// token.hpp's own gltfx_gfss_token_kind_count.
//
// PHYSICAL UNITS' OWN FIXED RATIO (ESCOPO.md SS2 decision 5: "96
// pontos de tela por polegada", GFSS-RESOLVE's own row, TODO.md:
// "fisicas 96dp=1in") is GFSS-RESOLVE's OWN job, not this file's -
// this header only names in/cm/mm/pt/pc as members of the closed unit
// enum; converting a gltfx_gfss_length whose unit is one of them into
// screen pixels is a RESOLUTION step this fatia's own service order
// explicitly keeps out of scope ("sem conversao no parse").
//
// GFSS-VALUE-2 (28/08/2026, ESCOPO.md SS2 "as seis decisoes de
// angulo, tempo e cadencia" - commit 400df8f): this fatia was sent
// BACK by the project leader after adversarial review found the
// TDD cycle itself was not chronological (design and test were
// written together, proven only afterwards by mutation - GODS_LAWS.md
// L-20 is taxative, "sem excecao"). The redo below is the SAME
// red-before-green cycle, PLUS the scope the leader grew in the same
// sitting: two more natures (angle, time), four more length units
// (vmin, vmax, ch, lh - 16 total), a four-member angle-unit enum, and
// a six-member time-unit enum (the five standard SI-symbol spellings
// plus `frames`, this library's own addition - see gltfx_gfss_time_
// unit's own comment below for the semantics the leader defined for
// it).
//
// ACHADO 6 OF THE REVIEW THAT SENT THIS BACK, DECIDED HERE, NOT BY
// ACCIDENT (GODS_LAWS.md L-27): "se angulo e tempo sao naturezas
// fisicamente distintas, misturar-las no mesmo campo de comprimento,
// atras de um enum so, junta categorias incompativeis." DECISION: they
// are NOT merged. gltfx_gfss_angle and gltfx_gfss_time are their OWN
// plain-aggregate types, with their OWN closed unit enums
// (gltfx_gfss_angle_unit, gltfx_gfss_time_unit) - never the SAME enum
// gltfx_gfss_length_unit widened to also carry "deg"/"s", and never
// the SAME gltfx_gfss_length struct reused with a shared "generic
// dimension" unit field. A length, an angle and a duration are three
// PHYSICALLY INCOMPATIBLE dimensions - "5px + 3deg" is meaningless the
// same way "5 meters + 3 seconds" is meaningless in physics - and a
// consumer that can write gltfx_gfss_length{5.0, some_shared_enum_
// value_that_happens_to_mean_deg} would be handed a value the TYPE
// SYSTEM claims is a length but is actually an angle: exactly the
// silent-mixing defect ESCOPO.md SS2 decision 5's own "a separacao
// entre as tres primeiras existe porque mistura-las esconde erro"
// already named for number/integer/length, extended here to the two
// new categories on the SAME reasoning, not a new one. The cost of
// three parallel {magnitude, unit} shapes instead of one shared one is
// three enums and three name functions instead of one - paid once, in
// this pre-1.0 window (ESCOPO.md SS3's own L-26 duplicate: "SOVERSION
// 0 e nada de estabilidade prometida"), against a defect that would
// otherwise be undetectable by the compiler AND by every closed-
// enumeration test this file's own discipline (GODS_LAWS.md L-40)
// exists to make impossible to miss.

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
    X(percentage)                                                                                  \
    X(angle)                                                                                       \
    X(time)

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
// list's 16-member count). Grouped exactly as ESCOPO.md SS2's own
// prose groups them - screen (px, dp); font-relative (em, rem, ex, ch,
// lh); viewport-relative (vw, vh, vmin, vmax); physical, fixed-ratio
// (in, cm, mm, pt, pc) - purely for readability against that source
// while reviewing; the enum itself carries no group information.
#define GLINTFX_GFSS_LENGTH_UNIT_LIST(X)                                                           \
    X(px)                                                                                          \
    X(dp)                                                                                          \
    X(em)                                                                                          \
    X(rem)                                                                                         \
    X(ex)                                                                                          \
    X(ch)                                                                                          \
    X(lh)                                                                                          \
    X(vw)                                                                                          \
    X(vh)                                                                                          \
    X(vmin)                                                                                        \
    X(vmax)                                                                                        \
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

// GLINTFX_GFSS_ANGLE_UNIT_LIST(X) - the closed set of angle units
// (ESCOPO.md SS2, GFSS-VALUE-2, decision 2: "as quatro do padrao" -
// CSS Values and Units Level 4's own <angle> production, W3C). Same
// X-macro/mechanical-count/static_assert discipline as the length unit
// list above - this header's own top comment (achado 6) is why this
// is its OWN enum, never appended to gltfx_gfss_length_unit.
#define GLINTFX_GFSS_ANGLE_UNIT_LIST(X)                                                            \
    X(deg)                                                                                         \
    X(rad)                                                                                         \
    X(grad)                                                                                        \
    X(turn)

enum class gltfx_gfss_angle_unit : std::uint8_t {
#define GLINTFX_GFSS_ANGLE_UNIT_ENUMERATOR(name) name,
    GLINTFX_GFSS_ANGLE_UNIT_LIST(GLINTFX_GFSS_ANGLE_UNIT_ENUMERATOR)
#undef GLINTFX_GFSS_ANGLE_UNIT_ENUMERATOR
};

// Mechanically counted, same discipline as gltfx_gfss_length_unit_count
// above.
inline constexpr std::size_t gltfx_gfss_angle_unit_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_ANGLE_UNIT_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_ANGLE_UNIT_LIST(GLINTFX_GFSS_ANGLE_UNIT_COUNT_ONE)
#undef GLINTFX_GFSS_ANGLE_UNIT_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_ANGLE_UNIT_LIST

// Returns the IDENTIFIER of `unit` (e.g. "turn") - defined in
// value_kind.cpp, same table/static_assert technique as
// gltfx_gfss_length_unit_name() above.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_angle_unit_name(gltfx_gfss_angle_unit unit) noexcept;

// A number WITH an angle unit - the payload of gltfx_gfss_value::angle
// below. NEVER converted into degrees (or any other member of this
// same enum) at this layer - the unit the author wrote is the unit
// this struct carries; normalizing is GFSS-RESOLVE's job, the exact
// same boundary gltfx_gfss_length's own comment already draws for
// physical units.
struct gltfx_gfss_angle {
    double magnitude = 0.0;
    gltfx_gfss_angle_unit unit = gltfx_gfss_angle_unit::deg;
};

// GLINTFX_GFSS_TIME_UNIT_LIST(X) - the closed set of time units.
// FIVE are the standard SI-symbol spellings the project leader
// ratified after being shown the cost of a spelling of his own
// devising (ESCOPO.md SS2, GFSS-VALUE-2, decision 1: his own first
// verbatim was "sec, mili-sec, nano-sec" - he changed to `s`/`ms`/`ns`
// after being shown that a spelling CSS itself does not use would
// REJECT every standard-authored stylesheet, the same reasoning that
// already justified accepting the physical length units - `h` and
// `min` have no CSS precedent but keep the same SI-symbol family).
// `frames` is this library's OWN SIXTH addition, with its own
// non-obvious semantics - see gltfx_gfss_value's own `duration` field
// comment below for the full "consequencia declarada a ele" text; this
// header only registers it as a valid unit lexeme, exactly like every
// other member of this enum, and does NOT resolve it (no 60 Hz
// arithmetic anywhere in this file - see that same comment for why).
#define GLINTFX_GFSS_TIME_UNIT_LIST(X)                                                             \
    X(ms)                                                                                          \
    X(s)                                                                                           \
    X(min)                                                                                         \
    X(h)                                                                                           \
    X(ns)                                                                                          \
    X(frames)

enum class gltfx_gfss_time_unit : std::uint8_t {
#define GLINTFX_GFSS_TIME_UNIT_ENUMERATOR(name) name,
    GLINTFX_GFSS_TIME_UNIT_LIST(GLINTFX_GFSS_TIME_UNIT_ENUMERATOR)
#undef GLINTFX_GFSS_TIME_UNIT_ENUMERATOR
};

// Mechanically counted, same discipline as every other _count above.
inline constexpr std::size_t gltfx_gfss_time_unit_count = [] {
    std::size_t count = 0;
#define GLINTFX_GFSS_TIME_UNIT_COUNT_ONE(name) ++count;
    GLINTFX_GFSS_TIME_UNIT_LIST(GLINTFX_GFSS_TIME_UNIT_COUNT_ONE)
#undef GLINTFX_GFSS_TIME_UNIT_COUNT_ONE
    return count;
}();

#undef GLINTFX_GFSS_TIME_UNIT_LIST

// Returns the IDENTIFIER of `unit` (e.g. "frames") - defined in
// value_kind.cpp, same table/static_assert technique as every other
// _name() function above.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_gfss_time_unit_name(gltfx_gfss_time_unit unit) noexcept;

// A number WITH a time unit - the payload of gltfx_gfss_value::
// duration below (named `duration`, not `time`, to avoid colliding in
// a reader's head with <ctime>/POSIX `time` - ESCOPO.md SS2's own
// GFSS-VALUE-2 impact notes name this exact collision). Preserved as
// WRITTEN, same boundary as every other unit in this file: "1.5min" is
// NOT normalized to 90.0 s here.
struct gltfx_gfss_time {
    double magnitude = 0.0;
    gltfx_gfss_time_unit unit = gltfx_gfss_time_unit::ms;
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

    // Valid iff kind == length. A number WITH one of the 16 units
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

    // Valid iff kind == angle. A number WITH one of the 4 angle units
    // above (this header's own top comment, achado 6: a SEPARATE type
    // from length, never sharing its enum or its struct).
    gltfx_gfss_angle angle{};

    // Valid iff kind == time. A number WITH one of the 6 time units
    // above - see gltfx_gfss_time's own comment for the field's name.
    //
    // `frames` IS A DURATION ALIAS, NOT "ONE REAL FRAME OF THIS
    // MONITOR" (ESCOPO.md SS2, GFSS-VALUE-2 decision 3, project
    // leader's own verbatim, 28/08/2026): "entra, mas calculado com o
    // monitor. padrao seria 60[Hz] e pedir 3 realmente duraria 50ms,
    // mas se o monitor tiver 144, faria a conversao com duplicacao de
    // frame se necessario, para durar os mesmos 50ms." That is: "3
    // frames" names a FIXED duration - 3/60 s, 50 ms - against a 60 Hz
    // REFERENCE, and that duration is IDENTICAL on every monitor; the
    // number of ACTUAL frames a 144 Hz monitor spends realizing it (7,
    // with one duplicated) is a rendering-loop concern, computed
    // nowhere near this file. THIS LAYER DOES NOT DO THAT ARITHMETIC:
    // a `gltfx_gfss_value` with kind == time and unit == frames stores
    // the magnitude EXACTLY as written (duration.magnitude == 3.0 for
    // "3frames", never 50.0) - the SAME "preserved as written, no
    // resolution at parse time" rule this file already applies to
    // every other unit in every other nature. Converting the fixed
    // 60 Hz reference into nanoseconds (or into an actual frame count
    // for a given monitor) is GFSS-RESOLVE's job, explicitly named as
    // future work by the CTO's own dated design note
    // (/var/tmp/glintfx-plan/valor-angulo-tempo.md SS2.2/SS8) - not
    // this fatia's, not this field's.
    gltfx_gfss_time duration{};
};

} // namespace glintfx::style
