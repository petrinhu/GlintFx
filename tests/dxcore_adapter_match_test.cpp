// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <optional>
#include <print>

#include "platform/win32/dxcore_adapter_match.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// dxcore_adapter_match_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-
// 5.md sec. 4.4; docs/plano-w6b-fatias-5b-revisao.md sec. 4.3): the
// closed 7-cell enumeration for match_dxcore_adapter() - LUID first,
// substring reserve, and the ambiguity boundary that keeps a wrong
// guess from ever standing in for "not found" (GODS_LAWS.md L-40).
//
// RED, SEEN: before dxcore_adapter_match.{hpp,cpp} existed, this
// file's own #include line failed to compile.
//
// dxcore_adapter_facts lives at GLOBAL scope (dxcore_adapter_
// enumeration.hpp's own header comment, the same "plain data, no
// namespace" shape drm_device_facts/egl_device_facts already use) -
// every field is spelled out below (never partial designated init) so
// -Wmissing-field-initializers stays quiet under GLINTFX_WERROR.

using glintfx::platform::match_dxcore_adapter;

namespace {
dxcore_adapter_facts make_facts(std::string description, std::uint64_t luid) {
    return dxcore_adapter_facts{.description = std::move(description),
                                .luid = luid,
                                .hardware_supported = false,
                                .is_hardware = false,
                                .integrated_supported = false,
                                .is_integrated = false};
}
} // namespace

GLINTFX_TEST(match_dxcore_adapter_closed_7_cell_enumeration) {
    int analyzed = 0;

    {
        // LUID casa um.
        const std::array<dxcore_adapter_facts, 2> facts{
            make_facts("NVIDIA GeForce RTX 3050 Laptop GPU", 111),
            make_facts("Intel(R) Iris(R) Xe Graphics", 222),
        };
        GLINTFX_CHECK(match_dxcore_adapter(facts, 111, "irrelevant") ==
                      std::optional<std::size_t>{0});
        ++analyzed;
    }
    {
        // LUID casa dois iguais -> nenhum (nunca cai pra substring).
        const std::array<dxcore_adapter_facts, 2> facts{
            make_facts("GPU A", 111),
            make_facts("GPU A", 111),
        };
        GLINTFX_CHECK(match_dxcore_adapter(facts, 111, "GPU A") == std::nullopt);
        ++analyzed;
    }
    {
        // LUID nao casa nenhum, substring unico -> esse.
        const std::array<dxcore_adapter_facts, 2> facts{
            make_facts("NVIDIA GeForce RTX 3050 Laptop GPU", 111),
            make_facts("Intel(R) Iris(R) Xe Graphics", 222),
        };
        GLINTFX_CHECK(
            match_dxcore_adapter(facts, 999, "NVIDIA GeForce RTX 3050 Laptop GPU/PCIe/SSE2") ==
            std::optional<std::size_t>{0});
        ++analyzed;
    }
    {
        // Substring casa dois -> nenhum.
        const std::array<dxcore_adapter_facts, 2> facts{
            make_facts("GPU", 0),
            make_facts("GPU", 0),
        };
        GLINTFX_CHECK(match_dxcore_adapter(facts, 0, "GPU Renderer") == std::nullopt);
        ++analyzed;
    }
    {
        // Substring vazia (description vazia) -> nenhum.
        const std::array<dxcore_adapter_facts, 1> facts{make_facts("", 0)};
        GLINTFX_CHECK(match_dxcore_adapter(facts, 0, "Anything") == std::nullopt);
        ++analyzed;
    }
    {
        // Description igual mas GL_RENDERER com sufixo (o caso NVIDIA
        // "/PCIe/SSE2") -> casa por substring.
        const std::array<dxcore_adapter_facts, 1> facts{
            make_facts("NVIDIA GeForce RTX 3050 Laptop GPU", 0)};
        GLINTFX_CHECK(
            match_dxcore_adapter(facts, 0, "NVIDIA GeForce RTX 3050 Laptop GPU/PCIe/SSE2") ==
            std::optional<std::size_t>{0});
        ++analyzed;
    }
    {
        // Sem LUID e sem substring -> nenhum.
        const std::array<dxcore_adapter_facts, 1> facts{make_facts("Intel Iris", 0)};
        GLINTFX_CHECK(match_dxcore_adapter(facts, 0, "Completely Different") == std::nullopt);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 7);
    std::println("dxcore_adapter_match_test: {} celula(s) conferida(s)", analyzed);
}
