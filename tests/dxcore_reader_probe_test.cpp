// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <vector>

#include "platform/win32/dxcore_adapter_enumeration.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// dxcore_reader_probe_test.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-
// 5b-revisao.md sec. 4.6): "the leitor ran against the real system and
// this is what it saw" - the Windows half of the pair egl_device_
// reader_probe.cpp (tests/container/) is the Linux half of (paired in
// tests/parity_aliases.txt: dxcore_reader_probe_test|egl_device_
// reader_probe - the SAME coverage, different mechanism, under two
// names). gl_context_parity_test.cpp is PUBLIC-API-ONLY and cannot see
// `dxcore_loaded`/`is_hardware`/`is_integrated` raw - this probe is
// the one place that reads enumerate_dxcore_adapters() directly.
//
// `dxcore_loaded`: this atom's own gltfx_rslt<...> collapses every
// failure of the pipeline (LoadLibraryW, GetProcAddress, CreateAdapter
// Factory, CreateAdapterList) into ONE `unsupported` error - there is
// no finer-grained signal to read `dxcore_loaded` from than "did the
// whole call succeed" (has_value()). Declared here, not hidden: a
// future fatia that wants to distinguish "dxcore.dll missing" from
// "CreateAdapterList itself failed" would need enumerate_dxcore_
// adapters() to expose that split first.
//
// `is_hardware_raw`/`is_integrated_raw`: 0 = false, 1 = true, 9 =
// property not supported (dxcore_adapter_facts::hardware_supported/
// integrated_supported false) - read off the FIRST adapter DXCore's
// own D3D12-graphics-filtered list returns (this executor's own
// software adapter, F4 of the plan).
//
// ONLY `dxcore_adapter_count >= 1` IS ASSERTED, and only WHEN `dxcore_
// loaded == 1` (GODS_LAWS.md L-40's own non-empty floor) - every other
// value here is printed, never compared, exactly like gl_context_
// parity_test.cpp's own gpu_kind_raw (sec. 4.6's own correction: the
// assertion enters after the first real run, never before).

GLINTFX_TEST(dxcore_reader_probe_prints_and_asserts_the_non_empty_floor) {
    const glintfx::gltfx_rslt<std::vector<dxcore_adapter_facts>> result =
        enumerate_dxcore_adapters();
    const bool loaded = result.has_value();
    std::printf("MEASURED dxcore_reader_probe_test.dxcore_loaded=%d\n", loaded ? 1 : 0);

    if (!loaded) {
        std::printf("dxcore_reader_probe_test: enumerate_dxcore_adapters() unsupported "
                    "(rejected_value=%s) - nada mais a medir\n",
                    std::string(result.err().rejected_value()).c_str());
        return;
    }

    const std::vector<dxcore_adapter_facts> &adapters = result.value();
    std::printf("MEASURED dxcore_reader_probe_test.dxcore_adapter_count=%zu\n", adapters.size());
    GLINTFX_CHECK(!adapters.empty());
    if (adapters.empty()) {
        return;
    }

    const dxcore_adapter_facts &first = adapters.front();
    std::printf("MEASURED dxcore_reader_probe_test.dxcore_luid_nonzero=%d\n",
                first.luid != 0 ? 1 : 0);

    const int is_hardware_raw = !first.hardware_supported ? 9 : (first.is_hardware ? 1 : 0);
    const int is_integrated_raw = !first.integrated_supported ? 9 : (first.is_integrated ? 1 : 0);
    std::printf("MEASURED dxcore_reader_probe_test.is_hardware_raw=%d\n", is_hardware_raw);
    std::printf("MEASURED dxcore_reader_probe_test.is_integrated_raw=%d\n", is_integrated_raw);
}
