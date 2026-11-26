// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string_view>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/light_tint.hpp>
#include <glintfx/core/vec2.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// light_tint_test.cpp - R2D-SPRITE-SHADE, H4 (docs/auditoria-api-sprite-shade.md, section 2.1;
// decisions D-W8-325, D-W8-355, D-W8-380, D-W8-397, and the fix1 decisions D-W8-421 to D-W8-426;
// GODS_LAWS.md L-17, L-20, L-21, L-40).
//
// The expected values are FIXED here, written before the implementation existed. They come from
// the parecer's table (section 2.1) and from /var/tmp/cto-w8/ordens-anexos/h4/contas/contas_h4.py,
// an independent computation outside the library, never from the program's output. Every value is
// a float the conversion gives exactly (0.5, 0.5625, 0.7F as 0.2F plus 0.5, ...), so the cases
// compare with ==.
//
// LIGHT HAS NO OPACITY (D-W8-397): every tint leaves with alpha 1, and the alpha of the ambient
// and of each colour is not read, so the three-value colour gltfx_rgba{0.2F, 0.2F, 0.2F} (alpha 0)
// is a valid ambient.
//
// The refusal table changes ONE field per cell: the public contract does not promise which field
// is named when several are out of range, so no cell depends on that order. Each refused cell also
// proves that nothing was written. The table prints its count, and a count different from the one
// written here fails (L-40).

namespace {

using glintfx::gltfx_err_code;
using glintfx::gltfx_light_falloff;
using glintfx::gltfx_light_source_2d;
using glintfx::gltfx_light_tint;
using glintfx::gltfx_rgba;
using glintfx::gltfx_rslt;
using glintfx::gltfx_vec2_world;

constexpr double k_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double k_inf = std::numeric_limits<double>::infinity();
constexpr float k_nan_f = std::numeric_limits<float>::quiet_NaN();
constexpr float k_inf_f = std::numeric_limits<float>::infinity();

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};
constexpr gltfx_rgba k_dark{0.0F, 0.0F, 0.0F, 0.0F};
// What a refused call must leave in out_tints: a value the function never writes.
constexpr gltfx_rgba k_untouched{0.125F, 0.25F, 0.375F, 0.625F};

// A light at (x, y) with range 10, white, intensity 1, and the law `falloff`.
gltfx_light_source_2d lamp(double x, double y, gltfx_light_falloff falloff) {
    return gltfx_light_source_2d{.position = {.x = x, .y = y},
                                 .range = 10.0,
                                 .color = k_white,
                                 .intensity = 1.0,
                                 .falloff = falloff};
}

bool same_rgba(const gltfx_rgba &got, const gltfx_rgba &want) {
    return got.red == want.red && got.green == want.green && got.blue == want.blue &&
           got.alpha == want.alpha;
}

void expect_rgba(const gltfx_rgba &got, const gltfx_rgba &want) {
    GLINTFX_CHECK(got.red == want.red);
    GLINTFX_CHECK(got.green == want.green);
    GLINTFX_CHECK(got.blue == want.blue);
    GLINTFX_CHECK(got.alpha == want.alpha);
}

// The tint of ONE vertex, or the untouched value when the call is refused (the case then fails).
gltfx_rgba tint_of(gltfx_vec2_world vertex, std::span<const gltfx_light_source_2d> lights,
                   gltfx_rgba ambient) {
    std::array<gltfx_vec2_world, 1> vertices{vertex};
    std::array<gltfx_rgba, 1> tints{k_untouched};
    const gltfx_rslt<void> result = gltfx_light_tint(vertices, lights, ambient, tints);
    GLINTFX_CHECK(result.has_value());
    return gltfx_rgba{tints[0].red, tints[0].green, tints[0].blue, tints[0].alpha};
}

gltfx_rgba tint_of_one(gltfx_vec2_world vertex, const gltfx_light_source_2d &light,
                       gltfx_rgba ambient) {
    const std::array<gltfx_light_source_2d, 1> lights{light};
    return tint_of(vertex, lights, ambient);
}

