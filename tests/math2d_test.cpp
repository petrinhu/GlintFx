// SPDX-License-Identifier: AGPL-3.0-or-later
//
// math2d_test.cpp - CORE-MATH2D (TODO.md W4, GODS_LAWS.md
// L-19/L-20/L-26/L-40): the closed proof of the six pieces this slice
// freezes on the public surface - vector, matrix, rectangle,
// transform, angle and interpolation - across the project's two
// precision families.
//
// THE FOUR DECISIONS OF THE PROJECT LEADER THIS FILE GUARDS, taken
// 05/09/2026 via AskUserQuestion; each header carries its own full
// text, this list is the map from decision to the case that proves it:
//
//   1. Double precision in the world, single on the screen; the
//      matrix for the graphics card is born single and built once per
//      frame.
//      -> the_two_precision_families_never_convert_into_each_other
//      -> world_precision_survives_past_the_screen_precision_limit
//      -> crossing_to_screen_precision_is_deterministic_and_can_lose_exactness
//   2. An angle is its own type, carrying its unit, at no memory cost.
//      -> an_angle_never_accepts_a_bare_number
//      -> angle_round_trips_through_degrees_including_zero_negative_and_full_turn
//   3. A rectangle is a corner plus a size.
//      -> a_negative_rect_size_places_the_far_corner_before_the_near_one
//   4. The matrix is stored in the order the graphics card expects.
//      -> mat3_memory_order_is_column_major_as_the_graphics_card_expects
//
// TWO CLOSED ENUMERATIONS (GODS_LAWS.md L-40), each printing a count
// derived from the list it walks, never from a hand-written number:
// k_all_types (the seven frozen value types) and k_all_operations (the
// six public functions). Adding an eighth type or a seventh function
// means adding it to the matching array here, in lockstep - the same
// contract err_code_test.cpp's k_all_codes and time_test.cpp's
// k_all_operations already carry.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <print>
#include <type_traits>

#include <glintfx/core/angle.hpp>
#include <glintfx/core/interpolate.hpp>
#include <glintfx/core/mat3.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/core/vec2.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

