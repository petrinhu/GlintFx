// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

#include "draw2d/quad_vertices.hpp"
#include "draw2d/triangle_batch.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// triangle_batch_test.cpp - R2D-BATCH, fatia B2c (docs/plano-w7d.md sec. 4.3/4.4, D-W7D-18):
// the pure atom that ASSEMBLES a frame's batch as INDEXED TRIANGLES - the way SDL3
// (SDL_RenderGeometry), RmlUi 6 and raylib assemble it - so that the same mechanism serves the quad
// of today and the shapes and images that come after (the vertex format is fixed once, here).
//
// WHAT IS PROVED, ALL OF IT ON PURE DATA (no GL, no window):
//   - one quad is 4 vertices and 6 indices, the two triangles of the frozen diagonal
//     (top_left, top_right, bottom_right) and (top_left, bottom_right, bottom_left)
//     (docs/auditoria-api-draw2d.md B0-I4);
//   - the vertex is 32 bytes: position float2 in pixels, texture coordinate float2, color float4
//     LINEAR and PREMULTIPLIED (a half-transparent red of alpha 0.5 is stored as 0.5 in the red
//     channel and 0.5 in alpha) - the mutant that stores the straight color is the one B7 repeats;
//   - the index is 32 bits (a batch of more than 16 384 pieces does not wrap);
//   - CONSECUTIVE pieces with the SAME state key (program, texture, blend) are ONE run, and a
//     change in ANY of the three starts another - both directions of the raylib#6110/#4849 pain;
//   - MEMORY: an armed allocator that fails at EVERY possible point of a growing batch never
//     throws, never leaves half a piece (vertices, indices and runs stay consistent), counts the
//     dropped piece, and lets later pieces through once the allocator works again.
//
// RED, SEEN: before triangle_batch.{hpp,cpp} existed, this file's own #include line failed to
// compile; then, against a body that stored nothing, every cell below failed.

using glintfx::gltfx_rgba;
using glintfx::gltfx_vec2_screen;
using glintfx::draw2d::batch_allocator;
using glintfx::draw2d::batch_state;
using glintfx::draw2d::batch_vertex;
using glintfx::draw2d::default_batch_allocator;
using glintfx::draw2d::quad_corners_pixel;
using glintfx::draw2d::triangle_batch;

namespace {
constexpr batch_state k_state_a{1, 1, 1};

[[nodiscard]] quad_corners_pixel square_at(float x, float y, float size) {
    return quad_corners_pixel{gltfx_vec2_screen{x, y}, gltfx_vec2_screen{x + size, y},
                              gltfx_vec2_screen{x + size, y + size},
                              gltfx_vec2_screen{x, y + size}};
}

constexpr gltfx_rgba k_white{1.0F, 1.0F, 1.0F, 1.0F};

// An armed allocator: it fails the Nth call to reallocate() (1-based; 0 = never fails), and
// counts every call. The batch takes plain function pointers, so the state is a thread-local.
struct armed_state {
    int calls = 0;
    int fail_at = 0;
    int failures = 0;
    std::array<std::size_t, 8> requested{}; // the size asked at each call (the first eight)
};
thread_local armed_state g_armed;

[[nodiscard]] void *armed_reallocate(void *block, std::size_t bytes) noexcept {
    if (g_armed.calls < 8) {
        g_armed.requested[static_cast<std::size_t>(g_armed.calls)] = bytes;
    }
    ++g_armed.calls;
    if (g_armed.fail_at != 0 && g_armed.calls == g_armed.fail_at) {
        ++g_armed.failures;
        return nullptr; // like realloc: the old block is untouched
    }
    return default_batch_allocator().reallocate(block, bytes);
}
void armed_release(void *block) noexcept { default_batch_allocator().release(block); }

[[nodiscard]] batch_allocator armed() { return batch_allocator{armed_reallocate, armed_release}; }
} // namespace

