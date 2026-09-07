// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <print>
#include <vector>

#include "platform/wayland/egl_device_dedup.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// egl_device_dedup_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md
// sec. 4.1/4.4, F3, D-W6b-33): the 5-cell enumeration for dedup_egl_
// devices() - the exact scenario F3 measured on the lider's own
// machine (the NVIDIA EGLDeviceEXT counted twice), plus the two
// boundaries that scenario depends on (GODS_LAWS.md L-40).
//
// RED, SEEN: before egl_device_dedup.{hpp,cpp} existed, this file's
// own #include line failed to compile.
//
// egl_device_facts is plain data at GLOBAL scope - every field spelled
// out below (never partial designated init) so -Wmissing-field-
// initializers stays quiet under GLINTFX_WERROR.

using glintfx::platform::dedup_egl_devices;

namespace {
egl_device_facts make_facts(bool software, std::string render_node, std::string primary_node) {
    return egl_device_facts{.queried = true,
                            .software = software,
                            .render_node = std::move(render_node),
                            .primary_node = std::move(primary_node)};
}
} // namespace

GLINTFX_TEST(egl_device_dedup_closed_5_cell_enumeration) {
    int analyzed = 0;

    {
        // Dois entries, mesmo render node (F3: "a mesma NVIDIA de
        // novo") -> 1 survivor.
        const std::array<egl_device_facts, 2> devices{
            make_facts(false, "/dev/dri/renderD129", ""),
            make_facts(false, "/dev/dri/renderD129", ""),
        };
        const std::vector<std::size_t> survivors = dedup_egl_devices(devices);
        GLINTFX_CHECK_EQ(survivors.size(), std::size_t{1});
        GLINTFX_CHECK_EQ(survivors.at(0), std::size_t{0});
        ++analyzed;
    }

    {
        // Dois render nodes diferentes -> 2 survivors.
        const std::array<egl_device_facts, 2> devices{
            make_facts(false, "/dev/dri/renderD129", ""),
            make_facts(false, "/dev/dri/renderD128", ""),
        };
        GLINTFX_CHECK_EQ(dedup_egl_devices(devices).size(), std::size_t{2});
        ++analyzed;
    }

    {
        // Render node vazio nos dois, primary node igual -> colapsa
        // via a reserva de primary node (D-W6b-30).
        const std::array<egl_device_facts, 2> devices{
            make_facts(false, "", "/dev/dri/card0"),
            make_facts(false, "", "/dev/dri/card0"),
        };
        GLINTFX_CHECK_EQ(dedup_egl_devices(devices).size(), std::size_t{1});
        ++analyzed;
    }

    {
        // Software duas vezes, sem no nenhum -> nunca colapsa (2
        // survivors) - uma chave vazia nao e uma chave compartilhada.
        const std::array<egl_device_facts, 2> devices{
            make_facts(true, "", ""),
            make_facts(true, "", ""),
        };
        GLINTFX_CHECK_EQ(dedup_egl_devices(devices).size(), std::size_t{2});
        ++analyzed;
    }

    {
        // Lista vazia -> ok, zero survivors (resposta valida, nao
        // varredura quebrada - o piso vive no chamador publico,
        // count() >= 1).
        const std::vector<egl_device_facts> devices;
        GLINTFX_CHECK_EQ(dedup_egl_devices(devices).size(), std::size_t{0});
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 5);
    std::println("egl_device_dedup_test: {} celula(s) conferida(s)", analyzed);
}
