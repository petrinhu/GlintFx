// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/dxcore_adapter_enumeration.hpp"

#if defined(_WIN32)

#include <cstring>

#include <windows.h>

// <initguid.h> BEFORE <dxcore.h>, in exactly this ONE translation unit
// (the only one that names DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS by
// value below): the standard Windows SDK mechanism for a DEFINE_GUID
// constant to be DEFINED here instead of just declared extern, so this
// file never needs to link an import library for it (GODS_LAWS.md L-07,
// dependency zero - dxcore.lib/dxguid.lib stay off the six-DLL import
// allowlist tools/ci/check-dep-zero-win.ps1 already measures).
// Documented behavior (learn.microsoft.com/windows/win32/directshow/
// directshow-faq#how-does-the-define_guid-macro-work): without
// <initguid.h>, DEFINE_GUID expands to `extern const GUID <name>;`
// (link error `unresolved external symbol` - exactly what this file hit
// before this line existed); with it, DEFINE_GUID expands to the real
// defining declaration. Two rules from the same source, both followed
// here: include it exactly ONCE across the whole link (a second
// inclusion for the same GUID is a compile error, "redefinition;
// multiple initialization"), and never from a precompiled header (this
// project has none). ORDER IS MANDATORY, not stylistic: this file never
// includes <dxcore_interface.h> (the actual header with the DEFINE_GUID
// line) directly - it arrives transitively through <dxcore.h> below.
// Reordering these two lines (alphabetizing the block, or a formatter
// that does it automatically) silently brings back the exact link
// error this comment exists to prevent - <initguid.h> MUST stay first.
// MEASURED: clang-format's default SortIncludes alphabetizes this pair
// on its own (dxcore.h before initguid.h) the moment they share a
// block with no blank line between them - the clang-format off/on
// pair below is not decoration, it is what keeps that from happening.
// clang-format off
#include <initguid.h>
#include <dxcore.h>
// clang-format on

#include <glintfx/core/err_code.hpp>

// dxcore_adapter_enumeration.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-
// 5b-revisao.md sec. 1.2/3, D-W6b-37, the lider's own order of
// 06/09/2026: "quem identifica as placas de video e o OS"): the real
// DXCore COM enumeration - LoadLibraryW(L"dxcore.dll") +
// GetProcAddress("DXCoreCreateAdapterFactory") (NEVER a static import:
// tools/ci/check-dep-zero-win.ps1's own allowlist stays untouched,
// D-W6b-37 regra (1)).
//
// <dxcore.h>/<dxcore_interface.h> ARE THE REAL WINDOWS SDK HEADERS
// (present in this project's own build toolchain, /opt/msvc/Windows
// Kits/10/Include/.../um/ inside glintfx-msvc:latest - CONFIRMED
// 06/09/2026), the SAME "API do sistema" category <windows.h> already
// is for GODS_LAWS.md L-07's dependency-zero rule - NOT a vendored
// third-party header. Only the ONE free function this file actually
// calls dynamically (DXCoreCreateAdapterFactory) is resolved through
// LoadLibraryW/GetProcAddress, matching its EXACT declared signature
// from the header (STDAPI, i.e. extern "C" HRESULT WINAPI) - the
// header is never linked against (no dxcore.lib), only its TYPE
// DECLARATIONS are used, so <dxcore.h>'s own template overloads
// (CreateAdapterList<T>(...), GetAdapter<T>(...), GetProperty<T>(...))
// work through ordinary virtual dispatch once a real interface pointer
// is in hand.
//
// *** CORRIGIDO 06/09/2026 (achado do team-lead, apurado pela consulta
// MCP Microsoft Learn que a primeira versao deste arquivo disparou e
// nao esperou): a primeira versao declarava um vtable a mao E TRES
// GUIDs de interface transcritos de memoria de treinamento (IID_
// IDXCoreAdapterFactory/List/Adapter) - dois estavam ERRADOS (so IID_
// IDXCoreAdapterList batia por coincidencia), e o proprio GUID de
// atributo (DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS) tambem estava
// errado. A ordem do team-lead, aplicada aqui: essas TRES IIDs de
// interface NAO SAO PARA SER TRANSCRITAS DE FORMA NENHUMA - o
// cabecalho oficial nao as publica como constantes autonomas para
// serem citadas, ele as anexa a CADA interface via MIDL_INTERFACE(...),
// e o caminho certo e deixar o COMPILADOR deduzi-las por __uuidof()
// (que os overloads de template acima ja fazem, via IID_PPV_ARGS) -
// nunca escrever o hexadecimal a mao. O risco de GUID errado
// desaparece POR CONSTRUCAO, nao fica so declarado. O UNICO GUID
// citado por nome neste arquivo agora e' DXCORE_ADAPTER_ATTRIBUTE_
// D3D12_GRAPHICS, que E' uma constante publica de verdade (DEFINE_GUID
// em dxcore_interface.h) - conferida contra a doc oficial pelo team-
// lead: 0c9ece4d-2f6e-4f01-8c96-e89e331b47b1.
//
// A ORDEM DO VTABLE nao e' mais um risco deste arquivo: IDXCoreAdapter/
// IDXCoreAdapterList/IDXCoreAdapterFactory sao usadas como os TIPOS
// REAIS do cabecalho (chamada de metodo virtual C++ comum), nao um
// struct de vtable desenhado a mao - um erro de ordem agora seria um
// erro de COMPILACAO contra o tipo oficial, nunca uma corrupcao de
// memoria silenciosa em tempo de execucao.
//
// NENHUM teste nesta arvore roda este arquivo contra hardware Windows
// real (D-W6b-39 item 4: `GL-GPU-KIND-HW-EVIDENCE` ja esta na INBOX do
// TODO.md).

