// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/memory_separation_kind.hpp"

namespace glintfx::platform {

std::optional<gltfx_gpu_kind> classify_by_memory_separation(const gl_memory_facts &facts) noexcept {
    if (!facts.nvx_present) {
        return std::nullopt;
    }

    if (facts.dedicated_kb < 0) {
        return std::nullopt; // invalid reading, never treated as zero
    }

    return facts.dedicated_kb > 0 ? gltfx_gpu_kind::dedicated : gltfx_gpu_kind::shared;
}

} // namespace glintfx::platform