namespace {

// The closed set of value types CORE-MATH2D freezes.
enum class math2d_type : std::uint8_t {
    vec2_world,
    vec2_screen,
    rect_world,
    rect_screen,
    angle,
    transform,
    mat3,
};

constexpr std::array<math2d_type, 7> k_all_types{
    math2d_type::vec2_world,  math2d_type::vec2_screen, math2d_type::rect_world,
    math2d_type::rect_screen, math2d_type::angle,       math2d_type::transform,
    math2d_type::mat3,
};

// The closed set of public functions CORE-MATH2D freezes.
enum class math2d_operation : std::uint8_t {
    angle_from_degrees,
    angle_to_degrees,
    vec2_world_to_screen,
    vec2_screen_to_world,
    mat3_from_transform,
    lerp,
};

constexpr std::array<math2d_operation, 6> k_all_operations{
    math2d_operation::angle_from_degrees,   math2d_operation::angle_to_degrees,
    math2d_operation::vec2_world_to_screen, math2d_operation::vec2_screen_to_world,
    math2d_operation::mat3_from_transform,  math2d_operation::lerp,
};

// The exactness limit of single precision: 2^24. Every whole number up
// to and including this one is representable as a float; the next one
// is not. Measured live before being written down (see
// core/vec2.hpp's own header comment), never taken from memory.
constexpr double k_screen_precision_whole_number_limit = 16'777'216.0;

// Reads the element at (row, column) of a matrix stored column by
// column - the convention core/mat3.hpp freezes. Local to this test on
// purpose: it is NOT part of the public surface, and writing the
// index arithmetic out here is what makes the convention checkable
// rather than assumed.
[[nodiscard]] float element_at(const glintfx::gltfx_mat3 &m, std::size_t row, std::size_t column) {
    return m.column_major[column * 3U + row];
}

[[nodiscard]] bool close_enough(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

GLINTFX_TEST(every_public_math2d_type_has_the_frozen_layout) {
    // GODS_LAWS.md L-19: a core value type's VISIBLE, STABLE LAYOUT is
    // its contract, so the layout is what a test has to pin. L-40: the
    // count printed below is derived from the loop that actually
    // walked the list, never from k_all_types.size() alone.
    std::size_t checked = 0;

    for (const math2d_type t : k_all_types) {
        switch (t) {
        case math2d_type::vec2_world:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_vec2_world>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_vec2_world>);
            static_assert(sizeof(glintfx::gltfx_vec2_world) == 2 * sizeof(double));
            static_assert(offsetof(glintfx::gltfx_vec2_world, x) == 0);
            static_assert(offsetof(glintfx::gltfx_vec2_world, y) == sizeof(double));
            static_assert(std::is_same_v<decltype(glintfx::gltfx_vec2_world::x), double>);
            ++checked;
            break;
        case math2d_type::vec2_screen:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_vec2_screen>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_vec2_screen>);
            static_assert(sizeof(glintfx::gltfx_vec2_screen) == 2 * sizeof(float));
            static_assert(offsetof(glintfx::gltfx_vec2_screen, x) == 0);
            static_assert(offsetof(glintfx::gltfx_vec2_screen, y) == sizeof(float));
            static_assert(std::is_same_v<decltype(glintfx::gltfx_vec2_screen::x), float>);
            ++checked;
            break;
        case math2d_type::rect_world:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_rect_world>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_rect_world>);
            static_assert(sizeof(glintfx::gltfx_rect_world) ==
                          2 * sizeof(glintfx::gltfx_vec2_world));
            static_assert(offsetof(glintfx::gltfx_rect_world, corner) == 0);
            static_assert(offsetof(glintfx::gltfx_rect_world, size) ==
                          sizeof(glintfx::gltfx_vec2_world));
            ++checked;
            break;
        case math2d_type::rect_screen:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_rect_screen>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_rect_screen>);
            static_assert(sizeof(glintfx::gltfx_rect_screen) ==
                          2 * sizeof(glintfx::gltfx_vec2_screen));
            static_assert(offsetof(glintfx::gltfx_rect_screen, corner) == 0);
            static_assert(offsetof(glintfx::gltfx_rect_screen, size) ==
                          sizeof(glintfx::gltfx_vec2_screen));
            ++checked;
            break;
        case math2d_type::angle:
            // The leader's second decision, half two: the type must
            // not cost more memory than the number it wraps.
            static_assert(std::is_standard_layout_v<glintfx::gltfx_angle>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_angle>);
            static_assert(sizeof(glintfx::gltfx_angle) == sizeof(double));
            static_assert(alignof(glintfx::gltfx_angle) == alignof(double));
            static_assert(offsetof(glintfx::gltfx_angle, radians) == 0);
            ++checked;
            break;
        case math2d_type::transform:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_transform>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_transform>);
            static_assert(offsetof(glintfx::gltfx_transform, translation) == 0);
            static_assert(offsetof(glintfx::gltfx_transform, rotation) ==
                          sizeof(glintfx::gltfx_vec2_world));
            static_assert(offsetof(glintfx::gltfx_transform, scale) ==
                          sizeof(glintfx::gltfx_vec2_world) + sizeof(glintfx::gltfx_angle));
            ++checked;
            break;
        case math2d_type::mat3:
            static_assert(std::is_standard_layout_v<glintfx::gltfx_mat3>);
            static_assert(std::is_trivially_copyable_v<glintfx::gltfx_mat3>);
            // Nine floats and nothing else - no padding, no tag. This
            // is what lets the array be handed straight to the driver.
            static_assert(sizeof(glintfx::gltfx_mat3) == 9 * sizeof(float));
            static_assert(offsetof(glintfx::gltfx_mat3, column_major) == 0);
            ++checked;
            break;
        }
    }

    GLINTFX_CHECK_EQ(checked, k_all_types.size());
    std::println("every_public_math2d_type_has_the_frozen_layout: {} of {} frozen type(s) checked",
                 checked, k_all_types.size());
}