GLINTFX_TEST(triangle_batch_assembly_cells) {
    int analyzed = 0;

    // 1. One quad: 4 vertices and 6 indices, the frozen diagonal, positions and texture
    //    coordinates by role.
    {
        triangle_batch batch;
        GLINTFX_CHECK(batch.add_quad(square_at(10.0F, 20.0F, 5.0F), k_white, k_state_a));
        GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{4});
        GLINTFX_CHECK_EQ(batch.indices().size(), std::size_t{6});
        const std::array<std::uint32_t, 6> expected{0, 1, 2, 0, 2, 3};
        for (std::size_t i = 0; i < 6; ++i) {
            GLINTFX_CHECK_EQ(batch.indices()[i], expected[i]);
        }
        const auto v = batch.vertices();
        GLINTFX_CHECK(v[0].x == 10.0F && v[0].y == 20.0F); // top_left
        GLINTFX_CHECK(v[1].x == 15.0F && v[1].y == 20.0F); // top_right
        GLINTFX_CHECK(v[2].x == 15.0F && v[2].y == 25.0F); // bottom_right
        GLINTFX_CHECK(v[3].x == 10.0F && v[3].y == 25.0F); // bottom_left
        GLINTFX_CHECK(v[0].u == 0.0F && v[0].v == 0.0F);
        GLINTFX_CHECK(v[1].u == 1.0F && v[1].v == 0.0F);
        GLINTFX_CHECK(v[2].u == 1.0F && v[2].v == 1.0F);
        GLINTFX_CHECK(v[3].u == 0.0F && v[3].v == 1.0F);
        ++analyzed;
    }
    // 2. The second quad indexes its OWN four vertices (base 4).
    {
        triangle_batch batch;
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK(batch.add_quad(square_at(2, 2, 1), k_white, k_state_a));
        GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{8});
        const std::array<std::uint32_t, 6> second{4, 5, 6, 4, 6, 7};
        for (std::size_t i = 0; i < 6; ++i) {
            GLINTFX_CHECK_EQ(batch.indices()[6 + i], second[i]);
        }
        ++analyzed;
    }
    // 3. The vertex is 32 bytes, and the color is stored LINEAR and PREMULTIPLIED: alpha 0.5 over
    //    a red of 1.0 is 0.5 in the channel (and alpha stays 0.5).
    {
        static_assert(sizeof(batch_vertex) == 32);
        triangle_batch batch;
        GLINTFX_CHECK(
            batch.add_quad(square_at(0, 0, 1), gltfx_rgba{1.0F, 0.5F, 0.0F, 0.5F}, k_state_a));
        for (const batch_vertex &vertex : batch.vertices()) {
            GLINTFX_CHECK(vertex.r == 0.5F && vertex.g == 0.25F && vertex.b == 0.0F &&
                          vertex.a == 0.5F);
        }
        ++analyzed;
    }
    // 4. A fully opaque color is stored as it is; a fully transparent one is zero in every channel.
    {
        triangle_batch batch;
        GLINTFX_CHECK(
            batch.add_quad(square_at(0, 0, 1), gltfx_rgba{0.2F, 0.4F, 0.6F, 1.0F}, k_state_a));
        GLINTFX_CHECK(
            batch.add_quad(square_at(0, 0, 1), gltfx_rgba{0.9F, 0.9F, 0.9F, 0.0F}, k_state_a));
        const auto v = batch.vertices();
        GLINTFX_CHECK(v[0].r == 0.2F && v[0].g == 0.4F && v[0].b == 0.6F && v[0].a == 1.0F);
        GLINTFX_CHECK(v[4].r == 0.0F && v[4].g == 0.0F && v[4].b == 0.0F && v[4].a == 0.0F);
        ++analyzed;
    }
    // 5. The index is 32 bits: 70 000 pieces (280 000 vertices) do not wrap at 65 535, and the
    //    last quad still indexes its own vertices.
    {
        triangle_batch batch;
        for (int i = 0; i < 70000; ++i) {
            GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{280000});
        GLINTFX_CHECK_EQ(batch.indices().size(), std::size_t{420000});
        GLINTFX_CHECK_EQ(batch.indices()[420000 - 6], std::uint32_t{279996});
        GLINTFX_CHECK_EQ(batch.indices()[420000 - 1], std::uint32_t{279999});
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 5);
    std::println("triangle_batch_test: {} celula(s) conferida(s) (montagem)", analyzed);
}

