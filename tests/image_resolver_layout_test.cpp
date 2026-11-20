// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
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

constexpr std::size_t k_resolution_region_offset =
    round_up(sizeof(texture_pointer), alignof(glintfx::gltfx_texture_region));
constexpr std::size_t k_resolution_size = round_up(
    k_resolution_region_offset + sizeof(glintfx::gltfx_texture_region), alignof(resolution));

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
    // The same facts as the static_asserts above, checked again in the running binary, so a broken
    // layout also shows up in the test report and not only as a build error.
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
