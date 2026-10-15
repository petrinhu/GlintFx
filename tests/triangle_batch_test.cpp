// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <bit>
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
//   - the vertex is 36 bytes (R2D-TEXTURE T4a, D-W8-23 and D-W8-406): position float2 in pixels,
//     texture coordinate float2, color float4, and the homogeneous coordinate q, 1 until T4b;
//     color float4
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
using glintfx::draw2d::colored_pixel_vertex;
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
    // 3. The vertex is 36 bytes, and the color is stored LINEAR and PREMULTIPLIED: alpha 0.5 over
    //    a red of 1.0 is 0.5 in the channel (and alpha stays 0.5).
    {
        static_assert(sizeof(batch_vertex) == 36);
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
    //    36 bytes, 6000 indices of 4 bytes, and one run.
    {
        g_armed = armed_state{};
        triangle_batch batch(armed());
        GLINTFX_CHECK(batch.reserve(1000));
        GLINTFX_CHECK_EQ(g_armed.requested[0], std::size_t{144000});
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

// S2a: TRIANGLE pieces. The vertices are in pixels with a color each; the indices are local to the
// piece (0..n-1), or none at all (then the piece is 0..n-1 in order). Same atomicity as add_quad.
namespace {

[[nodiscard]] colored_pixel_vertex vertex_at(float x, float y, gltfx_rgba color) {
    return colored_pixel_vertex{gltfx_vec2_screen{x, y}, color};
}

// Three vertices at (x, y), (x + 1, y), (x + 1, y + 1), all of `color`.
[[nodiscard]] std::array<colored_pixel_vertex, 3> triangle_at(float x, float y, gltfx_rgba color) {
    return {vertex_at(x, y, color), vertex_at(x + 1.0F, y, color),
            vertex_at(x + 1.0F, y + 1.0F, color)};
}

constexpr gltfx_rgba k_half_red{1.0F, 0.0F, 0.0F, 0.5F};

} // namespace

// quad, triangles, quad, all in one state: 11 vertices, ONE run of 15 indices, and the indices of
// the three pieces (the quad 0..3, the triangle 4..6, the quad 7..10).
GLINTFX_TEST(triangle_batch_quad_triangles_quad_is_one_run_of_fifteen_indices) {
    triangle_batch batch;
    const auto tri = triangle_at(5.0F, 0.0F, k_white);
    const std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
    GLINTFX_CHECK(batch.add_triangles(tri, local, k_state_a));
    GLINTFX_CHECK(batch.add_quad(square_at(10, 0, 1), k_white, k_state_a));
    GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{11});
    GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{1});
    GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{15});
    const std::array<std::uint32_t, 15> expected{0U, 1U, 2U, 0U, 2U, 3U, 4U, 5U,
                                                 6U, 7U, 8U, 9U, 7U, 9U, 10U};
    const auto indices = batch.indices();
    GLINTFX_CHECK_EQ(indices.size(), expected.size());
    for (std::size_t i = 0; i < expected.size() && i < indices.size(); ++i) {
        GLINTFX_CHECK_EQ(indices[i], expected[i]);
    }
}

// A triangle piece with NO indices, after a quad (base 4): six vertices give the indices 4..9.
GLINTFX_TEST(triangle_batch_triangles_without_indices_are_sequential_from_their_base) {
    triangle_batch batch;
    GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
    std::array<colored_pixel_vertex, 6> six{};
    for (std::size_t i = 0; i < six.size(); ++i) {
        six[i] = vertex_at(static_cast<float>(i), 5.0F, k_white);
    }
    GLINTFX_CHECK(batch.add_triangles(six, {}, k_state_a));
    GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{10});
    GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{12});
    const std::array<std::uint32_t, 12> expected{0U, 1U, 2U, 0U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const auto indices = batch.indices();
    GLINTFX_CHECK_EQ(indices.size(), expected.size());
    for (std::size_t i = 0; i < expected.size() && i < indices.size(); ++i) {
        GLINTFX_CHECK_EQ(indices[i], expected[i]);
    }
}

// The allocator refuses the SECOND call, which is the index store (the first is the vertex store).
// The batch is left as it was - no vertex, no index, no run - and the piece is counted, whole.
GLINTFX_TEST(triangle_batch_triangles_out_of_memory_on_the_index_store_leaves_it_as_it_was) {
    g_armed = armed_state{};
    g_armed.fail_at = 2;
    triangle_batch batch(armed());
    const auto tri = triangle_at(0.0F, 0.0F, k_white);
    const std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    GLINTFX_CHECK(!batch.add_triangles(tri, local, k_state_a));
    GLINTFX_CHECK_EQ(g_armed.failures, 1);
    GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{0});
    GLINTFX_CHECK_EQ(batch.indices().size(), std::size_t{0});
    GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{0});
    GLINTFX_CHECK_EQ(batch.pieces_dropped_out_of_memory(), std::size_t{1});
    // the allocator works again: the same piece goes through, whole
    GLINTFX_CHECK(batch.add_triangles(tri, local, k_state_a));
    GLINTFX_CHECK_EQ(batch.vertices().size(), std::size_t{3});
    GLINTFX_CHECK_EQ(batch.indices().size(), std::size_t{3});
}

