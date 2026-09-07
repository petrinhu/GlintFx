// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gl_memory_facts.hpp"

// platform/gl/memory_separation_kind.hpp - GL-GPU-KIND, "via 2" (docs/
// plano-w6b-fatias-5b-revisao.md sec. 1.5, D-W6b-38): the PURE
// classifier over gl_memory_facts - "does this device's own memory
// pool exist separately from system RAM, according to the driver
// itself". `dedicated_kb > 0` -> `dedicated` (this machine's own
// NVIDIA: a real 4 GiB pool, sec. 1.5); `dedicated_kb == 0` -> `shared`
// (this machine's own llvmpipe: the driver reports the pool as the
// WHOLE system RAM, sec. 1.5's own table); extension absent, or a
// negative reading (an invalid answer, never treated as zero) ->
// std::nullopt, "no opinion" - the caller falls through to `unknown`,
// never a guess.
//
// PLATFORM-AGNOSTIC (D-W6b-38's own header comment): also WGL's own
// reserve when DXCore does not answer IsIntegrated.

namespace glintfx::platform {

[[nodiscard]] std::optional<gltfx_gpu_kind>
classify_by_memory_separation(const gl_memory_facts &facts) noexcept;

} // namespace glintfx::platform
