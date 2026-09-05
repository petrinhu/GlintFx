// SPDX-License-Identifier: AGPL-3.0-or-later
#include "color_parse.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <glintfx/gfss/tokenizer.hpp>

#include "code_point.hpp"
#include "color_diagnostic_vocabulary.hpp"
#include "named_colors.hpp"
#include "numeric_lexeme.hpp"

// color_parse.cpp - GFSS-COLOR-PARSE (TODO.md, GODS_LAWS.md L-17: each
// function below answers exactly one question of the grammar
// color_parse.hpp's own header comment scopes - hex literal, named
// keyword, or one of the four legacy functions, never more than one
// per function).

namespace glintfx::style::detail {

namespace {

// --- shared plumbing -------------------------------------------------

gltfx_gfss_diagnostic make_diagnostic(const gltfx_gfss_token &token,
                                      std::string_view expected) noexcept {
    return gltfx_gfss_diagnostic{.line = token.line, .column = token.column, .expected = expected};
}

color_parse_result fail(const gltfx_gfss_diagnostic &diagnostic) noexcept {
    return color_parse_result{.ok = false, .value = {}, .diagnostic = diagnostic};
}

color_parse_result fail_at(const gltfx_gfss_token &token, std::string_view expected) noexcept {
    return fail(make_diagnostic(token, expected));
}

color_parse_result succeed(gltfx_rgba8 encoded) noexcept {
    return color_parse_result{
        .ok = true, .value = glintfx::gltfx_rgba_from_srgb8(encoded), .diagnostic = {}};
}

// The oklch() path's OWN success constructor - UNLIKE succeed() above,
// takes an already-LINEAR gltfx_rgba directly, never round-tripping
// through an 8-bit gltfx_rgba8 first. oklch()'s own conversion below
// already computes the exact linear-light answer in double precision;
// quantizing it to a byte and decoding it back would throw away
// exactly the headroom core/color.hpp's own decision 1 exists to keep
// (oklch() names colors a byte cannot represent without loss - see
// this file's own oklch color-space section below).
color_parse_result succeed_rgba(gltfx_rgba value) noexcept {
    return color_parse_result{.ok = true, .value = value, .diagnostic = {}};
}

// Reads the next token that is not <whitespace-token>, mirroring
// gltfx_gfss_next_token()'s own true/false convention (true while
// there is more; false the moment `out_token` IS the <EOF-token>) -
// color syntax has no construct where whitespace itself is
// significant (unlike, say, a future GFSS-VALUE-level shorthand),
// so every caller in this file wants exactly this skip.
bool next_significant_token(gltfx_gfss_cursor &cursor, gltfx_gfss_token &out_token) noexcept {
    bool more = true;
    do {
        more = gltfx_gfss_next_token(cursor, out_token);
    } while (more && out_token.kind == gltfx_gfss_token_kind::whitespace);
    return more;
}

// --- number decoding --------------------------------------------------
//
// decode_number_lexeme()/decode_percentage_lexeme() MOVED to
// numeric_lexeme.hpp/.cpp on GFSS-VALUE (GODS_LAWS.md L-17/L-27): see
// that file's own header comment for why - GFSS-VALUE needs the SAME
// <number-token> lexeme-to-double conversion for its own <number>/
// <integer>/<length> natures, and this project already paid two rounds
// of adversarial review to get the overflow-saturation path right the
// first time. This is a PURE MOVE - every call site below is unchanged,
// only the definition's own file moved.

// --- byte quantization ---------------------------------------------

// A <number> color component (0-255 domain) - clamps first, CSS Color
// 4 SS4/13's "the used value MUST be clamped" rule (color_parse.hpp's
// own scope-cut 5), the SAME convention core/color.hpp's own
// gltfx_rgba_to_srgb8() already applies at its own boundary.
std::uint8_t clamp_0_255_to_byte(double value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
}

// A unit fraction (0-1 domain: a <percentage> color component already
// divided by 100, or an <alpha-value>) - clamped THEN scaled, same
// rule.
std::uint8_t clamp_unit_to_byte(double unit) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0, 1.0) * 255.0));
}

std::uint8_t rgb_component_to_byte(double value, bool is_percentage) noexcept {
    return is_percentage ? clamp_unit_to_byte(value / 100.0) : clamp_0_255_to_byte(value);
}

// <alpha-value> ::= <number> [0,1] | <percentage> [0%,100%] - UNLIKE
// an rgb() color component, a plain <number> alpha is ALREADY a 0-1
// unit fraction, never a 0-255 domain value (CSS Color 4 SS13.3's own
// alpha grammar) - this is why alpha does not reuse
// rgb_component_to_byte() above even though both branch on the same
// `is_percentage` flag.
std::uint8_t alpha_component_to_byte(double value, bool is_percentage) noexcept {
    return clamp_unit_to_byte(is_percentage ? value / 100.0 : value);
}

// --- hex literal -----------------------------------------------------