GLINTFX_TEST(triangle_batch_run_cells) {
    int analyzed = 0;

    // Three pieces with the SAME state: ONE run of 18 indices.
    {
        triangle_batch batch;
        for (int i = 0; i < 3; ++i) {
            GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{1});
        GLINTFX_CHECK_EQ(batch.runs()[0].first_index, std::size_t{0});
        GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{18});
        ++analyzed;
    }
    // A change in ANY ONE of the three fields of the state key starts another run (program,
    // texture, blend), and the second run starts where the first ended.
    for (int field = 0; field < 3; ++field) {
        batch_state other = k_state_a;
        (field == 0 ? other.program : field == 1 ? other.texture : other.blend) = 2;
        triangle_batch batch;
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, other));
        GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{2});
        GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{6});
        GLINTFX_CHECK_EQ(batch.runs()[1].first_index, std::size_t{6});
        GLINTFX_CHECK_EQ(batch.runs()[1].index_count, std::size_t{6});
        ++analyzed;
    }
    // A, A, B, B -> two runs of 12; A, B, A -> THREE runs (only CONSECUTIVE pieces merge:
    // reordering to merge more is a later unit).
    {
        const batch_state b{1, 2, 1};
        triangle_batch batch;
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, b));
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, b));
        GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{2});
        GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{12});
        GLINTFX_CHECK_EQ(batch.runs()[1].index_count, std::size_t{12});
        ++analyzed;

        triangle_batch aba;
        GLINTFX_CHECK(aba.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK(aba.add_quad(square_at(0, 0, 1), k_white, b));
        GLINTFX_CHECK(aba.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        GLINTFX_CHECK_EQ(aba.runs().size(), std::size_t{3});
        ++analyzed;
    }
    // The state of a run is the state of its pieces.
    {
        const batch_state b{7, 8, 9};
        triangle_batch batch;
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, b));
        GLINTFX_CHECK(batch.runs()[0].state.program == 7 && batch.runs()[0].state.texture == 8 &&
                      batch.runs()[0].state.blend == 9);
        ++analyzed;
    }
    // clear() empties the batch and keeps its capacity: no allocation on the next fill.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        for (int i = 0; i < 100; ++i) {
            GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
        const int calls_after_first_fill = g_armed.calls;
        batch.clear();
        GLINTFX_CHECK(batch.vertices().empty() && batch.indices().empty() && batch.runs().empty());
        for (int i = 0; i < 100; ++i) {
            GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(g_armed.calls, calls_after_first_fill);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 8);
    std::println("triangle_batch_test: {} celula(s) conferida(s) (corridas)", analyzed);
}