gltfx_rgba grey(float value) { return gltfx_rgba{value, value, value, 1.0F}; }

} // namespace

GLINTFX_TEST(light_tint_linear_at_half_the_range_gives_one_half) {
    expect_rgba(
        tint_of_one({.x = 5.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::linear), k_dark),
        grey(0.5F));
}

GLINTFX_TEST(light_tint_smooth_at_half_the_range_gives_nine_sixteenths) {
    expect_rgba(
        tint_of_one({.x = 5.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::smooth), k_dark),
        grey(0.5625F));
}

GLINTFX_TEST(light_tint_at_the_light_is_full_for_both_laws) {
    expect_rgba(
        tint_of_one({.x = 0.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::linear), k_dark),
        grey(1.0F));
    expect_rgba(
        tint_of_one({.x = 0.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::smooth), k_dark),
        grey(1.0F));
}

GLINTFX_TEST(light_tint_at_and_beyond_the_range_is_zero_for_both_laws) {
    for (const gltfx_light_falloff law :
         {gltfx_light_falloff::linear, gltfx_light_falloff::smooth}) {
        expect_rgba(tint_of_one({.x = 10.0, .y = 0.0}, lamp(0.0, 0.0, law), k_dark), grey(0.0F));
        expect_rgba(tint_of_one({.x = 12.0, .y = 0.0}, lamp(0.0, 0.0, law), k_dark), grey(0.0F));
    }
}

GLINTFX_TEST(light_tint_ambient_is_added_before_the_clamp) {
    // Alpha 0, what the three-value colour gltfx_rgba{0.2F, 0.2F, 0.2F} gives (written out because
    // -Wextra warns on the missing field): it is not read.
    const gltfx_rgba ambient{0.2F, 0.2F, 0.2F, 0.0F};
    expect_rgba(
        tint_of_one({.x = 5.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::linear), ambient),
        gltfx_rgba{0.7F, 0.7F, 0.7F, 1.0F});
}

GLINTFX_TEST(light_tint_two_lights_of_six_tenths_saturate_at_one) {
    gltfx_light_source_2d first = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    first.intensity = 0.6;
    const std::array<gltfx_light_source_2d, 2> lights{first, first};
    expect_rgba(tint_of({.x = 0.0, .y = 0.0}, lights, k_dark), grey(1.0F));
}

GLINTFX_TEST(light_tint_with_no_light_is_the_ambient) {
    const gltfx_rgba ambient{0.25F, 0.5F, 0.75F, 0.4F};
    expect_rgba(tint_of({.x = 1.0, .y = 2.0}, {}, ambient), gltfx_rgba{0.25F, 0.5F, 0.75F, 1.0F});
}

GLINTFX_TEST(light_tint_distance_is_measured_on_the_plane) {
    // The light at (3, 4) and the vertex at (0, 0): d = 5 (hypot), half the range.
    expect_rgba(
        tint_of_one({.x = 0.0, .y = 0.0}, lamp(3.0, 4.0, gltfx_light_falloff::linear), k_dark),
        grey(0.5F));
}

GLINTFX_TEST(light_tint_is_colour_times_intensity_per_channel) {
    gltfx_light_source_2d light = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    light.color = gltfx_rgba{1.0F, 0.5F, 0.0F, 1.0F};
    light.intensity = 2.0;
    expect_rgba(tint_of_one({.x = 5.0, .y = 0.0}, light, k_dark),
                gltfx_rgba{1.0F, 0.5F, 0.0F, 1.0F});
}