// The color of each vertex is premultiplied on its own: (1, 0, 0, 0.5) becomes (0.5, 0, 0, 0.5).
// The texture coordinate of a triangle vertex is inert: 0.
GLINTFX_TEST(triangle_batch_triangle_vertex_colors_are_premultiplied_one_by_one) {
    triangle_batch batch;
    const std::array<colored_pixel_vertex, 3> tri{vertex_at(0.0F, 0.0F, k_half_red),
                                                  vertex_at(1.0F, 0.0F, k_half_red),
                                                  vertex_at(1.0F, 1.0F, k_white)};
    GLINTFX_CHECK(batch.add_triangles(tri, {}, k_state_a));
    const auto stored = batch.vertices();
    GLINTFX_CHECK_EQ(stored.size(), std::size_t{3});
    GLINTFX_CHECK(stored[0].r == 0.5F && stored[0].g == 0.0F && stored[0].b == 0.0F);
    GLINTFX_CHECK(stored[0].a == 0.5F);
    GLINTFX_CHECK(stored[2].r == 1.0F && stored[2].a == 1.0F);
    GLINTFX_CHECK(stored[0].u == 0.0F && stored[0].v == 0.0F);
}

// The batch COPIES the vertices: the caller overwrites its array after the add, and the batch keeps
// the values submitted. The expected values are the literals written here.
GLINTFX_TEST(triangle_batch_triangle_vertices_are_copied_not_referenced) {
    triangle_batch batch;
    std::array<colored_pixel_vertex, 3> source = triangle_at(2.0F, 3.0F, k_white);
    GLINTFX_CHECK(batch.add_triangles(source, {}, k_state_a));
    source = triangle_at(90.0F, 90.0F, k_half_red);
    const auto stored = batch.vertices();
    GLINTFX_CHECK_EQ(stored.size(), std::size_t{3});
    GLINTFX_CHECK(stored[0].x == 2.0F && stored[0].y == 3.0F);
    GLINTFX_CHECK(stored[1].x == 3.0F && stored[2].y == 4.0F);
    GLINTFX_CHECK(stored[2].r == 1.0F && stored[2].a == 1.0F);
}

// S2a-fix1: a run OPENED by a triangle piece counts the indices it was given, not six.
// The batch starts with the triangle, so the run is opened (not extended) by it.
GLINTFX_TEST(triangle_batch_a_run_opened_by_a_triangle_counts_its_given_indices) {
    triangle_batch batch;
    const auto tri = triangle_at(0.0F, 0.0F, k_white);
    const std::array<std::uint32_t, 3> local{0U, 1U, 2U};
    GLINTFX_CHECK(batch.add_triangles(tri, local, k_state_a));
    GLINTFX_CHECK_EQ(batch.runs().size(), std::size_t{1});
    GLINTFX_CHECK_EQ(batch.runs()[0].index_count, std::size_t{3});
}

// R2D-TEXTURE T4a (D-W8-23, D-W8-406): every vertex carries the homogeneous coordinate q LAST, and
// for an untextured piece it is 1, for a quad and for a triangle piece alike (T4b-B1). The quad
// keeps its texture coordinates by role, the triangle piece keeps them at 0.
GLINTFX_TEST(triangle_batch_every_vertex_carries_q_of_one) {
    static_assert(offsetof(batch_vertex, q) == 8U * sizeof(float),
                  "q is the ninth float, after the color");
    triangle_batch batch;
    GLINTFX_CHECK(batch.add_quad(square_at(0, 0, 1), k_white, k_state_a));
    GLINTFX_CHECK(batch.add_triangles(triangle_at(2.0F, 3.0F, k_white), {}, k_state_a));
    const auto stored = batch.vertices();
    GLINTFX_CHECK_EQ(stored.size(), std::size_t{7});
    int with_q_of_one = 0;
    for (const batch_vertex &vertex : stored) {
        with_q_of_one += vertex.q == 1.0F ? 1 : 0;
    }
    GLINTFX_CHECK_EQ(with_q_of_one, 7);
    GLINTFX_CHECK(stored[2].u == 1.0F && stored[2].v == 1.0F);
    GLINTFX_CHECK(stored[4].u == 0.0F && stored[4].v == 0.0F);
}