GLINTFX_TEST(every_public_math2d_operation_is_exercised) {
    std::size_t exercised = 0;

    for (const math2d_operation op : k_all_operations) {
        switch (op) {
        case math2d_operation::angle_from_degrees: {
            const glintfx::gltfx_angle a = glintfx::gltfx_angle_from_degrees(180.0);
            GLINTFX_CHECK(close_enough(a.radians, std::numbers::pi, 1e-12));
            ++exercised;
            break;
        }
        case math2d_operation::angle_to_degrees: {
            const double degrees =
                glintfx::gltfx_angle_to_degrees(glintfx::gltfx_angle{.radians = std::numbers::pi});
            GLINTFX_CHECK(close_enough(degrees, 180.0, 1e-12));
            ++exercised;
            break;
        }
        case math2d_operation::vec2_world_to_screen: {
            const glintfx::gltfx_vec2_screen s =
                glintfx::gltfx_vec2_world_to_screen(glintfx::gltfx_vec2_world{.x = 3.0, .y = -4.0});
            GLINTFX_CHECK_EQ(s.x, 3.0F);
            GLINTFX_CHECK_EQ(s.y, -4.0F);
            ++exercised;
            break;
        }
        case math2d_operation::vec2_screen_to_world: {
            const glintfx::gltfx_vec2_world w = glintfx::gltfx_vec2_screen_to_world(
                glintfx::gltfx_vec2_screen{.x = 3.0F, .y = -4.0F});
            GLINTFX_CHECK_EQ(w.x, 3.0);
            GLINTFX_CHECK_EQ(w.y, -4.0);
            ++exercised;
            break;
        }
        case math2d_operation::mat3_from_transform: {
            const glintfx::gltfx_mat3 m =
                glintfx::gltfx_mat3_from_transform(glintfx::gltfx_transform{
                    .translation = {.x = 7.0, .y = 9.0},
                    .rotation = {.radians = 0.0},
                    .scale = {.x = 1.0, .y = 1.0},
                });
            GLINTFX_CHECK_EQ(element_at(m, 0, 2), 7.0F);
            GLINTFX_CHECK_EQ(element_at(m, 1, 2), 9.0F);
            ++exercised;
            break;
        }
        case math2d_operation::lerp: {
            GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(0.0, 10.0, 0.5), 5.0);
            ++exercised;
            break;
        }
        }
    }

    GLINTFX_CHECK_EQ(exercised, k_all_operations.size());
    std::println(
        "every_public_math2d_operation_is_exercised: {} of {} public operation(s) exercised",
        exercised, k_all_operations.size());
}

GLINTFX_TEST(the_two_precision_families_never_convert_into_each_other) {
    // The leader was warned that two families on the public surface
    // only work if the boundary is obvious, and that an ambiguous
    // boundary turns the decision itself into a defect. This is the
    // compile-time half of that guarantee: neither family converts
    // into the other, in either direction, so a value cannot cross by
    // accident - only through a named call.
    static_assert(!std::is_convertible_v<glintfx::gltfx_vec2_world, glintfx::gltfx_vec2_screen>);
    static_assert(!std::is_convertible_v<glintfx::gltfx_vec2_screen, glintfx::gltfx_vec2_world>);
    static_assert(!std::is_convertible_v<glintfx::gltfx_rect_world, glintfx::gltfx_rect_screen>);
    static_assert(!std::is_convertible_v<glintfx::gltfx_rect_screen, glintfx::gltfx_rect_world>);
    // And the two families really are the two precisions the decision
    // names - not two spellings of the same thing.
    static_assert(std::is_same_v<decltype(glintfx::gltfx_vec2_world::x), double>);
    static_assert(std::is_same_v<decltype(glintfx::gltfx_vec2_screen::x), float>);
    static_assert(!std::is_same_v<glintfx::gltfx_vec2_world, glintfx::gltfx_vec2_screen>);

    // The runtime half: a value that crosses does so through the named
    // call and comes back unchanged for a magnitude both precisions
    // hold exactly.
    const glintfx::gltfx_vec2_world original{.x = 1.5, .y = -2.25};
    const glintfx::gltfx_vec2_world back =
        glintfx::gltfx_vec2_screen_to_world(glintfx::gltfx_vec2_world_to_screen(original));
    GLINTFX_CHECK_EQ(back.x, original.x);
    GLINTFX_CHECK_EQ(back.y, original.y);
}

