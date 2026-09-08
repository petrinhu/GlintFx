// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_device_dedup.hpp"

#include <new>

namespace glintfx::platform {

std::vector<std::size_t> dedup_egl_devices(std::span<const egl_device_facts> devices) noexcept {
    // GODS_LAWS.md L-22: push_back() abaixo (nas duas listas) pode
    // lancar std::bad_alloc dentro de uma funcao noexcept - a mesma
    // guarda que este arquivo's own chamador (egl_device_enumeration.
    // cpp's own enumerate_egl_devices()) ja aplica em volta desta
    // propria chamada. Esta funcao NAO tem canal de erro (retorna um
    // std::vector<std::size_t> puro, indexado diretamente pelo
    // chamador contra `raw`) - o desfecho honesto que preserva esse
    // contrato sem arriscar um indice invalido e degradar para "zero
    // sobreviventes", o mesmo estado que o proprio teste vazio
    // (egl_device_dedup_test.cpp) ja prova como valido e seguro.
    //
    // WIN-DEBUG-CTORALLOC (07/09/2026, gemeo do achado em gfx_open_
    // only_fixation.cpp): `survivors` precisa nascer DENTRO do try{},
    // ao lado de `seen_keys` - mesma razao (construtor padrao de
    // std::vector pode alocar sob MSVC Debug).
    try {
        std::vector<std::size_t> survivors;
        std::vector<std::string> seen_keys;

        for (std::size_t i = 0; i < devices.size(); ++i) {
            const egl_device_facts &device = devices[i];
            const std::string &key =
                !device.render_node.empty() ? device.render_node : device.primary_node;

            if (key.empty()) {
                // No node at all (e.g. a software device) - never collapses
                // with anything else, order preserved.
                survivors.push_back(i);
                continue;
            }

            bool already_seen = false;
            for (const std::string &seen_key : seen_keys) {
                if (seen_key == key) {
                    already_seen = true;
                    break;
                }
            }

            if (!already_seen) {
                seen_keys.push_back(key);
                survivors.push_back(i);
            }
        }

        return survivors;
    } catch (const std::bad_alloc &) {
        return {};
    }
}

} // namespace glintfx::platform
