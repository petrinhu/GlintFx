// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gpu_kind_exclusion.hpp"

namespace glintfx::platform {

std::vector<gltfx_gpu_kind>
apply_gpu_kind_exclusion(std::span<const gltfx_gpu_kind> kinds) noexcept {
    std::vector<gltfx_gpu_kind> result(kinds.begin(), kinds.end());

    bool any_kernel_shared = false;
    for (const gltfx_gpu_kind kind : kinds) {
        if (kind == gltfx_gpu_kind::shared) {
            any_kernel_shared = true;
            break;
        }
    }

    if (!any_kernel_shared) {
        return result; // nothing to exclude by - via 2 or unknown decide next
    }

    for (gltfx_gpu_kind &kind : result) {
        if (kind == gltfx_gpu_kind::unknown) {
            kind = gltfx_gpu_kind::dedicated;
        }
    }

    return result;
}

} // namespace glintfx::platform