GLINTFX_TEST(an_angle_never_accepts_a_bare_number) {
    // The leader's second decision, half one: passing a bare number
    // where an angle is expected must not compile. There is no
    // conversion in either direction, so neither a raw double nor a
    // raw count of degrees can slip into an angle parameter.
    static_assert(!std::is_convertible_v<double, glintfx::gltfx_angle>);
    static_assert(!std::is_convertible_v<glintfx::gltfx_angle, double>);
    static_assert(!std::is_convertible_v<int, glintfx::gltfx_angle>);
    static_assert(!std::is_convertible_v<float, glintfx::gltfx_angle>);

    // What DOES work is the named, unit-carrying construction.
    const glintfx::gltfx_angle quarter_turn = glintfx::gltfx_angle_from_degrees(90.0);
    GLINTFX_CHECK(close_enough(quarter_turn.radians, std::numbers::pi / 2.0, 1e-12));
}

GLINTFX_TEST(angle_round_trips_through_degrees_including_zero_negative_and_full_turn) {
    // Boundary cases, not a happy path: zero, negative, a full turn,
    // and two full turns - the last one proving that nothing is
    // silently normalized into [0, 360), which would destroy the
    // difference between "spins twice" and "does not move".
    constexpr std::array<double, 6> k_degrees{0.0, 90.0, -90.0, 180.0, 360.0, 720.0};

    std::size_t round_trips = 0;
    for (const double degrees : k_degrees) {
        const glintfx::gltfx_angle a = glintfx::gltfx_angle_from_degrees(degrees);
        const double back = glintfx::gltfx_angle_to_degrees(a);
        // Tolerance scaled by magnitude: the round trip multiplies and
        // divides by an irrational constant, so an absolute tolerance
        // that fits 0 would be dishonestly tight at 720.
        GLINTFX_CHECK(close_enough(back, degrees, 1e-12 * (1.0 + std::abs(degrees))));
        ++round_trips;
    }
    GLINTFX_CHECK_EQ(round_trips, k_degrees.size());

    // Zero is exact, not merely close.
    GLINTFX_CHECK_EQ(glintfx::gltfx_angle_from_degrees(0.0).radians, 0.0);
    GLINTFX_CHECK_EQ(glintfx::gltfx_angle_to_degrees(glintfx::gltfx_angle{.radians = 0.0}), 0.0);

    std::println("angle_round_trips_through_degrees...: {} of {} boundary angle(s) round-tripped",
                 round_trips, k_degrees.size());
}

GLINTFX_TEST(world_precision_survives_past_the_screen_precision_limit) {
    // THE number the whole two-family decision exists for. Single
    // precision holds every whole number up to sixteen million and
    // stops; double keeps going. Both halves are asserted, because
    // only the PAIR proves the split earns its cost.
    const double just_past_the_limit = k_screen_precision_whole_number_limit + 1.0;

    // The world side holds it exactly.
    const glintfx::gltfx_vec2_world w{.x = just_past_the_limit, .y = -just_past_the_limit};
    GLINTFX_CHECK_EQ(w.x, just_past_the_limit);
    GLINTFX_CHECK(w.x != k_screen_precision_whole_number_limit);
    GLINTFX_CHECK_EQ(w.y, -just_past_the_limit);

    // The screen side does not: it collapses onto the limit itself.
    const glintfx::gltfx_vec2_screen s = glintfx::gltfx_vec2_world_to_screen(w);
    GLINTFX_CHECK_EQ(static_cast<double>(s.x), k_screen_precision_whole_number_limit);
    GLINTFX_CHECK_EQ(static_cast<double>(s.y), -k_screen_precision_whole_number_limit);

    // And the limit itself still crosses exactly - the loss starts at
    // the very next whole number, not before it.
    const glintfx::gltfx_vec2_screen at_limit = glintfx::gltfx_vec2_world_to_screen(
        glintfx::gltfx_vec2_world{.x = k_screen_precision_whole_number_limit, .y = 0.0});
    GLINTFX_CHECK_EQ(static_cast<double>(at_limit.x), k_screen_precision_whole_number_limit);
}

