// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <glintfx/draw2d/image_resolver.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// image_resolver_layout_test.cpp - W8 deep verification, round 1, finding C4-1 (D-W8-2008;
// GODS_LAWS.md L-20, L-26). The leader ratified that the packets the library and the consumer's
// resolver exchange NEVER change shape: new data comes in a packet of a new type, as Vulkan does
// (D-W8-1247, ratified 20/11/2026). This test is what makes that promise fail loudly if someone
// breaks it: adding, removing, retyping or reordering a field of gltfx_image_reference,
// gltfx_image_resolution or gltfx_image_resolver stops this file from compiling, and the run-time
// checks below say the same in the test report.
//
// The header tells the CONSUMER never to take the size of gltfx_image_reference; the library
// takes it here only to guard its own promise. Every expected size is written from the field
// types, never as a number measured on this machine, so the same facts hold on every target
// (L-04: Linux and Windows, 64-bit).

namespace {

constexpr std::size_t round_up(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

using reference = glintfx::gltfx_image_reference;
using resolution = glintfx::gltfx_image_resolution;
using resolver = glintfx::gltfx_image_resolver;
using texture_pointer = const glintfx::gltfx_texture_2d *;
using expected_function =
    glintfx::gltfx_image_resolution (*)(void *, const glintfx::gltfx_image_reference *) noexcept;

// What goes INSIDE gltfx_image_resolution by value is part of the frozen packet (D-W8-1247; W8 deep
// verification, fix1 of R2-A, D-W8-2035): every expected size below comes from the primitive types
// (double, std::uint8_t, a pointer), never from sizeof() of a nested type, which would grow with
// it.
constexpr std::size_t k_vec2_texel_size = 2 * sizeof(double);
constexpr std::size_t k_rect_texel_size = 2 * k_vec2_texel_size;
constexpr std::size_t k_region_size =
    round_up(k_rect_texel_size + sizeof(std::uint8_t), alignof(double));
constexpr std::size_t k_resolution_region_offset =
    round_up(sizeof(texture_pointer), alignof(double));
constexpr std::size_t k_resolution_size = round_up(
    k_resolution_region_offset + k_region_size,
    alignof(double) > alignof(texture_pointer) ? alignof(double) : alignof(texture_pointer));

} // namespace

// The three packets stay plain values with fixed field offsets.
static_assert(
    std::is_aggregate_v<reference> && std::is_standard_layout_v<reference> &&
        std::is_trivially_copyable_v<reference>,
    "gltfx_image_reference must stay a plain aggregate: its layout is frozen (D-W8-1247)");
static_assert(
    std::is_aggregate_v<resolution> && std::is_standard_layout_v<resolution> &&
        std::is_trivially_copyable_v<resolution>,
    "gltfx_image_resolution must stay a plain aggregate: its layout is frozen (D-W8-1247)");
static_assert(std::is_aggregate_v<resolver> && std::is_standard_layout_v<resolver> &&
                  std::is_trivially_copyable_v<resolver>,
              "gltfx_image_resolver must stay a plain aggregate: its layout is frozen (D-W8-1247)");

// gltfx_image_reference: exactly one field, the text.
static_assert(std::is_same_v<decltype(reference::text), std::string_view>,
              "gltfx_image_reference::text must stay a std::string_view");
static_assert(offsetof(reference, text) == 0, "text must stay the first field");
static_assert(
    sizeof(reference) == sizeof(std::string_view),
    "gltfx_image_reference must stay exactly one string_view: a new field breaks D-W8-1247");

// gltfx_image_resolution: the texture, then the region, and nothing else.
static_assert(std::is_same_v<decltype(resolution::texture), texture_pointer>,
              "gltfx_image_resolution::texture must stay a pointer to a const texture");
static_assert(std::is_same_v<decltype(resolution::region), glintfx::gltfx_texture_region>,
              "gltfx_image_resolution::region must stay a gltfx_texture_region");
static_assert(offsetof(resolution, texture) == 0, "texture must stay the first field");
static_assert(offsetof(resolution, region) == k_resolution_region_offset,
              "region must stay right after texture");
static_assert(sizeof(resolution) == k_resolution_size,
              "gltfx_image_resolution must stay texture plus region: a new field breaks D-W8-1247");

// What the region holds, by value: the rectangle in texels (corner, then size, two doubles each),
// then the mirror (one byte), and nothing else. A field added anywhere in here changes the packet.
using region = glintfx::gltfx_texture_region;
using rect_texel = glintfx::gltfx_rect_texel;
using vec2_texel = glintfx::gltfx_vec2_texel;
static_assert(std::is_same_v<decltype(vec2_texel::x), double> &&
                  std::is_same_v<decltype(vec2_texel::y), double>,
              "gltfx_vec2_texel must stay two doubles, x then y");
static_assert(offsetof(vec2_texel, x) == 0 && offsetof(vec2_texel, y) == sizeof(double) &&
                  sizeof(vec2_texel) == k_vec2_texel_size,
              "gltfx_vec2_texel must stay x then y: a new field breaks D-W8-1247");
static_assert(std::is_same_v<decltype(rect_texel::corner), vec2_texel> &&
                  std::is_same_v<decltype(rect_texel::size), vec2_texel>,
              "gltfx_rect_texel must stay corner then size, two gltfx_vec2_texel");
static_assert(offsetof(rect_texel, corner) == 0 &&
                  offsetof(rect_texel, size) == k_vec2_texel_size &&
                  sizeof(rect_texel) == k_rect_texel_size,
              "gltfx_rect_texel must stay corner then size: a new field breaks D-W8-1247");
static_assert(std::is_same_v<std::underlying_type_t<glintfx::gltfx_texture_mirror>, std::uint8_t>,
              "gltfx_texture_mirror must stay one byte");
// The four values keep their numbers: a program compiled before writes them, and a renumbered value
// would read as another mirror (D-W8-1189, D-W8-2039). A NEW value may be appended by a later
// version, as texture_region.hpp says (a value this build does not know is refused by the call,
// R4), so the count of values is not pinned.
static_assert(
    static_cast<std::uint8_t>(glintfx::gltfx_texture_mirror::none) == 0 &&
        static_cast<std::uint8_t>(glintfx::gltfx_texture_mirror::flip_left_right) == 1 &&
        static_cast<std::uint8_t>(glintfx::gltfx_texture_mirror::flip_top_bottom) == 2 &&
        static_cast<std::uint8_t>(glintfx::gltfx_texture_mirror::flip_left_right_and_top_bottom) ==
            3,
    "the four gltfx_texture_mirror values must keep their numbers: renumbering breaks D-W8-1247");
static_assert(std::is_same_v<decltype(region::source), rect_texel> &&
                  std::is_same_v<decltype(region::mirror), glintfx::gltfx_texture_mirror>,
              "gltfx_texture_region must stay source then mirror");
static_assert(offsetof(region, source) == 0 && offsetof(region, mirror) == k_rect_texel_size &&
                  sizeof(region) == k_region_size,
              "gltfx_texture_region must stay source then mirror: a new field breaks D-W8-1247");

// gltfx_image_resolver: the function, then the context, and nothing else.
static_assert(
    std::is_same_v<glintfx::gltfx_image_resolver_fn, expected_function>,
    "the resolver function type must stay (void *, const reference *) noexcept -> resolution");
static_assert(std::is_same_v<decltype(resolver::function), glintfx::gltfx_image_resolver_fn>,
              "gltfx_image_resolver::function must stay a gltfx_image_resolver_fn");
static_assert(std::is_same_v<decltype(resolver::context), void *>,
              "gltfx_image_resolver::context must stay a void pointer");
static_assert(offsetof(resolver, function) == 0, "function must stay the first field");
static_assert(offsetof(resolver, context) == sizeof(glintfx::gltfx_image_resolver_fn),
              "context must stay right after function");
static_assert(sizeof(resolver) == sizeof(glintfx::gltfx_image_resolver_fn) + sizeof(void *),
              "gltfx_image_resolver must stay function plus context: a new field breaks D-W8-1247");

GLINTFX_TEST(image_resolver_packets_keep_the_frozen_layout_at_run_time) {
    // A broken layout never reaches this point: the static_asserts above stop the build. This case
    // only gives the runner a test to report, with the sizes the file was built with.
    GLINTFX_CHECK_EQ(sizeof(reference), sizeof(std::string_view));
    GLINTFX_CHECK_EQ(offsetof(resolution, region), k_resolution_region_offset);
    GLINTFX_CHECK_EQ(sizeof(resolution), k_resolution_size);
    GLINTFX_CHECK_EQ(sizeof(resolver), sizeof(glintfx::gltfx_image_resolver_fn) + sizeof(void *));
}

GLINTFX_TEST(image_resolver_packets_default_to_no_owner_and_no_resolver) {
    // Values written by hand from the header text, never read back from the code under test.
    constexpr resolution answer{};
    constexpr resolver registration{};
    GLINTFX_CHECK(answer.texture == nullptr);
    GLINTFX_CHECK(registration.function == nullptr);
    GLINTFX_CHECK(registration.context == nullptr);
}
