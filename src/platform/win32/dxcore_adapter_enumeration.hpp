// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glintfx/core/err.hpp>

// platform/win32/dxcore_adapter_enumeration.hpp - GL-GPU-KIND (docs/
// plano-w6b-fatias-5b-revisao.md sec. 1.2/3, D-W6b-37, the lider's own
// order of 06/09/2026: "quem identifica as placas de video e o OS"):
// PLAIN DATA about ONE DXCore adapter, exactly as the system's own
// DXCoreAdapterProperty answers report it - what enumerate_dxcore_
// adapters() (dxcore_adapter_enumeration.cpp, Win32-only) fills, and
// what dxcore_adapter_match.hpp/dxcore_gpu_kind.hpp's own pure atoms
// consume.
//
// WIN32-DOMAIN, NOT WIN32-ONLY: this header itself never includes a
// Win32 or COM header - it compiles on all five platforms, the same
// shape drm_device_facts.hpp already established for the Linux side.
//
// `hardware_supported`/`integrated_supported` mirror IsPropertySupported()
// itself (Microsoft Learn's own DXCoreAdapterProperty page): a
// property this DXCore build cannot answer is NOT the same thing as
// "false" - D-W6b-37's own regra (3)/(4) reads `unknown` for either,
// never guesses.
struct dxcore_adapter_facts {
    std::string description; // DriverDescription (UTF-8)
    std::uint64_t luid = 0;  // InstanceLuid, packed low|high
    bool hardware_supported = false;
    bool is_hardware = false;
    bool integrated_supported = false;
    bool is_integrated = false;
};

// enumerate_dxcore_adapters() - Win32-only implementation (dxcore_
// adapter_enumeration.cpp): LoadLibraryW(L"dxcore.dll") +
// GetProcAddress("DXCoreCreateAdapterFactory") (NEVER a static import -
// tools/ci/check-dep-zero-win.ps1's own allowlist stays untouched,
// D-W6b-37), CreateAdapterList filtered by DXCORE_ADAPTER_ATTRIBUTE_
// D3D12_GRAPHICS, then IsPropertySupported+GetProperty for each of the
// four properties above, per adapter. dxcore.dll missing (Windows
// older than version 2004) -> gltfx_err_code::unsupported, rejected_
// value() == "dxcore" (D-W6b-37's own regra (1)).
[[nodiscard]] glintfx::gltfx_rslt<std::vector<dxcore_adapter_facts>>
enumerate_dxcore_adapters() noexcept;