GLINTFX_TEST(crossing_to_screen_precision_is_deterministic_and_can_lose_exactness) {
    // What happens AT the crossing, stated as behavior rather than
    // hope. Four cases, each one a shape a consumer can really hand in.
    constexpr std::array<double, 4> k_inputs{
        0.0,
        -0.0,
        16'777'217.0, // loses exactness
        0.1,          // not representable exactly in EITHER precision
    };

    std::size_t crossings = 0;
    for (const double input : k_inputs) {
        const glintfx::gltfx_vec2_screen s =
            glintfx::gltfx_vec2_world_to_screen(glintfx::gltfx_vec2_world{.x = input, .y = input});
        // DETERMINISTIC: the same input always produces the same
        // output, and both components take the same path.
        GLINTFX_CHECK_EQ(s.x, s.y);
        GLINTFX_CHECK_EQ(s.x, static_cast<float>(input));

        // The way BACK is always exact, for every one of them: the
        // round trip world -> screen -> world returns exactly what the
        // FIRST crossing produced.
        const glintfx::gltfx_vec2_world back = glintfx::gltfx_vec2_screen_to_world(s);
        GLINTFX_CHECK_EQ(back.x, static_cast<double>(s.x));
        ++crossings;
    }
    GLINTFX_CHECK_EQ(crossings, k_inputs.size());

    // An infinity keeps its direction instead of becoming a finite
    // number or a non-number (the "keeps the input's direction"
    // clause of this project's pure-math rule).
    const glintfx::gltfx_vec2_screen infinite = glintfx::gltfx_vec2_world_to_screen(
        glintfx::gltfx_vec2_world{.x = std::numeric_limits<double>::infinity(),
                                  .y = -std::numeric_limits<double>::infinity()});
    GLINTFX_CHECK(std::isinf(infinite.x) && infinite.x > 0.0F);
    GLINTFX_CHECK(std::isinf(infinite.y) && infinite.y < 0.0F);

    std::println("crossing_to_screen_precision...: {} of {} crossing(s) exercised", crossings,
                 k_inputs.size());
}

GLINTFX_TEST(a_negative_rect_size_places_the_far_corner_before_the_near_one) {
    // The frozen MEANING of a negative size (core/rect.hpp's own
    // header comment): empty, never inverted. This case pins the
    // arithmetic consequence that makes "empty" the honest reading -
    // the far corner, which is corner + size by this shape's own
    // definition, lands BEFORE the near corner on the axis that went
    // negative, so the region between them holds nothing.
    const glintfx::gltfx_rect_world negative{
        .corner = {.x = 10.0, .y = 10.0},
        .size = {.x = -4.0, .y = 3.0},
    };
    const double far_x = negative.corner.x + negative.size.x;
    const double far_y = negative.corner.y + negative.size.y;
    GLINTFX_CHECK(far_x < negative.corner.x);
    GLINTFX_CHECK(far_y > negative.corner.y);

    // A zero size is the ordinary empty case, and it is NOT negative -
    // the two are distinct inputs that a future consuming operation
    // has to treat the same way.
    const glintfx::gltfx_rect_screen empty{
        .corner = {.x = 1.0F, .y = 2.0F},
        .size = {.x = 0.0F, .y = 0.0F},
    };
    GLINTFX_CHECK_EQ(empty.corner.x + empty.size.x, empty.corner.x);

    // And the type really does store the negative value it was given -
    // nothing normalizes it behind the consumer's back.
    GLINTFX_CHECK_EQ(negative.size.x, -4.0);
    GLINTFX_CHECK_EQ(negative.corner.x, 10.0);
}

