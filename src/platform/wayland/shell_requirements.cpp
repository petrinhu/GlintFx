// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/shell_requirements.hpp"

#include <glintfx/core/err_code.hpp>

namespace glintfx::platform {

gltfx_rslt<void> check_shell_requirements(const global_catalog &catalog) noexcept {
    if (catalog.find_by_interface("wl_compositor") == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::not_found).with_rejected_value("wl_compositor"));
    }
    if (catalog.find_by_interface("xdg_wm_base") == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::not_found).with_rejected_value("xdg_wm_base"));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