// R2D-TEXTURE T4b-B1 (D-W8-470): a textured quad stores, at each corner, the coordinate TIMES its
// weight q and q itself last; the program divides. The weights are powers of two, so every product
// is exact in float and compared with ==.
GLINTFX_TEST(triangle_batch_textured_quad_stores_the_coordinate_times_q_and_q_last) {
    const glintfx::draw2d::quad_texture_corners reads{
        {{0.25F, 0.5F, 2.0F}, {0.75F, 0.5F, 0.5F}, {0.75F, 1.0F, 4.0F}, {0.25F, 1.0F, 0.25F}}};
    triangle_batch batch;
    GLINTFX_CHECK(batch.add_textured_quad(square_at(0, 0, 8), reads,
                                          gltfx_rgba{1.0F, 0.5F, 0.25F, 0.5F}, k_state_a));
    const auto v = batch.vertices();
    GLINTFX_CHECK_EQ(v.size(), std::size_t{4});
    GLINTFX_CHECK(v[0].u == 0.5F && v[0].v == 1.0F && v[0].q == 2.0F);      // top_left
    GLINTFX_CHECK(v[1].u == 0.375F && v[1].v == 0.25F && v[1].q == 0.5F);   // top_right
    GLINTFX_CHECK(v[2].u == 3.0F && v[2].v == 4.0F && v[2].q == 4.0F);      // bottom_right
    GLINTFX_CHECK(v[3].u == 0.0625F && v[3].v == 0.25F && v[3].q == 0.25F); // bottom_left
    // the tint is stored premultiplied, like every color
    GLINTFX_CHECK(v[0].r == 0.5F && v[0].g == 0.25F && v[0].b == 0.125F && v[0].a == 0.5F);
    GLINTFX_CHECK(v[3].x == 0.0F && v[3].y == 8.0F); // the positions are the corners by role
}

// add_quad() is add_textured_quad() with the coordinates by role and q = 1, byte for byte: what
// keeps every untextured piece drawn exactly as before the textured drawing came.
GLINTFX_TEST(triangle_batch_add_quad_is_the_textured_quad_with_the_role_corners) {
    const glintfx::draw2d::quad_texture_corners &role = glintfx::draw2d::k_role_texture_corners;
    GLINTFX_CHECK(role[0].u == 0.0F && role[0].v == 0.0F && role[0].q == 1.0F);
    GLINTFX_CHECK(role[1].u == 1.0F && role[1].v == 0.0F && role[1].q == 1.0F);
    GLINTFX_CHECK(role[2].u == 1.0F && role[2].v == 1.0F && role[2].q == 1.0F);
    GLINTFX_CHECK(role[3].u == 0.0F && role[3].v == 1.0F && role[3].q == 1.0F);
    triangle_batch plain;
    triangle_batch textured;
    const gltfx_rgba color{0.25F, 0.5F, 1.0F, 0.5F};
    GLINTFX_CHECK(plain.add_quad(square_at(3, 4, 5), color, k_state_a));
    GLINTFX_CHECK(textured.add_textured_quad(square_at(3, 4, 5), role, color, k_state_a));
    GLINTFX_CHECK_EQ(plain.vertices().size(), textured.vertices().size());
    // Bit for bit: each vertex as its nine 32-bit words (std::bit_cast, never memcmp of floats).
    using vertex_words = std::array<std::uint32_t, 9>;
    int equal_bits = 0;
    for (std::size_t i = 0; i < plain.vertices().size(); ++i) {
        const bool same = std::bit_cast<vertex_words>(plain.vertices()[i]) ==
                          std::bit_cast<vertex_words>(textured.vertices()[i]);
        equal_bits += same ? 1 : 0;
    }
    GLINTFX_CHECK_EQ(equal_bits, 4);
}

// A change of texture between two quads starts another run, in order, never reordered (D-W8-13):
// textures A, B, A are three runs, A, B, A.
GLINTFX_TEST(triangle_batch_textures_a_b_a_are_three_runs_in_order) {
    const batch_state a{1, 7, 0};
    const batch_state b{1, 9, 0};
    const auto &role = glintfx::draw2d::k_role_texture_corners;
    triangle_batch batch;
    GLINTFX_CHECK(batch.add_textured_quad(square_at(0, 0, 1), role, k_white, a));
    GLINTFX_CHECK(batch.add_textured_quad(square_at(1, 0, 1), role, k_white, b));
    GLINTFX_CHECK(batch.add_textured_quad(square_at(2, 0, 1), role, k_white, a));
    const auto runs = batch.runs();
    GLINTFX_CHECK_EQ(runs.size(), std::size_t{3});
    GLINTFX_CHECK(runs[0].state.texture == 7U && runs[0].first_index == 0U);
    GLINTFX_CHECK(runs[1].state.texture == 9U && runs[1].first_index == 6U);
    GLINTFX_CHECK(runs[2].state.texture == 7U && runs[2].first_index == 12U);
}