GLINTFX_TEST(mat3_memory_order_is_column_major_as_the_graphics_card_expects) {
    // The leader's fourth decision. A pure translation is the cleanest
    // probe there is: in COLUMN-major order the two translation
    // components sit at indices 6 and 7, contiguous, at the END of the
    // array. In row-major order they would sit at indices 2 and 5.
    // Checking BOTH is what makes this case fail loudly if someone
    // "corrects" the order to match a textbook.
    const glintfx::gltfx_mat3 m = glintfx::gltfx_mat3_from_transform(glintfx::gltfx_transform{
        .translation = {.x = 11.0, .y = 22.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 1.0, .y = 1.0},
    });

    GLINTFX_CHECK_EQ(m.column_major[6], 11.0F);
    GLINTFX_CHECK_EQ(m.column_major[7], 22.0F);
    GLINTFX_CHECK_EQ(m.column_major[8], 1.0F);
    // The row-major positions must NOT hold the translation.
    GLINTFX_CHECK_EQ(m.column_major[2], 0.0F);
    GLINTFX_CHECK_EQ(m.column_major[5], 0.0F);

    // Stated the other way round, through the (row, column) reader:
    // the translation is column 2, rows 0 and 1.
    GLINTFX_CHECK_EQ(element_at(m, 0, 2), 11.0F);
    GLINTFX_CHECK_EQ(element_at(m, 1, 2), 22.0F);
    GLINTFX_CHECK_EQ(element_at(m, 2, 2), 1.0F);
    GLINTFX_CHECK_EQ(element_at(m, 2, 0), 0.0F);
    GLINTFX_CHECK_EQ(element_at(m, 2, 1), 0.0F);
}

GLINTFX_TEST(mat3_from_transform_applies_scale_then_rotation_then_translation) {
    // A quarter turn with a NON-UNIFORM scale - the only shape that
    // can tell the three orders apart. With scale first, the matrix's
    // upper-left block is [[sx*cos, -sy*sin], [sx*sin, sy*cos]]; any
    // other order swaps which scale factor lands on which entry.
    const double sx = 2.0;
    const double sy = 5.0;
    const glintfx::gltfx_mat3 m = glintfx::gltfx_mat3_from_transform(glintfx::gltfx_transform{
        .translation = {.x = 100.0, .y = 200.0},
        .rotation = glintfx::gltfx_angle_from_degrees(90.0),
        .scale = {.x = sx, .y = sy},
    });

    const double tolerance = 1e-5;
    // cos(90) == 0, sin(90) == 1.
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 0, 0)), 0.0, tolerance));
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 1, 0)), sx, tolerance));
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 0, 1)), -sy, tolerance));
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 1, 1)), 0.0, tolerance));
    // Translation is NOT scaled or rotated - it is applied last.
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 0, 2)), 100.0, tolerance));
    GLINTFX_CHECK(close_enough(static_cast<double>(element_at(m, 1, 2)), 200.0, tolerance));
    // Bottom row of an affine 2D matrix.
    GLINTFX_CHECK_EQ(element_at(m, 2, 0), 0.0F);
    GLINTFX_CHECK_EQ(element_at(m, 2, 1), 0.0F);
    GLINTFX_CHECK_EQ(element_at(m, 2, 2), 1.0F);
}

GLINTFX_TEST(the_neutral_transform_produces_the_identity_matrix) {
    // core/mat3.hpp promises this exact call is the identity's only
    // spelling, which is why no separate identity constant ships.
    const glintfx::gltfx_mat3 m = glintfx::gltfx_mat3_from_transform(glintfx::gltfx_transform{
        .translation = {.x = 0.0, .y = 0.0},
        .rotation = {.radians = 0.0},
        .scale = {.x = 1.0, .y = 1.0},
    });

    std::size_t elements_checked = 0;
    for (std::size_t column = 0; column < 3U; ++column) {
        for (std::size_t row = 0; row < 3U; ++row) {
            const float expected = (row == column) ? 1.0F : 0.0F;
            GLINTFX_CHECK_EQ(element_at(m, row, column), expected);
            ++elements_checked;
        }
    }
    GLINTFX_CHECK_EQ(elements_checked, static_cast<std::size_t>(9));
    std::println("the_neutral_transform_produces_the_identity_matrix: {} of 9 element(s) checked",
                 elements_checked);
}

