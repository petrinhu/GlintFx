// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gl_version_policy.hpp"

#include <glintfx/core/err_code.hpp>

namespace glintfx::platform {

gltfx_rslt<void> validate_gl_context_version(std::uint32_t major, std::uint32_t minor,
                                             std::uint32_t profile_mask) noexcept {
    const bool version_ok = (major > 3) || (major == 3 && minor >= 3);
    const bool core_ok = (profile_mask & k_gl_context_core_profile_bit) != 0;

    if (version_ok && core_ok) {
        return gltfx_rslt<void>::ok();
    }

    return gltfx_rslt<void>::err(
        gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("gl_version"));
}

} // namespace glintfx::platform
