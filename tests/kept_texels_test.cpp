// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <memory>

#include <glintfx/core/color.hpp>

#include "draw2d/kept_texels.hpp"
#include "draw2d/pod_buffer.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// kept_texels_test.cpp - R2D-TEXTURE, fatia T3: the ownership of the texels a texture with reduced
// levels keeps (draw2d/kept_texels.hpp), through a COUNTING allocator: the block taken is the block
// read, it is freed exactly once, by the allocator that made it, when the value goes, and taking a
// second block frees the first. The leak side is also seen by the sanitizer run.

using glintfx::gltfx_rgba8;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::kept_texels;
using glintfx::draw2d::pod_buffer;

namespace {

int g_released = 0;
void *g_last_released = nullptr;

void *counting_reallocate(void *block, std::size_t bytes) noexcept {
    return std::realloc(block, bytes);
}

void counting_release(void *block) noexcept {
    ++g_released;
    g_last_released = block;
    std::free(block);
}

constexpr batch_allocator k_counting{&counting_reallocate, &counting_release};

// A block of `count` texels made by the counting allocator.
[[nodiscard]] pod_buffer<gltfx_rgba8> block_of(std::size_t count) {
    auto *data =
        static_cast<gltfx_rgba8 *>(counting_reallocate(nullptr, count * sizeof(gltfx_rgba8)));
    return pod_buffer<gltfx_rgba8>{data, count, count};
}

} // namespace

GLINTFX_TEST(kept_texels_nothing_kept_is_empty_and_frees_nothing) {
    g_released = 0;
    {
        kept_texels kept;
        GLINTFX_CHECK(kept.texels().empty());
    }
    GLINTFX_CHECK_EQ(g_released, 0);
}

GLINTFX_TEST(kept_texels_the_block_taken_is_read_and_freed_once_when_it_goes) {
    g_released = 0;
    const pod_buffer<gltfx_rgba8> made = block_of(6U);
    {
        auto kept = std::make_unique<kept_texels>();
        kept->adopt(made, k_counting);
        GLINTFX_CHECK_EQ(kept->texels().size(), std::size_t{6});
        GLINTFX_CHECK(kept->texels().data() == made.data);
        GLINTFX_CHECK_EQ(g_released, 0);
    }
    GLINTFX_CHECK_EQ(g_released, 1);
    GLINTFX_CHECK(g_last_released == made.data);
}

GLINTFX_TEST(kept_texels_taking_a_second_block_frees_the_first) {
    g_released = 0;
    const pod_buffer<gltfx_rgba8> first = block_of(4U);
    const pod_buffer<gltfx_rgba8> second = block_of(2U);
    {
        kept_texels kept;
        kept.adopt(first, k_counting);
        kept.adopt(second, k_counting);
        GLINTFX_CHECK_EQ(g_released, 1);
        GLINTFX_CHECK(g_last_released == first.data);
        GLINTFX_CHECK_EQ(kept.texels().size(), std::size_t{2});
    }
    GLINTFX_CHECK_EQ(g_released, 2);
    GLINTFX_CHECK(g_last_released == second.data);
}