std::uint8_t expand_hex_nibble(std::uint8_t nibble) noexcept {
    return static_cast<std::uint8_t>(nibble * 17U); // 0x/hex digit duplicated: 0xf -> 0xff
}

std::uint8_t hex_byte(std::uint8_t high_nibble, std::uint8_t low_nibble) noexcept {
    return static_cast<std::uint8_t>((high_nibble << 4U) | low_nibble);
}

// Precondition: every character of `hex_digit` already passed
// code_point.hpp's own is_hex_digit() - callers of this function check
// that first, at the point where a per-character diagnostic (line +
// column of the OFFENDING digit, not the token) would matter; this
// helper only ever converts what is already known-valid.
std::uint8_t hex_nibble_value(char hex_digit) noexcept {
    if (hex_digit >= '0' && hex_digit <= '9') {
        return static_cast<std::uint8_t>(hex_digit - '0');
    }
    if (hex_digit >= 'a' && hex_digit <= 'f') {
        return static_cast<std::uint8_t>(hex_digit - 'a' + 10);
    }
    assert(hex_digit >= 'A' && hex_digit <= 'F' &&
           "hex_nibble_value(): caller must validate is_hex_digit() first");
    return static_cast<std::uint8_t>(hex_digit - 'A' + 10);
}

bool hex_body_has_valid_length(std::size_t size) noexcept {
    return size == 3 || size == 4 || size == 6 || size == 8;
}

// #RGB/#RGBA (each digit duplicated) or #RRGGBB/#RRGGBBAA - the FOUR
// forms color_parse.hpp's own header comment enumerates. Alpha
// defaults to fully opaque (255) when the body has no alpha digits
// (3 or 6).
gltfx_rgba8 decode_hex_body(std::string_view body) noexcept {
    std::array<std::uint8_t, 8> nibble{};
    for (std::size_t i = 0; i < body.size(); ++i) {
        nibble[i] = hex_nibble_value(body[i]);
    }
    if (body.size() <= 4) {
        const std::uint8_t alpha = body.size() == 4 ? expand_hex_nibble(nibble[3]) : 255U;
        return gltfx_rgba8{.red = expand_hex_nibble(nibble[0]),
                           .green = expand_hex_nibble(nibble[1]),
                           .blue = expand_hex_nibble(nibble[2]),
                           .alpha = alpha};
    }
    const std::uint8_t alpha = body.size() == 8 ? hex_byte(nibble[6], nibble[7]) : 255U;
    return gltfx_rgba8{.red = hex_byte(nibble[0], nibble[1]),
                       .green = hex_byte(nibble[2], nibble[3]),
                       .blue = hex_byte(nibble[4], nibble[5]),
                       .alpha = alpha};
}

color_parse_result parse_hex_color(const gltfx_gfss_token &token) noexcept {
    const std::string_view body = token.lexeme.substr(1); // strip leading '#'
    if (!hex_body_has_valid_length(body.size())) {
        return fail_at(token, k_color_expected_valid_hex_length);
    }
    for (const char ch : body) {
        if (!is_hex_digit(static_cast<int>(static_cast<unsigned char>(ch)))) {
            return fail_at(token, k_color_expected_hex_digit);
        }
    }
    return succeed(decode_hex_body(body));
}

// --- named keyword -----------------------------------------------------

color_parse_result parse_named_color(const gltfx_gfss_token &token) noexcept {
    gltfx_rgba8 value{};
    if (!lookup_named_color(token.lexeme, value)) {
        return fail_at(token, k_color_expected_known_color_keyword);
    }
    return succeed(value);
}

// --- hsl() conversion -------------------------------------------------