GLINTFX_TEST(triangle_batch_out_of_memory_at_every_point) {
    int analyzed = 0;

    // Fill a batch of 40 pieces under an allocator that fails the Nth reallocation, for EVERY N
    // that the fill makes (the reference fill counts them). Whatever the point of failure: nothing
    // throws (the functions are noexcept: a throw would end the process), the dropped piece is
    // COUNTED, and the batch holds whole pieces only - 4 vertices and 6 indices per piece, and the
    // runs covering exactly the indices.
    g_armed = armed_state{};
    {
        triangle_batch reference(armed());
        for (int i = 0; i < 40; ++i) {
            GLINTFX_CHECK(reference.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
    }
    const int total_calls = g_armed.calls;
    GLINTFX_CHECK(total_calls >= 3); // vertices, indices and runs each grow at least once

    for (int n = 1; n <= total_calls; ++n) {
        g_armed = armed_state{};
        g_armed.fail_at = n;
        triangle_batch batch(armed());
        std::size_t accepted = 0;
        std::size_t refused = 0;
        for (int i = 0; i < 40; ++i) {
            if (batch.add_quad(square_at(0, 0, 1), k_white, k_state_a)) {
                ++accepted;
            } else {
                ++refused;
            }
        }
        GLINTFX_CHECK_EQ(g_armed.failures, 1);
        GLINTFX_CHECK(refused >= 1);
        GLINTFX_CHECK_EQ(batch.pieces_dropped_out_of_memory(), refused);
        GLINTFX_CHECK_EQ(batch.vertices().size(), accepted * 4);
        GLINTFX_CHECK_EQ(batch.indices().size(), accepted * 6);
        // the runs cover exactly the indices, no more and no less
        std::size_t covered = 0;
        for (const auto &run : batch.runs()) {
            GLINTFX_CHECK_EQ(run.first_index, covered);
            covered += run.index_count;
        }
        GLINTFX_CHECK_EQ(covered, batch.indices().size());
        // the allocator works again: a later piece goes through
        GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        ++analyzed;
    }
    // every allocation point of the reference fill was armed once (the count is impressed below)
    GLINTFX_CHECK_EQ(analyzed, total_calls);

    // A batch that can never allocate drops every piece, counts them, and holds nothing.
    {
        g_armed = armed_state{};
        struct never {
            static void *fail(void *, std::size_t) noexcept { return nullptr; }
        };
        triangle_batch batch(batch_allocator{never::fail, armed_release});
        for (int i = 0; i < 5; ++i) {
            GLINTFX_CHECK(!batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(batch.pieces_dropped_out_of_memory(), std::size_t{5});
        GLINTFX_CHECK(batch.vertices().empty() && batch.indices().empty() && batch.runs().empty());
        ++analyzed;
    }

    std::println("triangle_batch_test: {} celula(s) conferida(s) (falta de memoria: {} pontos de "
                 "falha + o alocador que nunca aloca)",
                 analyzed, total_calls);
}

// reserve() (R2D-BATCH B4, `reserve_pieces` of the public descriptor): room for that many MORE
// pieces, taken in one go, so that the fill that follows allocates nothing.
GLINTFX_TEST(triangle_batch_reserve_cells) {
    // 1. After reserve(100), a hundred pieces go in with NO further call to the allocator.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        GLINTFX_CHECK(batch.reserve(100));
        const int after_reserve = g_armed.calls;
        GLINTFX_CHECK_EQ(after_reserve, 3); // vertices, indices, runs: one each
        for (int i = 0; i < 100; ++i) {
            GLINTFX_CHECK(
                batch.add_quad(square_at(static_cast<float>(i), 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(g_armed.calls, after_reserve);
        GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{400});
    }
    // 2. A request of MORE THAN DOUBLE the capacity in one call takes exactly what was asked, not
    // the
    //    doubled capacity (`grown = needed`): 1000 pieces from an empty batch are 4000 vertices of
    //    32 bytes, 6000 indices of 4 bytes, and one run.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        GLINTFX_CHECK(batch.reserve(1000));
        GLINTFX_CHECK_EQ(g_armed.requested[0], std::size_t{128000});
        GLINTFX_CHECK_EQ(g_armed.requested[1], std::size_t{24000});
        GLINTFX_CHECK(g_armed.requested[2] >= sizeof(glintfx::draw2d::draw_run));
        // ... and they all fit: no more calls while a thousand pieces are added
        const int after_reserve = g_armed.calls;
        for (int i = 0; i < 1000; ++i) {
            GLINTFX_CHECK(
                batch.add_quad(square_at(static_cast<float>(i), 0, 1), k_white, k_state_a));
        }
        GLINTFX_CHECK_EQ(g_armed.calls, after_reserve);
    }
    // 3. An allocator that fails at EACH of the three points: reserve() says false, and the batch
    // is
    //    still usable and empty (the stores it did grow stay, harmlessly).
    for (int point = 1; point <= 3; ++point) {
        g_armed = armed_state{};
        g_armed.fail_at = point;
        triangle_batch batch(armed());
        GLINTFX_CHECK(!batch.reserve(50));
        GLINTFX_CHECK(batch.vertices().empty() && batch.indices().empty() && batch.runs().empty());
        GLINTFX_CHECK(
            batch.add_quad(square_at(0, 0, 1), k_white, k_state_a)); // allocator works again
    }
    // 4. A request that overflows the size arithmetic is refused without asking the allocator.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        GLINTFX_CHECK(!batch.reserve(std::numeric_limits<std::size_t>::max() / 2));
        GLINTFX_CHECK_EQ(g_armed.calls, 0);
        // 2^63 pieces: BOTH products (4 vertices and 6 indices per piece) wrap to zero, so without
        // the explicit check the request would "succeed" having reserved nothing.
        GLINTFX_CHECK(!batch.reserve(std::size_t{1} << 63));
        GLINTFX_CHECK_EQ(g_armed.calls, 0);
    }
    // 5. reserve(0) is a no-op that succeeds.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        GLINTFX_CHECK(batch.reserve(0));
        GLINTFX_CHECK_EQ(g_armed.calls, 0);
    }
    std::println("triangle_batch_test: reserve conferido (5 casos)");
}

// CTO (revisao do 088d5cc): the public promise of reserve_pieces is that the first frames "do not
// grow storage while you draw" - with pieces that ALTERNATE state (a run each), too.
GLINTFX_TEST(triangle_batch_reserve_covers_one_run_per_piece) {
    g_armed = armed_state{};
    triangle_batch batch(armed());
    GLINTFX_CHECK(batch.reserve(100));
    const int after_reserve = g_armed.calls;
    for (int i = 0; i < 100; ++i) {
        GLINTFX_CHECK(batch.add_quad(square_at(static_cast<float>(i), 0, 1), k_white,
                                     i % 2 == 0 ? k_state_a : batch_state{2, 2, 2}));
    }
    GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{100});
    GLINTFX_CHECK_EQ(g_armed.calls, after_reserve);
}