// Neither the ambient's alpha nor the light colour's alpha reaches the tint: it is 1 for an alpha
// of 0.25 in the ambient and of 0.5 in the light, and for both alphas at 0 (D-W8-397).
GLINTFX_TEST(light_tint_alpha_is_always_one_whatever_the_input_alphas) {
    gltfx_light_source_2d light = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    light.color = gltfx_rgba{1.0F, 1.0F, 1.0F, 0.5F};
    expect_rgba(tint_of_one({.x = 5.0, .y = 0.0}, light, gltfx_rgba{0.0F, 0.0F, 0.0F, 0.25F}),
                grey(0.5F));
    light.color = gltfx_rgba{1.0F, 1.0F, 1.0F, 0.0F};
    expect_rgba(tint_of_one({.x = 5.0, .y = 0.0}, light, k_dark), grey(0.5F));
}

GLINTFX_TEST(light_tint_writes_each_vertex_in_its_own_place) {
    const std::array<gltfx_vec2_world, 3> vertices{gltfx_vec2_world{.x = 0.0, .y = 0.0},
                                                   gltfx_vec2_world{.x = 5.0, .y = 0.0},
                                                   gltfx_vec2_world{.x = 10.0, .y = 0.0}};
    const std::array<gltfx_light_source_2d, 1> lights{lamp(0.0, 0.0, gltfx_light_falloff::linear)};
    std::array<gltfx_rgba, 3> tints{k_untouched, k_untouched, k_untouched};
    GLINTFX_CHECK(gltfx_light_tint(vertices, lights, k_dark, tints).has_value());
    expect_rgba(tints[0], grey(1.0F));
    expect_rgba(tints[1], grey(0.5F));
    expect_rgba(tints[2], grey(0.0F));
}

GLINTFX_TEST(light_tint_of_no_vertex_is_accepted) {
    const std::array<gltfx_light_source_2d, 1> lights{lamp(0.0, 0.0, gltfx_light_falloff::linear)};
    GLINTFX_CHECK(gltfx_light_tint({}, lights, k_dark, {}).has_value());
}

GLINTFX_TEST(light_tint_ambient_above_one_is_clamped) {
    expect_rgba(tint_of({.x = 0.0, .y = 0.0}, {}, gltfx_rgba{2.0F, 1.5F, 0.5F, 1.0F}),
                gltfx_rgba{1.0F, 1.0F, 0.5F, 1.0F});
}

// The largest colour times the largest intensity is infinity. Out of range, f is 0, and the tint
// must be 0, never NaN: the reach (intensity * f) is computed before the colour (D-W8-380).
GLINTFX_TEST(light_tint_overflowing_light_gives_zero_out_of_range_and_one_in_range) {
    gltfx_light_source_2d light = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    light.color = gltfx_rgba{3.0e38F, 0.0F, 0.0F, 1.0F};
    light.intensity = 1.0e308;
    expect_rgba(tint_of_one({.x = 12.0, .y = 0.0}, light, k_dark), grey(0.0F));
    expect_rgba(tint_of_one({.x = 5.0, .y = 0.0}, light, k_dark),
                gltfx_rgba{1.0F, 0.0F, 0.0F, 1.0F});
}

GLINTFX_TEST(light_tint_tiny_range_stays_finite) {
    gltfx_light_source_2d light = lamp(0.0, 0.0, gltfx_light_falloff::smooth);
    light.range = 1.0e-300;
    expect_rgba(tint_of_one({.x = 1.0, .y = 0.0}, light, k_dark), grey(0.0F));
    expect_rgba(tint_of_one({.x = 0.0, .y = 0.0}, light, k_dark), grey(1.0F));
}

// Beyond the range the linear law is 0, never negative: an ambient of 0.2 stays 0.2 (fix1, I-5).
GLINTFX_TEST(light_tint_linear_law_beyond_the_range_takes_nothing_from_the_ambient) {
    const gltfx_rgba ambient{0.2F, 0.2F, 0.2F, 0.0F};
    expect_rgba(
        tint_of_one({.x = 12.0, .y = 0.0}, lamp(0.0, 0.0, gltfx_light_falloff::linear), ambient),
        grey(0.2F));
}