struct rgb_unit {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

// CSS Color 4's own reference "hsl to rgb" conversion (public, standard
// color math - https://www.w3.org/TR/css-color-4/#hsl-to-rgb, read
// under GODS_LAWS.md L-29, re-derived and checked by hand against
// hsl(0, 100%, 50%) == red before being trusted here, never copied
// CODE from any implementation of it). `hue_deg` is unrestricted (this
// function itself normalizes into [0,360)); `sat_unit`/`light_unit`
// are already-clamped [0,1] fractions - the caller (parse_hsl_color()
// below) owns turning a <percentage> token into that fraction.
rgb_unit hsl_to_rgb_unit(double hue_deg, double sat_unit, double light_unit) noexcept {
    double hue = std::fmod(hue_deg, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double sat = std::clamp(sat_unit, 0.0, 1.0);
    const double light = std::clamp(light_unit, 0.0, 1.0);
    const double a = sat * std::min(light, 1.0 - light);

    auto channel = [&](double n) noexcept -> double {
        const double k = std::fmod(n + hue / 30.0, 12.0);
        return light - a * std::max(-1.0, std::min({k - 3.0, 9.0 - k, 1.0}));
    };
    return rgb_unit{.red = channel(0.0), .green = channel(8.0), .blue = channel(4.0)};
}

// --- oklch() color-space conversion ------------------------------------
//
// GFSS-OKLCH (TODO.md, ESCOPO.md SS4 decision 8): converts an OKLCh
// perceptual triple into linear sRGB, gamut-mapping any color oklch()
// can name that the display's sRGB gamut cannot reproduce - the work
// color_parse.hpp's own scope-cut 6 names as the reason this notation
// waited for its own fatia instead of joining hex/named/rgb/hsl above.
//
// SOURCES, READ UNDER GODS_LAWS.md L-29/L-43 (learn the technique,
// never copy code - every number below is a PUBLISHED CONSTANT,
// re-derived from the cited source, never pasted from any
// implementation of it):
//
//   1. THE OKLAB<->LINEAR-SRGB MATRICES: Björn Ottosson, "A perceptual
//      color space for image processing"
//      (https://bottosson.github.io/posts/oklab/, "Conversion from
//      linear sRGB to Oklab" and its own inverse) - the ORIGINAL
//      PUBLICATION OKLab and OKLCh are defined in, and what the CSS
//      Color 4 spec's own oklab()/oklch() section
//      (https://www.w3.org/TR/css-color-4/#specifying-oklab-oklch)
//      points straight at. The nine constants each direction below are
//      that page's own LMS<->linear-sRGB and LMS'(cube-root)<->Lab 3x3
//      matrices, transcribed digit for digit - a number is not
//      copyrightable expression (GODS_LAWS.md L-28's own "Termos de
//      licenca" section already settles this for published
//      color-format constants).
//   2. THE GAMUT MAPPING ALGORITHM: CSS Color Module Level 4, section
//      "CSS Gamut Mapping" (https://www.w3.org/TR/css-color-4/#css-
//      gamut-mapping) defines the algorithm every browser's own
//      oklch()-to-sRGB conversion implements: binary search over OKLCh
//      chroma at fixed lightness/hue, converging when the candidate's
//      own naive per-channel clip is within a "just noticeable
//      difference" of the candidate itself. The two constants that
//      govern convergence (JND 0.02, chroma-search EPSILON 0.0001) and
//      the control flow (a `min_in_gamut` flag that lets the search
//      keep shrinking chroma even past the first in-gamut hit, because
//      deltaEOK is not guaranteed monotonic in chroma) were confirmed
//      live against color-js/color.js's own toGamutCSS() function
//      (https://github.com/color-js/color.js/blob/main/src/toGamut.js)
//      - the reference implementation the CSS Working Group itself
//      cites for this algorithm - then re-derived by hand in this
//      project's own types (GODS_LAWS.md L-29: the STRUCTURE is the
//      technique learned; every line below is this project's own).
//
// WHY THE "IN GAMUT" CHECK NEEDS NO GAMMA ENCODING (design choice made
// HERE, GODS_LAWS.md L-27, marked INFERENCE): the cited algorithm's
// own "clip" step converts to the destination RGB space and clamps
// each channel to [0,1] there. core/color.hpp's own gltfx_rgba is
// ALREADY linear light (that header's own decision 3), and the sRGB
// transfer function (IEC 61966-2-1) is a monotonic bijection of [0,1]
// onto [0,1] with f(0)=0 and f(1)=1 at BOTH ends - clamping a
// component to [0,1] in gamma-encoded sRGB and clamping the SAME
// component to [0,1] in linear sRGB are therefore the IDENTICAL
// operation. Every step below works directly in the linear-light
// gltfx_rgba domain, with no gamma round-trip anywhere.
//
// TOTAL, NEVER FALLIBLE: every real (lightness, chroma, hue) triple has
// a well-defined answer (lightness outside [0,1] maps to pure
// white/black by the algorithm's own first two branches; negative
// chroma clamps to zero, hue is an unrestricted periodic angle - the
// SAME "clamp/wrap, never reject" convention color_parse.hpp's own
// scope-cut 5 and hsl_to_rgb_unit()'s own header comment already
// establish above). Alpha plays NO PART in this conversion (CSS Color
// 4's own "alpha is independent of color-space transforms",
// core/color.hpp decision 5's own straight-alpha convention) - the
// caller (parse_oklch_arguments() below) reads it separately and
// overwrites the returned value's own `.alpha`.

struct linear_srgb_triplet {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

struct oklab_triplet {
    double lightness = 0.0;
    double a = 0.0;
    double b = 0.0;
};

oklab_triplet linear_srgb_to_oklab(const linear_srgb_triplet &rgb) noexcept {
    const double l = 0.4122214708 * rgb.red + 0.5363325363 * rgb.green + 0.0514459929 * rgb.blue;
    const double m = 0.2119034982 * rgb.red + 0.6806995451 * rgb.green + 0.1073969566 * rgb.blue;
    const double s = 0.0883024619 * rgb.red + 0.2817188376 * rgb.green + 0.6299787005 * rgb.blue;

    // std::cbrt already handles a negative radicand correctly (IEEE
    // 754: cbrt(-x) == -cbrt(x)) - l/m/s go negative for a
    // saturated/out-of-gamut color (Ottosson's own page: "L, M and S
    // can become negative for colors outside of the sRGB gamut"), so
    // this is never std::pow(x, 1.0/3.0), which is undefined for a
    // negative base.
    const double l_root = std::cbrt(l);
    const double m_root = std::cbrt(m);
    const double s_root = std::cbrt(s);

    return oklab_triplet{
        .lightness = 0.2104542553 * l_root + 0.7936177850 * m_root - 0.0040720468 * s_root,
        .a = 1.9779984951 * l_root - 2.4285922050 * m_root + 0.4505937099 * s_root,
        .b = 0.0259040371 * l_root + 0.7827717662 * m_root - 0.8086757660 * s_root,
    };
}

linear_srgb_triplet oklab_to_linear_srgb(const oklab_triplet &lab) noexcept {
    const double l_root = lab.lightness + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    const double m_root = lab.lightness - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    const double s_root = lab.lightness - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

    const double l = l_root * l_root * l_root;
    const double m = m_root * m_root * m_root;
    const double s = s_root * s_root * s_root;

    return linear_srgb_triplet{
        .red = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        .green = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        .blue = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
    };
}

// OKLCh -> OKLab is plain polar-to-cartesian - CSS Color 4's own
// oklch() definition IS this pairing, by name, needing no citation
// beyond the matrices above.
oklab_triplet oklch_to_oklab(double lightness, double chroma, double hue_degrees) noexcept {
    const double hue_radians = hue_degrees * std::numbers::pi / 180.0;
    return oklab_triplet{
        .lightness = lightness,
        .a = chroma * std::cos(hue_radians),
        .b = chroma * std::sin(hue_radians),
    };
}

linear_srgb_triplet oklch_to_linear_srgb(double lightness, double chroma,
                                         double hue_degrees) noexcept {
    return oklab_to_linear_srgb(oklch_to_oklab(lightness, chroma, hue_degrees));
}

bool is_in_unit_cube(const linear_srgb_triplet &rgb) noexcept {
    return rgb.red >= 0.0 && rgb.red <= 1.0 && rgb.green >= 0.0 && rgb.green <= 1.0 &&
           rgb.blue >= 0.0 && rgb.blue <= 1.0;
}

linear_srgb_triplet clip_to_unit_cube(const linear_srgb_triplet &rgb) noexcept {
    return linear_srgb_triplet{
        .red = std::clamp(rgb.red, 0.0, 1.0),
        .green = std::clamp(rgb.green, 0.0, 1.0),
        .blue = std::clamp(rgb.blue, 0.0, 1.0),
    };
}

// deltaEOK: plain Euclidean distance in OKLab (CSS Color 4's own
// "Color Differences" section, this file's own citation 2 above).
double delta_e_ok(const oklab_triplet &first, const oklab_triplet &second) noexcept {
    const double delta_lightness = first.lightness - second.lightness;
    const double delta_a = first.a - second.a;
    const double delta_b = first.b - second.b;
    return std::sqrt(delta_lightness * delta_lightness + delta_a * delta_a + delta_b * delta_b);
}

constexpr double k_gamut_mapping_jnd = 0.02;
constexpr double k_gamut_mapping_chroma_epsilon = 0.0001;

// The cited binary search itself (this file's own citation 2 above) -
// called only once the caller already knows `chroma` at (`lightness`,
// `hue_degrees`) falls OUTSIDE the unit cube.
linear_srgb_triplet gamut_map_to_linear_srgb(double lightness, double chroma,
                                             double hue_degrees) noexcept {
    double chroma_low = 0.0;
    double chroma_high = chroma;
    bool low_is_known_in_gamut = true;

    double current_chroma = chroma;
    linear_srgb_triplet current_rgb = oklch_to_linear_srgb(lightness, current_chroma, hue_degrees);
    linear_srgb_triplet clipped = clip_to_unit_cube(current_rgb);
    double error = delta_e_ok(oklch_to_oklab(lightness, current_chroma, hue_degrees),
                              linear_srgb_to_oklab(clipped));
    if (error < k_gamut_mapping_jnd) {
        return clipped;
    }

    while (chroma_high - chroma_low > k_gamut_mapping_chroma_epsilon) {
        current_chroma = (chroma_low + chroma_high) / 2.0;
        current_rgb = oklch_to_linear_srgb(lightness, current_chroma, hue_degrees);
        if (low_is_known_in_gamut && is_in_unit_cube(current_rgb)) {
            chroma_low = current_chroma;
            continue;
        }
        clipped = clip_to_unit_cube(current_rgb);
        error = delta_e_ok(oklch_to_oklab(lightness, current_chroma, hue_degrees),
                           linear_srgb_to_oklab(clipped));
        if (error < k_gamut_mapping_jnd) {
            if (k_gamut_mapping_jnd - error < k_gamut_mapping_chroma_epsilon) {
                break;
            }
            low_is_known_in_gamut = false;
            chroma_low = current_chroma;
        } else {
            chroma_high = current_chroma;
        }
    }
    return clipped;
}

// The one entry point parse_oklch_arguments() below calls - see this
// section's own header comment for the full derivation of every
// branch.
gltfx_rgba oklch_to_gltfx_rgba(double lightness, double chroma, double hue_degrees) noexcept {
    const double clamped_chroma = std::max(chroma, 0.0);

    linear_srgb_triplet result{};
    if (lightness >= 1.0) {
        result = linear_srgb_triplet{.red = 1.0, .green = 1.0, .blue = 1.0};
    } else if (lightness <= 0.0) {
        result = linear_srgb_triplet{.red = 0.0, .green = 0.0, .blue = 0.0};
    } else {
        const linear_srgb_triplet origin =
            oklch_to_linear_srgb(lightness, clamped_chroma, hue_degrees);
        result = is_in_unit_cube(origin)
                     ? origin
                     : gamut_map_to_linear_srgb(lightness, clamped_chroma, hue_degrees);
    }

    return gltfx_rgba{.red = static_cast<float>(result.red),
                      .green = static_cast<float>(result.green),
                      .blue = static_cast<float>(result.blue),
                      .alpha = 1.0F};
}

// --- rgb()/rgba()/hsl()/hsla() ----------------------------------------

enum class color_function_kind : std::uint8_t { rgb, rgba, hsl, hsla };

bool try_match_color_function_kind(std::string_view name, color_function_kind &out_kind) noexcept {
    struct entry {
        // Default member initializer, not a user-declared constructor
        // (would forfeit aggregate-init below) - the SAME cppcheck
        // uninitMemberVarNoCtor fix named_colors.hpp's own
        // named_color_entry already applies.
        std::string_view name;
        color_function_kind kind = color_function_kind::rgb;
    };
    constexpr std::array<entry, 4> k_functions{{
        {"rgb", color_function_kind::rgb},
        {"rgba", color_function_kind::rgba},
        {"hsl", color_function_kind::hsl},
        {"hsla", color_function_kind::hsla},
    }};
    for (const entry &candidate : k_functions) {
        if (ascii_case_insensitive_equal(candidate.name, name)) {
            out_kind = candidate.kind;
            return true;
        }
    }
    return false;
}

// ESCOPO.md SS4 decision 8 / color_parse.hpp's own scope-cut 6: these
// three are RECOGNIZED CSS color notations this version does not ship.
// oklch() USED to be a fourth member of this list (GFSS-OKLCH,
// TODO.md, was still "Pendente" then) - GFSS-OKLCH shipped it (see
// this file's own "oklch() color-space conversion" section above and
// parse_oklch_arguments() below), so it is recognized and parsed now,
// never reaching this predicate. lab()/lch()/oklab() remain out of the
// v1 scope entirely (ESCOPO.md SS4 decision 7 names only oklch() as
// entering v1 alongside the legacy notations).
bool is_deferred_color_notation(std::string_view name) noexcept {
    constexpr std::array<std::string_view, 3> k_deferred{{"lab", "lch", "oklab"}};
    for (const std::string_view candidate : k_deferred) {
        if (ascii_case_insensitive_equal(candidate, name)) {
            return true;
        }
    }
    return false;
}

struct color_component {
    double value = 0.0;
    bool is_percentage = false;
};

// Which token kind(s) a given argument SLOT accepts - hue accepts only
// <number>, saturation/lightness accept only <percentage>, an rgb()
// color component or an alpha value accepts either. A PLAIN BOOLEAN
// pair ("accepts number" / "accepts percentage") would let a caller
// construct the nonsensical "accepts neither" by mistake; this enum
// makes that combination unrepresentable.
enum class component_kind_requirement : std::uint8_t {
    number_or_percentage,
    number_only,
    percentage_only
};

// The three OUT parameters read_color_component() below fills on
// every call, success or failure, grouped into one struct -
// CONTRACT.md SS6.2's own 4-parameter ceiling (GODS_LAWS.md L-17): the
// bare function used to take `cursor` and `requirement` PLUS three
// more out-parameters, five in total. `component` and `token` are
// BOTH needed on a SUCCESSFUL call (the caller's own
// rgb_component_type_matches_first() check needs the token's own
// line/column to build a k_color_expected_uniform_component_types
// diagnostic if a LATER argument fails the uniform-type check, and by
// then read_color_component() has already moved on to that later
// argument's own token) - `failure` is the only field that only
// matters on the false path.
struct color_component_read {
    color_component component{};
    gltfx_gfss_token token{};
    gltfx_gfss_diagnostic failure{};
};

// One argument matching `requirement` - MUTATION-FOUND BUG, FIXED HERE
// (this project's own first real red, captured live before this fix:
// hsl(50%, 50%, 50%) - a PERCENTAGE in the HUE slot - was wrongly
// ACCEPTED, because an earlier version of this function only used its
// "which diagnostic to raise" parameter on the WRONG-TOKEN-KIND
// fallback branch, never to REJECT a token that matched a DIFFERENT,
// still-valid-looking kind. `requirement` is now consulted on EVERY
// branch, not only the last one - a <percentage-token> in a
// number_only slot, or a <number-token> in a percentage_only slot,
// both now fall through to the SAME "neither branch matched" failure
// path a genuinely wrong token kind already took.
bool read_color_component(gltfx_gfss_cursor &cursor, component_kind_requirement requirement,
                          color_component_read &out) noexcept {
    if (!next_significant_token(cursor, out.token)) {
        out.failure = make_diagnostic(out.token, k_color_expected_argument_count);
        return false;
    }
    const bool accepts_number = requirement != component_kind_requirement::percentage_only;
    const bool accepts_percentage = requirement != component_kind_requirement::number_only;
    if (accepts_number && out.token.kind == gltfx_gfss_token_kind::number) {
        out.component = color_component{.value = decode_number_lexeme(out.token.lexeme),
                                        .is_percentage = false};
        return true;
    }
    if (accepts_percentage && out.token.kind == gltfx_gfss_token_kind::percentage) {
        out.component = color_component{.value = decode_percentage_lexeme(out.token.lexeme),
                                        .is_percentage = true};
        return true;
    }
    const std::string_view expected = requirement == component_kind_requirement::number_only
                                          ? k_color_expected_number
                                      : requirement == component_kind_requirement::percentage_only
                                          ? k_color_expected_percentage
                                          : k_color_expected_number_or_percentage;
    out.failure = make_diagnostic(out.token, expected);
    return false;
}

// The separator that must follow argument `index` (0-based) of
// `required_args` total: a comma for every argument but the last, a
// close-paren for the last. Returns false (out_failure set) on
// anything else, including distinguishing "one comma too many" (an
// extra argument) from "the ')' never came".
bool read_component_separator(gltfx_gfss_cursor &cursor, std::size_t index,
                              std::size_t required_args,
                              gltfx_gfss_diagnostic &out_failure) noexcept {
    const bool is_last_argument = index + 1 == required_args;
    gltfx_gfss_token token{};
    if (!next_significant_token(cursor, token)) {
        // Ran out of input: if we already have every required
        // component, the only thing missing is the ')' itself; if we
        // do not, this is the SAME "too few arguments" shape
        // read_color_component() reports when EOF arrives in ITS OWN
        // slot - both roads lead to the SAME diagnostic identifier for
        // the SAME real defect (arity), the paren case is the only
        // one that is not an arity question at all.
        out_failure = make_diagnostic(token, is_last_argument ? k_color_expected_closing_parenthesis
                                                              : k_color_expected_argument_count);
        return false;
    }
    if (is_last_argument) {
        if (token.kind == gltfx_gfss_token_kind::close_paren) {
            return true;
        }
        out_failure = make_diagnostic(token, token.kind == gltfx_gfss_token_kind::comma
                                                 ? k_color_expected_argument_count
                                                 : k_color_expected_closing_parenthesis);
        return false;
    }
    if (token.kind == gltfx_gfss_token_kind::comma) {
        return true;
    }
    out_failure = make_diagnostic(token, k_color_expected_comma);
    return false;
}

// rgb()/rgba()'s own extra rule CSS Color 3 states once for the whole
// function (color_parse.hpp's own scope-cut 3): the three COLOR
// components (index 0/1/2 - red/green/blue) must share ONE type; index
// 3, when present, is alpha and is DELIBERATELY excluded (scope-cut 3:
// "alpha is INDEPENDENT of that rule"). hsl()/hsla() never call this -
// hue is always <number>, saturation/lightness are always
// <percentage>, so there is no "uniform type" question there at all.
// Checks ONLY `components[index]` against `components[0]` - NOT every
// slot up to `index` at once - because a slot beyond `index` has not
// been read from the token stream yet and still holds its
// default-constructed (is_percentage == false) value, which would
// otherwise read as a FALSE type mismatch the moment `components[0]`
// happens to be a percentage.
bool rgb_component_type_matches_first(const std::array<color_component, 4> &components,
                                      std::size_t index) noexcept {
    return components[index].is_percentage == components[0].is_percentage;
}

gltfx_rgba8 rgb_components_to_rgba8(const std::array<color_component, 4> &components,
                                    bool has_alpha) noexcept {
    return gltfx_rgba8{
        .red = rgb_component_to_byte(components[0].value, components[0].is_percentage),
        .green = rgb_component_to_byte(components[1].value, components[1].is_percentage),
        .blue = rgb_component_to_byte(components[2].value, components[2].is_percentage),
        .alpha = has_alpha
                     ? alpha_component_to_byte(components[3].value, components[3].is_percentage)
                     : std::uint8_t{255},
    };
}

gltfx_rgba8 hsl_components_to_rgba8(const std::array<color_component, 4> &components,
                                    bool has_alpha) noexcept {
    const rgb_unit unit = hsl_to_rgb_unit(components[0].value, components[1].value / 100.0,
                                          components[2].value / 100.0);
    return gltfx_rgba8{
        .red = clamp_unit_to_byte(unit.red),
        .green = clamp_unit_to_byte(unit.green),
        .blue = clamp_unit_to_byte(unit.blue),
        .alpha = has_alpha
                     ? alpha_component_to_byte(components[3].value, components[3].is_percentage)
                     : std::uint8_t{255},
    };
}

// Reads every argument of an already-recognized rgb()/rgba()/hsl()/
// hsla() call - `cursor` is positioned right after the opening '(' the
// <function-token> itself consumed (tokenizer.hpp's own convention,
// same as consume_url_or_function_token in tokenizer.cpp). One
// function, one grammar shape (`<comp>, <comp>, <comp> [, <alpha>] )`)
// shared by all four names - the SLOT-specific pieces (which
// diagnostic a wrong token kind raises, whether the three color
// components must share a type, which formula turns them into bytes)
// are the only things that vary, and each is a parameter or a
// dedicated helper above, never a fourth copy of this loop.
color_parse_result parse_function_arguments(gltfx_gfss_cursor &cursor,
                                            color_function_kind kind) noexcept {
    const bool is_hsl_family =
        kind == color_function_kind::hsl || kind == color_function_kind::hsla;
    const bool has_alpha = kind == color_function_kind::rgba || kind == color_function_kind::hsla;
    const std::size_t required_args = has_alpha ? 4 : 3;

    std::array<color_component, 4> components{};
    for (std::size_t i = 0; i < required_args; ++i) {
        const bool is_hue = is_hsl_family && i == 0;
        const bool is_saturation_or_lightness = is_hsl_family && (i == 1 || i == 2);
        const component_kind_requirement requirement =
            is_hue                       ? component_kind_requirement::number_only
            : is_saturation_or_lightness ? component_kind_requirement::percentage_only
                                         : component_kind_requirement::number_or_percentage;

        color_component_read read{};
        if (!read_color_component(cursor, requirement, read)) {
            return fail(read.failure);
        }
        components[i] = read.component;
        if (!is_hsl_family && i > 0 && i < 3 && !rgb_component_type_matches_first(components, i)) {
            return fail(make_diagnostic(read.token, k_color_expected_uniform_component_types));
        }
        gltfx_gfss_diagnostic separator_failure{};
        if (!read_component_separator(cursor, i, required_args, separator_failure)) {
            return fail(separator_failure);
        }
    }

    return succeed(is_hsl_family ? hsl_components_to_rgba8(components, has_alpha)
                                 : rgb_components_to_rgba8(components, has_alpha));
}

// --- oklch() grammar ---------------------------------------------------
//
// oklch() ::= oklch( <L> <C> <H> [ '/' <alpha> ]? )  (ESCOPO.md SS4
// decision 7/8: the standard's OWN grammar - whitespace-separated, not
// the legacy comma family parse_function_arguments() above serves. CSS
// Color 4 never defined a comma form for this notation, so there is no
// "legacy vs modern" choice to make the way color_parse.hpp's own
// scope-cut 1 had to for rgb()/hsl().)
//
// THREE V1 CUTS, MARKED HERE AS THIS FATIA'S OWN INFERENCE (GODS_LAWS.md
// L-27 - the service order that opened GFSS-OKLCH names the conversion
// and the gamut mapping, never this grammar's own edge shape):
//   1. NO `none` KEYWORD - no other notation in this file accepts it
//      either (color_parse.hpp's own six scope-cuts never mention it),
//      so honoring it here would be the FIRST use of a CSS Color 4
//      keyword nothing else in this track recognizes yet.
//   2. NO `oklch(from ...)` RELATIVE COLOR SYNTAX - a distinct, far
//      larger CSS Color 5 feature (reading and re-deriving an ORIGIN
//      color), out of scope for a single conversion-and-gamut-mapping
//      fatia.
//   3. CHROMA IS A BARE <number> ONLY, no <percentage> - CSS Color 4's
//      own percentage reference range for chroma (100% = 0.4) is a
//      SEPARATE scaling constant this fatia's own service order never
//      named, unlike lightness/alpha, whose percentage meaning (0% =
//      0, 100% = 1) is already unambiguous and already established
//      elsewhere in this exact file.
//
// HUE IS UNRESTRICTED - the SAME "wraps, is never rejected" convention
// hsl_to_rgb_unit()'s own header comment above already documents for
// hsl()'s hue. NOT a v1 cut, a deliberate consistency choice: CSS
// Color 4 defines hue as a plain periodic <number> for every
// hue-bearing notation it has, hsl() included.
// Reads oklch()'s own OPTIONAL tail - `/ <alpha>` then ')', or just
// ')' - one question: "how much alpha, and did the call end cleanly?"
// `out_alpha_unit` keeps its caller-supplied default (fully opaque)
// unless a slash was actually present. Split out of
// parse_oklch_arguments() below so that function reads as ONE
// straight-line sequence of "read component, read component, read
// component, read the tail" (CONTRACT.md SS6.2's own function-length
// ceiling, GODS_LAWS.md L-17).
bool read_oklch_alpha_and_closing_paren(gltfx_gfss_cursor &cursor,
                                        const gltfx_gfss_token &separator, double &out_alpha_unit,
                                        gltfx_gfss_diagnostic &out_failure) noexcept {
    if (separator.kind == gltfx_gfss_token_kind::close_paren) {
        return true;
    }
    if (separator.kind != gltfx_gfss_token_kind::delim || separator.lexeme != "/") {
        out_failure = make_diagnostic(separator, k_color_expected_slash_or_closing_parenthesis);
        return false;
    }
    color_component_read alpha_read{};
    if (!read_color_component(cursor, component_kind_requirement::number_or_percentage,
                              alpha_read)) {
        out_failure = alpha_read.failure;
        return false;
    }
    out_alpha_unit =
        std::clamp(alpha_read.component.is_percentage ? alpha_read.component.value / 100.0
                                                      : alpha_read.component.value,
                   0.0, 1.0);
    gltfx_gfss_token closing{};
    if (!next_significant_token(cursor, closing) ||
        closing.kind != gltfx_gfss_token_kind::close_paren) {
        out_failure = make_diagnostic(closing, k_color_expected_closing_parenthesis);
        return false;
    }
    return true;
}

color_parse_result parse_oklch_arguments(gltfx_gfss_cursor &cursor) noexcept {
    color_component_read lightness_read{};
    if (!read_color_component(cursor, component_kind_requirement::number_or_percentage,
                              lightness_read)) {
        return fail(lightness_read.failure);
    }
    color_component_read chroma_read{};
    if (!read_color_component(cursor, component_kind_requirement::number_only, chroma_read)) {
        return fail(chroma_read.failure);
    }
    color_component_read hue_read{};
    if (!read_color_component(cursor, component_kind_requirement::number_only, hue_read)) {
        return fail(hue_read.failure);
    }

    gltfx_gfss_token separator{};
    if (!next_significant_token(cursor, separator)) {
        return fail_at(separator, k_color_expected_slash_or_closing_parenthesis);
    }
    double alpha_unit = 1.0;
    gltfx_gfss_diagnostic alpha_failure{};
    if (!read_oklch_alpha_and_closing_paren(cursor, separator, alpha_unit, alpha_failure)) {
        return fail(alpha_failure);
    }

    const double lightness_unit = lightness_read.component.is_percentage
                                      ? lightness_read.component.value / 100.0
                                      : lightness_read.component.value;
    const gltfx_rgba mapped =
        oklch_to_gltfx_rgba(lightness_unit, chroma_read.component.value, hue_read.component.value);
    return succeed_rgba(gltfx_rgba{.red = mapped.red,
                                   .green = mapped.green,
                                   .blue = mapped.blue,
                                   .alpha = static_cast<float>(alpha_unit)});
}

color_parse_result parse_function_color(gltfx_gfss_cursor &cursor,
                                        const gltfx_gfss_token &function_token) noexcept {
    const std::string_view name = function_token.lexeme.substr(0, function_token.lexeme.size() - 1);

    if (ascii_case_insensitive_equal("oklch", name)) {
        return parse_oklch_arguments(cursor);
    }

    color_function_kind kind{};
    if (!try_match_color_function_kind(name, kind)) {
        return fail_at(function_token, is_deferred_color_notation(name)
                                           ? k_color_expected_shipped_color_notation
                                           : k_color_expected_known_color_function);
    }
    return parse_function_arguments(cursor, kind);
}

} // namespace

color_parse_result parse_color(std::string_view text) noexcept {
    gltfx_gfss_cursor cursor{.source = text};
    gltfx_gfss_token token{};
    if (!next_significant_token(cursor, token)) {
        return fail_at(token, k_color_expected_color_value);
    }

    // A <bad-string-token>/<bad-url-token> already carries a diagnostic
    // from tokenizer.cpp's own vocabulary (diagnostic_vocabulary.hpp) -
    // propagated as-is rather than masked behind this file's generic
    // "not a color value" identifier, the same layering principle
    // token.hpp's own header comment documents for these two recovery
    // classes.
    if (token.kind == gltfx_gfss_token_kind::bad_string ||
        token.kind == gltfx_gfss_token_kind::bad_url) {
        return fail(token.diagnostic);
    }

    color_parse_result value{};
    if (token.kind == gltfx_gfss_token_kind::hash) {
        value = parse_hex_color(token);
    } else if (token.kind == gltfx_gfss_token_kind::ident) {
        value = parse_named_color(token);
    } else if (token.kind == gltfx_gfss_token_kind::function) {
        value = parse_function_color(cursor, token);
    } else {
        return fail_at(token, k_color_expected_color_value);
    }
    if (!value.ok) {
        return value;
    }

    gltfx_gfss_token trailing{};
    if (next_significant_token(cursor, trailing)) {
        return fail_at(trailing, k_color_expected_no_trailing_content);
    }
    return value;
}

} // namespace glintfx::style::detail