GLINTFX_TEST(interpolation_returns_the_exact_endpoint_at_one_where_the_naive_form_does_not) {
    // The measured trap (core/interpolate.hpp's own header comment).
    // The naive expression is computed RIGHT HERE, in this test, and
    // asserted to be wrong - so this case proves the library's form is
    // better than the obvious one, instead of merely asserting that
    // the library agrees with itself.
    constexpr std::array<double, 4> k_starts{1e17, -1e17, 1e16, 1e8};
    constexpr double k_end = 1.0;

    std::size_t pairs = 0;
    std::size_t naive_failures = 0;
    for (const double a : k_starts) {
        const double naive = a + 1.0 * (k_end - a);
        const double ours = glintfx::gltfx_lerp(a, k_end, 1.0);
        // Ours is exact at the far end, for every one of them.
        GLINTFX_CHECK_EQ(ours, k_end);
        // And exact at the near end too.
        GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(a, k_end, 0.0), a);
        if (naive != k_end) {
            ++naive_failures;
        }
        ++pairs;
    }
    GLINTFX_CHECK_EQ(pairs, k_starts.size());
    // GODS_LAWS.md L-40, the non-empty sweep floor applied to the
    // MEASUREMENT itself: if the naive form failed on NONE of these,
    // this case would be proving nothing and must say so rather than
    // print green.
    GLINTFX_CHECK(naive_failures > 0);
    std::println("interpolation_returns_the_exact_endpoint_at_one...: {} of {} pair(s) checked, "
                 "naive form wrong on {} of them",
                 pairs, k_starts.size(), naive_failures);
}

GLINTFX_TEST(interpolation_extrapolates_past_both_ends_instead_of_clamping) {
    // Not clamped, on purpose: an easing curve that overshoots asks
    // for exactly this (see TODO.md's ANIM-EASING).
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(0.0, 10.0, 1.5), 15.0);
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(0.0, 10.0, -0.5), -5.0);
    // Middle, and a descending pair (b below a), which is a legitimate
    // direction and not an error.
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(0.0, 10.0, 0.5), 5.0);
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(10.0, 0.0, 0.25), 7.5);
    // Both endpoints equal: every parameter lands on the same value.
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(3.0, 3.0, 0.0), 3.0);
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(3.0, 3.0, 1.0), 3.0);
    GLINTFX_CHECK_EQ(glintfx::gltfx_lerp(3.0, 3.0, 0.5), 3.0);
}

GLINTFX_TEST(interpolation_never_fabricates_a_finite_answer_from_a_non_finite_input) {
    // MEASURED, not assumed (core/interpolate.hpp's own header
    // comment): std::lerp on this toolchain answers a non-finite input
    // with a plausible FINITE number - lerp(not-a-number, 1, 0.5)
    // returns 1, lerp(+infinity, 1, 0.5) returns 1, lerp(2, 3,
    // not-a-number) returns 3. That is the silent-success shape this
    // project refuses, so the rule is decided here instead of
    // inherited: any non-finite input, and the answer is not-a-number.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    // Closed enumeration of the non-finite argument POSITIONS, times
    // the non-finite VALUES a double can carry: 3 positions x 3 values
    // (not-a-number, +infinity, -infinity) = 9 cells, none skipped.
    constexpr std::size_t k_positions = 3;
    const std::array<double, 3> k_non_finite{nan, inf, -inf};

    std::size_t cells = 0;
    for (const double bad : k_non_finite) {
        for (std::size_t position = 0; position < k_positions; ++position) {
            const double a = (position == 0) ? bad : 2.0;
            const double b = (position == 1) ? bad : 3.0;
            const double t = (position == 2) ? bad : 0.5;
            const double result = glintfx::gltfx_lerp(a, b, t);
            GLINTFX_CHECK(std::isnan(result));
            ++cells;
        }
    }
    GLINTFX_CHECK_EQ(cells, k_non_finite.size() * k_positions);

    // The exact three calls whose measured std::lerp answers were
    // fabricated finite numbers - named one by one so a future change
    // that reintroduces the delegation fails HERE, on the cases that
    // actually caught it.
    GLINTFX_CHECK(std::isnan(glintfx::gltfx_lerp(nan, 1.0, 0.5)));
    GLINTFX_CHECK(std::isnan(glintfx::gltfx_lerp(inf, 1.0, 0.5)));
    GLINTFX_CHECK(std::isnan(glintfx::gltfx_lerp(2.0, 3.0, nan)));

    // Never undefined behavior and never a crash: a finite call still
    // returns a finite answer, so the guard did not swallow the
    // ordinary path.
    GLINTFX_CHECK(!std::isnan(glintfx::gltfx_lerp(0.0, 1.0, 0.5)));

    std::println("interpolation_never_fabricates...: {} of {} non-finite cell(s) checked", cells,
                 k_non_finite.size() * k_positions);
}
