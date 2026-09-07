// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <print>
#include <string_view>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_state.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gpu_kind_state_test.cpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, sec. 10.3, D-W6b-13, GODS_LAWS.md L-20/L-40): the TDD red/
// green witness for glintfx::platform::gpu_kind_state (src/platform/
// gl/gpu_kind_state.hpp) - the four cases the plan's own fatia 2b row
// names.
//
// RED, SEEN: before gpu_kind_state.hpp existed, this file's own
// #include line failed to compile.

using glintfx::gltfx_gpu_info;
using glintfx::gltfx_gpu_kind;
using glintfx::platform::gpu_kind_state;

GLINTFX_TEST(gpu_kind_state_is_born_unknown_with_an_empty_name) {
    const gpu_kind_state state;
    const gltfx_gpu_info info = state.read();
    GLINTFX_CHECK(info.kind == gltfx_gpu_kind::unknown);
    GLINTFX_CHECK(info.name.empty());
}

GLINTFX_TEST(gpu_kind_state_learns_software_from_what_the_adapter_tells_it) {
    gpu_kind_state state;
    state.learn(gltfx_gpu_kind::software, "llvmpipe");
    const gltfx_gpu_info info = state.read();
    GLINTFX_CHECK(info.kind == gltfx_gpu_kind::software);
    GLINTFX_CHECK(info.name == "llvmpipe");
}

GLINTFX_TEST(gpu_kind_state_never_invents_a_name_an_empty_one_stays_empty) {
    gpu_kind_state state;
    state.learn(gltfx_gpu_kind::dedicated, std::string_view{});
    const gltfx_gpu_info info = state.read();
    GLINTFX_CHECK(info.kind == gltfx_gpu_kind::dedicated);
    GLINTFX_CHECK(info.name.empty());
}

// GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec. 4.1, D-W6b-33): learn()
// gained a third argument, enumeration_index - defaults to the
// sentinel so every OLDER two-argument call site above keeps reading
// back k_gltfx_gpu_index_unknown, and a caller that DOES pass a real
// index gets it back unchanged.
GLINTFX_TEST(gpu_kind_state_learn_without_index_reads_back_the_sentinel) {
    gpu_kind_state state;
    state.learn(gltfx_gpu_kind::dedicated, "NVIDIA GeForce RTX 3050 Laptop GPU");
    GLINTFX_CHECK_EQ(state.read().enumeration_index, glintfx::k_gltfx_gpu_index_unknown);
}

GLINTFX_TEST(gpu_kind_state_learn_with_index_reads_it_back_unchanged) {
    gpu_kind_state state;
    state.learn(gltfx_gpu_kind::dedicated, "NVIDIA GeForce RTX 3050 Laptop GPU", 1);
    GLINTFX_CHECK_EQ(state.read().enumeration_index, std::uint32_t{1});
}

GLINTFX_TEST(gpu_kind_state_closed_four_value_enumeration) {
    const std::array<gltfx_gpu_kind, 4> kinds{
        gltfx_gpu_kind::unknown,
        gltfx_gpu_kind::software,
        gltfx_gpu_kind::shared,
        gltfx_gpu_kind::dedicated,
    };

    int analyzed = 0;
    for (const gltfx_gpu_kind kind : kinds) {
        gpu_kind_state state;
        state.learn(kind, "example");
        GLINTFX_CHECK(state.read().kind == kind);
        ++analyzed;
    }
    GLINTFX_CHECK_EQ(analyzed, static_cast<int>(kinds.size()));
    std::println("gpu_kind_state_closed_four_value_enumeration: {} kinds analyzed, {} found",
                 analyzed, kinds.size());
}
