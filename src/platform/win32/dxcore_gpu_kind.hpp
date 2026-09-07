// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/win32/dxcore_adapter_enumeration.hpp"

// platform/win32/dxcore_gpu_kind.hpp - GL-GPU-KIND (docs/plano-w6b-
// fatias-5b-revisao.md sec. 3/4.1, D-W6b-37): the PURE Windows
// classifier - "given the DXCore properties of the matched adapter,
// and nothing else, what gltfx_gpu_kind is this". DXGI is OUT of this
// atom entirely (sec. 3's own "DXGI sai do desenho da 5b inteiro") -
// the system's own DXCore `IsHardware`/`IsIntegrated` decide, per
// adapter, WITHOUT counting how many adapters exist.
//
// THE FOUR RULES, IN ORDER, EXACTLY (D-W6b-37 regras 2-4):
//   1. not localized (no matched index) -> unknown
//   2. hardware_supported == false -> unknown
//   3. is_hardware == false -> software (checked BEFORE integrated -
//      "software vence integrada" is a real cell, sec. 4.3)
//   4. integrated_supported == false -> unknown
//   5. is_integrated == true -> shared; false -> dedicated
//
// WIN32-DOMAIN, NOT WIN32-ONLY: takes only dxcore_adapter_facts (plain
// data), compiles and is tested on all five platforms.

namespace glintfx::platform {

[[nodiscard]] gltfx_gpu_kind classify_dxcore_gpu(std::span<const dxcore_adapter_facts> facts,
                                                 std::optional<std::size_t> matched) noexcept;

} // namespace glintfx::platform