// The distance does not overflow on the way (std::hypot): a vertex at 1e200 under a light of
// range 1e308 is at a hundred-millionth of the range, and takes the full light (fix1, I-6).
GLINTFX_TEST(light_tint_very_far_vertex_keeps_its_distance_on_the_plane) {
    gltfx_light_source_2d light = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    light.range = 1.0e308;
    expect_rgba(tint_of_one({.x = 1.0e200, .y = 0.0}, light, k_dark), grey(1.0F));
}

// Every vertex is checked, not only the first, and a refusal found after the first vertex still
// writes nothing (fix1, I-3 and I-4): three vertices, the bad one last.
GLINTFX_TEST(light_tint_a_bad_later_vertex_is_refused_and_nothing_is_written) {
    const std::array<gltfx_vec2_world, 3> vertices{gltfx_vec2_world{.x = 5.0, .y = 0.0},
                                                   gltfx_vec2_world{.x = 1.0, .y = 0.0},
                                                   gltfx_vec2_world{.x = k_nan, .y = 0.0}};
    const std::array<gltfx_light_source_2d, 1> lights{lamp(0.0, 0.0, gltfx_light_falloff::linear)};
    std::array<gltfx_rgba, 3> tints{k_untouched, k_untouched, k_untouched};
    const gltfx_rslt<void> result = gltfx_light_tint(vertices, lights, k_dark, tints);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.has_error() && result.err().rejected_value() == "vertices");
    for (const gltfx_rgba &tint : tints) {
        GLINTFX_CHECK(same_rgba(tint, k_untouched));
    }
}

// A refusal of a light writes nothing either, over several good vertices (fix1, I-4).
GLINTFX_TEST(light_tint_a_bad_light_over_good_vertices_writes_nothing) {
    const std::array<gltfx_vec2_world, 2> vertices{gltfx_vec2_world{.x = 5.0, .y = 0.0},
                                                   gltfx_vec2_world{.x = 1.0, .y = 0.0}};
    gltfx_light_source_2d bad = lamp(0.0, 0.0, gltfx_light_falloff::linear);
    bad.range = 0.0;
    const std::array<gltfx_light_source_2d, 2> lights{lamp(0.0, 0.0, gltfx_light_falloff::linear),
                                                      bad};
    std::array<gltfx_rgba, 2> tints{k_untouched, k_untouched};
    const gltfx_rslt<void> result = gltfx_light_tint(vertices, lights, k_dark, tints);
    GLINTFX_CHECK(result.has_error() && result.err().rejected_value() == "range");
    GLINTFX_CHECK(same_rgba(tints[0], k_untouched));
    GLINTFX_CHECK(same_rgba(tints[1], k_untouched));
}