namespace {

using dxcore_create_adapter_factory_fn = HRESULT(WINAPI *)(REFIID riid, void **ppvFactory);

// Reads ONE string property (DriverDescription) into a std::string -
// GetPropertySize() first (the two-call idiom the header's own worked
// examples use for a variable-length property - there is no template
// overload for this one, unlike the fixed-size GetProperty<T>() the
// rest of this file uses), then GetProperty() into a buffer sized
// exactly that.
[[nodiscard]] std::string read_driver_description(IDXCoreAdapter *adapter) noexcept {
    if (!adapter->IsPropertySupported(DXCoreAdapterProperty::DriverDescription)) {
        return {};
    }
    std::size_t size = 0;
    if (adapter->GetPropertySize(DXCoreAdapterProperty::DriverDescription, &size) != S_OK ||
        size == 0) {
        return {};
    }
    std::string buffer(size, '\0');
    if (adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, size, buffer.data()) !=
        S_OK) {
        return {};
    }
    // DriverDescription is a NUL-terminated ASCII/UTF-8 C string inside
    // the buffer (Microsoft Learn) - trim the trailing NUL(s)
    // GetPropertySize() already counted into `size`.
    const std::size_t nul_pos = buffer.find('\0');
    if (nul_pos != std::string::npos) {
        buffer.resize(nul_pos);
    }
    return buffer;
}

[[nodiscard]] std::uint64_t read_instance_luid(IDXCoreAdapter *adapter) noexcept {
    if (!adapter->IsPropertySupported(DXCoreAdapterProperty::InstanceLuid)) {
        return 0;
    }
    LUID luid{};
    if (adapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &luid) != S_OK) {
        return 0;
    }
    std::uint64_t value = 0;
    std::memcpy(&value, &luid, sizeof(value));
    return value;
}

[[nodiscard]] dxcore_adapter_facts read_adapter_facts(IDXCoreAdapter *adapter) noexcept {
    dxcore_adapter_facts facts;
    facts.description = read_driver_description(adapter);
    facts.luid = read_instance_luid(adapter);

    facts.hardware_supported = adapter->IsPropertySupported(DXCoreAdapterProperty::IsHardware);
    if (facts.hardware_supported) {
        bool is_hardware = false;
        if (adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &is_hardware) == S_OK) {
            facts.is_hardware = is_hardware;
        } else {
            facts.hardware_supported = false;
        }
    }

    facts.integrated_supported = adapter->IsPropertySupported(DXCoreAdapterProperty::IsIntegrated);
    if (facts.integrated_supported) {
        bool is_integrated = false;
        if (adapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &is_integrated) == S_OK) {
            facts.is_integrated = is_integrated;
        } else {
            facts.integrated_supported = false;
        }
    }

    return facts;
}

} // namespace

glintfx::gltfx_rslt<std::vector<dxcore_adapter_facts>> enumerate_dxcore_adapters() noexcept {
    using glintfx::gltfx_err;
    using glintfx::gltfx_err_code;
    using glintfx::gltfx_rslt;

    HMODULE dxcore_module = ::LoadLibraryW(L"dxcore.dll");
    if (dxcore_module == nullptr) {
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    auto create_factory = reinterpret_cast<dxcore_create_adapter_factory_fn>(
        ::GetProcAddress(dxcore_module, "DXCoreCreateAdapterFactory"));
    if (create_factory == nullptr) {
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    // __uuidof(IDXCoreAdapterFactory) - the compiler deduces the IID
    // from the REAL type's own MIDL_INTERFACE(...) attribute (this
    // file's own top comment): never a hand-transcribed hex literal.
    IDXCoreAdapterFactory *factory = nullptr;
    if (create_factory(__uuidof(IDXCoreAdapterFactory), reinterpret_cast<void **>(&factory)) !=
            S_OK ||
        factory == nullptr) {
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    // DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS - the ONE GUID this file
    // still names, a real published constant (this file's own top
    // comment: confirmed against Microsoft Learn, 0c9ece4d-2f6e-4f01-
    // 8c96-e89e331b47b1).
    IDXCoreAdapterList *adapter_list = nullptr;
    const GUID attribute = DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS;
    if (FAILED(factory->CreateAdapterList(1, &attribute, &adapter_list)) ||
        adapter_list == nullptr) {
        factory->Release();
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    const std::uint32_t adapter_count = adapter_list->GetAdapterCount();
    std::vector<dxcore_adapter_facts> facts;
    facts.reserve(adapter_count);

    for (std::uint32_t i = 0; i < adapter_count; ++i) {
        IDXCoreAdapter *adapter = nullptr;
        if (SUCCEEDED(adapter_list->GetAdapter(i, &adapter)) && adapter != nullptr) {
            facts.push_back(read_adapter_facts(adapter));
            adapter->Release();
        }
    }

    adapter_list->Release();
    factory->Release();

    return gltfx_rslt<std::vector<dxcore_adapter_facts>>::ok(std::move(facts));
}

#endif // defined(_WIN32)