// THE CLOSED TABLE OF THE INPUT CHECK. Each cell breaks one field (or none, for the accepted cells
// at the edge of each range), calls the function over one vertex and one or two lights, and states
// the field it expects named, or "" for an accepted call. The cells are grouped by the field they
// break, one function per group (fix1, I-7: no function past 40 lines, CONTRACT.md 6.2).
namespace {

// How many out_tints a cell passes for its one vertex.
enum class tint_room : std::uint8_t { exact, shorter, longer };

struct check_cell {
    const char *label = "";
    gltfx_vec2_world vertex{};
    gltfx_rgba ambient{};
    gltfx_light_source_2d first{};
    gltfx_light_source_2d second{}; // used when `two_lights`
    bool two_lights = false;
    tint_room room = tint_room::exact;
    std::string_view refused;
};

gltfx_light_source_2d good() { return lamp(0.0, 0.0, gltfx_light_falloff::linear); }

gltfx_light_source_2d with_position(double x, double y) {
    gltfx_light_source_2d light = good();
    light.position = {.x = x, .y = y};
    return light;
}

gltfx_light_source_2d with_range(double range) {
    gltfx_light_source_2d light = good();
    light.range = range;
    return light;
}

gltfx_light_source_2d with_intensity(double intensity) {
    gltfx_light_source_2d light = good();
    light.intensity = intensity;
    return light;
}

gltfx_light_source_2d with_color(gltfx_rgba color) {
    gltfx_light_source_2d light = good();
    light.color = color;
    return light;
}

gltfx_light_source_2d with_falloff(gltfx_light_falloff falloff) {
    gltfx_light_source_2d light = good();
    light.falloff = falloff;
    return light;
}

constexpr gltfx_vec2_world k_vertex{.x = 5.0, .y = 0.0};

check_cell cell(const char *label, gltfx_light_source_2d first, std::string_view refused) {
    return check_cell{label, k_vertex, k_dark, first, good(), false, tint_room::exact, refused};
}

check_cell ambient_cell(const char *label, gltfx_rgba ambient, std::string_view refused) {
    return check_cell{label, k_vertex, ambient, good(), good(), false, tint_room::exact, refused};
}

check_cell vertex_cell(const char *label, gltfx_vec2_world vertex) {
    return check_cell{label, vertex, k_dark, good(), good(), false, tint_room::exact, "vertices"};
}

check_cell room_cell(const char *label, tint_room room) {
    return check_cell{label, k_vertex, k_dark, good(), good(), false, room, "out_tints"};
}

check_cell second_light_cell(const char *label, gltfx_light_source_2d second,
                             std::string_view refused) {
    return check_cell{label, k_vertex, k_dark, good(), second, true, tint_room::exact, refused};
}

// The out_tints of a cell: none, one or two places, all untouched before the call.
std::span<gltfx_rgba> tints_of(const check_cell &c, std::array<gltfx_rgba, 2> &tints) {
    switch (c.room) {
    case tint_room::shorter:
        return std::span<gltfx_rgba>(tints).first(0);
    case tint_room::longer:
        return std::span<gltfx_rgba>(tints);
    case tint_room::exact:
        break;
    }
    return std::span<gltfx_rgba>(tints).first(1);
}

// Runs one cell; true when the cell holds (the right name, or acceptance, and nothing written on a
// refusal).
bool cell_holds(const check_cell &c) {
    const std::array<gltfx_vec2_world, 1> vertices{c.vertex};
    const std::array<gltfx_light_source_2d, 2> both{c.first, c.second};
    const std::span<const gltfx_light_source_2d> lights =
        c.two_lights ? std::span<const gltfx_light_source_2d>(both)
                     : std::span<const gltfx_light_source_2d>(both).first(1);
    std::array<gltfx_rgba, 2> tints{k_untouched, k_untouched};
    const gltfx_rslt<void> result =
        gltfx_light_tint(vertices, lights, c.ambient, tints_of(c, tints));
    if (c.refused.empty()) {
        return result.has_value();
    }
    return result.has_error() && result.err().code() == gltfx_err_code::invalid_argument &&
           result.err().rejected_value() == c.refused && same_rgba(tints[0], k_untouched) &&
           same_rgba(tints[1], k_untouched);
}

// The size of out_tints and the vertex.
std::array<check_cell, 4> room_and_vertex_cells() {
    return {room_cell("out_tints shorter than vertices", tint_room::shorter),
            room_cell("out_tints longer than vertices", tint_room::longer),
            vertex_cell("vertex x NaN", {.x = k_nan, .y = 0.0}),
            vertex_cell("vertex y infinite", {.x = 0.0, .y = k_inf})};
}

// Each colour channel of the ambient, not finite and negative (fix1, I-1).
std::array<check_cell, 6> ambient_cells() {
    return {ambient_cell("ambient red NaN", {k_nan_f, 0.0F, 0.0F, 1.0F}, "ambient"),
            ambient_cell("ambient red negative", {-0.5F, 0.0F, 0.0F, 1.0F}, "ambient"),
            ambient_cell("ambient green negative", {0.0F, -0.1F, 0.0F, 1.0F}, "ambient"),
            ambient_cell("ambient green infinite", {0.0F, k_inf_f, 0.0F, 1.0F}, "ambient"),
            ambient_cell("ambient blue infinite", {0.0F, 0.0F, k_inf_f, 1.0F}, "ambient"),
            ambient_cell("ambient blue negative", {0.0F, 0.0F, -0.5F, 1.0F}, "ambient")};
}

// Each field of a light, and the second light (fix1, I-1 for the colour channels).
std::array<check_cell, 18> light_cells() {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: the cast is the case
    const auto out_of_enum = static_cast<gltfx_light_falloff>(7);
    return {cell("position x NaN", with_position(k_nan, 0.0), "position"),
            cell("position y minus infinity", with_position(0.0, -k_inf), "position"),
            cell("range zero", with_range(0.0), "range"),
            cell("range negative", with_range(-1.0), "range"),
            cell("range NaN", with_range(k_nan), "range"),
            cell("range infinite", with_range(k_inf), "range"),
            cell("intensity negative", with_intensity(-1.0), "intensity"),
            cell("intensity NaN", with_intensity(k_nan), "intensity"),
            cell("intensity infinite", with_intensity(k_inf), "intensity"),
            cell("colour red negative", with_color({-1.0F, 1.0F, 1.0F, 1.0F}), "color"),
            cell("colour red infinite", with_color({k_inf_f, 1.0F, 1.0F, 1.0F}), "color"),
            cell("colour green negative", with_color({1.0F, -1.0F, 1.0F, 1.0F}), "color"),
            cell("colour green infinite", with_color({1.0F, k_inf_f, 1.0F, 1.0F}), "color"),
            cell("colour blue NaN", with_color({1.0F, 1.0F, k_nan_f, 1.0F}), "color"),
            cell("colour blue negative", with_color({1.0F, 1.0F, -1.0F, 1.0F}), "color"),
            cell("falloff unspecified", with_falloff(gltfx_light_falloff::unspecified), "falloff"),
            cell("falloff out of the enumeration", with_falloff(out_of_enum), "falloff"),
            second_light_cell("second light with range zero", with_range(0.0), "range")};
}

// Accepted at the edge of each range, and the alphas that are not read (D-W8-397).
std::array<check_cell, 8> accepted_cells() {
    return {cell("intensity zero", with_intensity(0.0), ""),
            cell("colour zero", with_color(k_dark), ""),
            cell("colour above one", with_color({5.0F, 0.0F, 0.0F, 1.0F}), ""),
            cell("range tiny but positive", with_range(1.0e-300), ""),
            ambient_cell("ambient zero", k_dark, ""),
            second_light_cell("two good lights", good(), ""),
            ambient_cell("ambient alpha NaN, not read", {0.0F, 0.0F, 0.0F, k_nan_f}, ""),
            cell("colour alpha negative, not read", with_color({1.0F, 1.0F, 1.0F, -1.0F}), "")};
}

// The count of the table, as it is printed.
struct table_count {
    int refused = 0;
    int accepted = 0;
    int failures = 0;
};

void run_cells(std::span<const check_cell> cells, table_count &count) {
    for (const check_cell &c : cells) {
        if (!cell_holds(c)) {
            ++count.failures;
            std::printf("[FAIL CELL] %s\n", c.label);
        }
        ++(c.refused.empty() ? count.accepted : count.refused);
    }
}

} // namespace

GLINTFX_TEST(light_tint_closed_table_of_the_input_check) {
    table_count count;
    run_cells(room_and_vertex_cells(), count);
    run_cells(ambient_cells(), count);
    run_cells(light_cells(), count);
    run_cells(accepted_cells(), count);
    std::printf("celulas=%d recusadas=%d aceitas=%d falhas=%d\n", count.refused + count.accepted,
                count.refused, count.accepted, count.failures);
    GLINTFX_CHECK(count.refused == 28);
    GLINTFX_CHECK(count.accepted == 8);
    GLINTFX_CHECK(count.failures == 0);
}
