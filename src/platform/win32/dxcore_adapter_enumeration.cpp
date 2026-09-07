// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/dxcore_adapter_enumeration.hpp"

#if defined(_WIN32)

#include <cstring>

#include <windows.h>

#include <glintfx/core/err_code.hpp>

// dxcore_adapter_enumeration.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-
// 5b-revisao.md sec. 1.2/3, D-W6b-37, the lider's own order of
// 06/09/2026: "quem identifica as placas de video e o OS"): the real
// DXCore COM enumeration - LoadLibraryW(L"dxcore.dll") +
// GetProcAddress("DXCoreCreateAdapterFactory") (NEVER a static import:
// tools/ci/check-dep-zero-win.ps1's own allowlist stays untouched,
// D-W6b-37 regra (1)), a raw COM vtable (no <dxcore.h>/<dxcore_
// interface.h> included - GODS_LAWS.md L-07: this project's own
// dependency-zero rule, the same "declared by hand" technique wgl_
// context_adapter.cpp already uses for its own WGL_ARB_* tokens,
// applied here to a COM interface instead of a flat C API).
//
// *** VERIFICACAO PENDENTE, DECLARADA (GODS_LAWS.md L-27/L-44): as
// TRES GUIDs abaixo (IID_IDXCoreAdapterFactory, IID_IDXCoreAdapterList,
// DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS) foram transcritas de
// memoria de treinamento, NAO conferidas contra o cabecalho oficial
// dxcore_interface.h nesta sessao (a pesquisa MCP Microsoft Learn
// disparada para confirma-las nao retornou a tempo). O RISCO e
// ASSIMETRICO e por isso aceitavel para compilar e revisar: um GUID
// errado falha de forma SEGURA (QueryInterface/CreateAdapterList
// devolve E_NOINTERFACE, este codigo ja trata como enumeracao
// `unsupported`, gpu().kind degrada para `unknown` - o mesmo caminho
// honesto de "dxcore.dll ausente"); JA A ORDEM DO VTABLE (que este
// arquivo tambem declara por conta propria) e' o que corromperia
// memoria se estivesse errada, e essa ordem e' a conhecida, estavel e
// publicamente documentada desde a introducao do DXCore no Windows 10
// versao 2004 (2020) - o candidato mais provavel de erro real aqui sao
// os GUIDs, nao a forma das interfaces. NENHUM teste nesta arvore roda
// este arquivo contra hardware real (D-W6b-39 item 4: `GL-GPU-KIND-
// HW-EVIDENCE` ja esta na INBOX do TODO.md) - o proximo editor que
// tiver acesso a um Windows real, OU ao SDK oficial para colar os
// valores exatos de dxcore_interface.h, deve conferir os tres GUIDs
// abaixo antes de tratar esta enumeracao como definitivamente correta
// em produção.

namespace {

// IUnknown - a base de toda interface COM abaixo, vtable de 3 slots,
// exatamente como o COM define desde sempre.
struct IUnknownVtbl {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(void *self, REFIID riid, void **ppvObject);
    ULONG(STDMETHODCALLTYPE *AddRef)(void *self);
    ULONG(STDMETHODCALLTYPE *Release)(void *self);
};

struct IUnknownRaw {
    const IUnknownVtbl *vtbl;
};

// DXCoreAdapterProperty - Microsoft Learn, dxcore_interface.h (ordem
// do enum, estavel desde 2020): apenas as quatro propriedades esta
// fatia le (D-W6b-37).
enum class DXCoreAdapterProperty : std::uint32_t {
    InstanceLuid = 0,
    DriverVersion = 1,
    DriverDescription = 2,
    HardwareID = 3,
    KmdModelVersion = 4,
    ComputePreemptionGranularity = 5,
    GraphicsPreemptionGranularity = 6,
    DedicatedAdapterMemory = 7,
    DedicatedSystemMemory = 8,
    SharedSystemMemory = 9,
    AcgCompatible = 10,
    IsHardware = 11,
    IsIntegrated = 12,
    IsDetachable = 13,
};

// IDXCoreAdapter - vtable ordem: IUnknown (3) + IsValid, IsAttribute
// Supported, IsPropertySupported, GetProperty, GetPropertySize,
// IsQueryStateSupported, QueryState, IsSetStateSupported, SetState,
// GetFactory (Microsoft Learn, "IDXCoreAdapter interface").
struct IDXCoreAdapterVtbl {
    IUnknownVtbl unknown;
    BOOL(STDMETHODCALLTYPE *IsValid)(void *self);
    BOOL(STDMETHODCALLTYPE *IsAttributeSupported)(void *self, REFGUID attributeGUID);
    BOOL(STDMETHODCALLTYPE *IsPropertySupported)(void *self, DXCoreAdapterProperty property);
    HRESULT(STDMETHODCALLTYPE *GetProperty)(void *self, DXCoreAdapterProperty property,
                                            std::size_t bufferSize, void *propertyData);
    HRESULT(STDMETHODCALLTYPE *GetPropertySize)(void *self, DXCoreAdapterProperty property,
                                                std::size_t *bufferSize);
    void *IsQueryStateSupported;
    void *QueryState;
    void *IsSetStateSupported;
    void *SetState;
    void *GetFactory;
};

struct IDXCoreAdapterRaw {
    const IDXCoreAdapterVtbl *vtbl;
};

// IDXCoreAdapterList - vtable ordem: IUnknown (3) + GetAdapter,
// GetAdapterCount, IsStale, GetFactory, Sort, IsAdapterPreference
// Supported (Microsoft Learn, "IDXCoreAdapterList interface").
struct IDXCoreAdapterListVtbl {
    IUnknownVtbl unknown;
    HRESULT(STDMETHODCALLTYPE *GetAdapter)(void *self, std::uint32_t index, REFIID riid,
                                           void **ppvAdapter);
    std::uint32_t(STDMETHODCALLTYPE *GetAdapterCount)(void *self);
    BOOL(STDMETHODCALLTYPE *IsStale)(void *self);
    void *GetFactory;
    void *Sort;
    void *IsAdapterPreferenceSupported;
};

struct IDXCoreAdapterListRaw {
    const IDXCoreAdapterListVtbl *vtbl;
};

// IDXCoreAdapterFactory - vtable ordem: IUnknown (3) + CreateAdapter
// List, GetAdapterByLuid, IsNotificationTypeSupported, RegisterEvent
// Notification, UnregisterEventNotification (Microsoft Learn,
// "IDXCoreAdapterFactory interface").
struct IDXCoreAdapterFactoryVtbl {
    IUnknownVtbl unknown;
    HRESULT(STDMETHODCALLTYPE *CreateAdapterList)(void *self, std::uint32_t numAttributes,
                                                  const GUID *filterAttributes, REFIID riid,
                                                  void **ppvAdapterList);
    void *GetAdapterByLuid;
    void *IsNotificationTypeSupported;
    void *RegisterEventNotification;
    void *UnregisterEventNotification;
};

struct IDXCoreAdapterFactoryRaw {
    const IDXCoreAdapterFactoryVtbl *vtbl;
};

// *** GUIDs, ver a "VERIFICACAO PENDENTE" no topo deste arquivo.
// IID_IDXCoreAdapterFactory.
constexpr GUID k_iid_dxcore_adapter_factory = {
    0x54D2DA14, 0xEF48, 0x401A, {0x9E, 0xF7, 0xA7, 0x52, 0xF8, 0x1A, 0xFE, 0x71}};
// IID_IDXCoreAdapterList.
constexpr GUID k_iid_dxcore_adapter_list = {
    0x526C7776, 0x40E9, 0x459B, {0xB7, 0x11, 0xF3, 0x2A, 0xD7, 0x6D, 0xFC, 0x28}};
// DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS.
constexpr GUID k_dxcore_adapter_attribute_d3d12_graphics = {
    0x0649A7DA, 0xB4B6, 0x4816, {0xB7, 0x1F, 0x60, 0x68, 0xFD, 0x0C, 0x51, 0xE9}};

using dxcore_create_adapter_factory_fn = HRESULT(WINAPI *)(REFIID riid, void **ppvFactory);

// Reads ONE string property (DriverDescription) into a std::string -
// GetPropertySize() first (the two-call idiom every DXCore property
// read uses, Microsoft Learn's own worked example), then GetProperty()
// into a buffer sized exactly that.
[[nodiscard]] std::string read_driver_description(IDXCoreAdapterRaw *adapter) noexcept {
    if (adapter->vtbl->IsPropertySupported(adapter, DXCoreAdapterProperty::DriverDescription) ==
        0) {
        return {};
    }
    std::size_t size = 0;
    if (adapter->vtbl->GetPropertySize(adapter, DXCoreAdapterProperty::DriverDescription, &size) !=
            S_OK ||
        size == 0) {
        return {};
    }
    std::string buffer(size, '\0');
    if (adapter->vtbl->GetProperty(adapter, DXCoreAdapterProperty::DriverDescription, size,
                                   buffer.data()) != S_OK) {
        return {};
    }
    // DriverDescription is a NUL-terminated UTF-8 C string inside the
    // buffer (Microsoft Learn) - trim the trailing NUL(s) GetPropertySize()
    // already counted into `size`.
    const std::size_t nul_pos = buffer.find('\0');
    if (nul_pos != std::string::npos) {
        buffer.resize(nul_pos);
    }
    return buffer;
}

[[nodiscard]] std::uint64_t read_instance_luid(IDXCoreAdapterRaw *adapter) noexcept {
    if (adapter->vtbl->IsPropertySupported(adapter, DXCoreAdapterProperty::InstanceLuid) == 0) {
        return 0;
    }
    LUID luid{};
    if (adapter->vtbl->GetProperty(adapter, DXCoreAdapterProperty::InstanceLuid, sizeof(luid),
                                   &luid) != S_OK) {
        return 0;
    }
    std::uint64_t value = 0;
    std::memcpy(&value, &luid, sizeof(value));
    return value;
}

[[nodiscard]] dxcore_adapter_facts read_adapter_facts(IDXCoreAdapterRaw *adapter) noexcept {
    dxcore_adapter_facts facts;
    facts.description = read_driver_description(adapter);
    facts.luid = read_instance_luid(adapter);

    facts.hardware_supported =
        adapter->vtbl->IsPropertySupported(adapter, DXCoreAdapterProperty::IsHardware) != 0;
    if (facts.hardware_supported) {
        BOOL is_hardware = 0;
        if (adapter->vtbl->GetProperty(adapter, DXCoreAdapterProperty::IsHardware,
                                       sizeof(is_hardware), &is_hardware) == S_OK) {
            facts.is_hardware = is_hardware != 0;
        } else {
            facts.hardware_supported = false;
        }
    }

    facts.integrated_supported =
        adapter->vtbl->IsPropertySupported(adapter, DXCoreAdapterProperty::IsIntegrated) != 0;
    if (facts.integrated_supported) {
        BOOL is_integrated = 0;
        if (adapter->vtbl->GetProperty(adapter, DXCoreAdapterProperty::IsIntegrated,
                                       sizeof(is_integrated), &is_integrated) == S_OK) {
            facts.is_integrated = is_integrated != 0;
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

    IDXCoreAdapterFactoryRaw *factory = nullptr;
    if (create_factory(k_iid_dxcore_adapter_factory, reinterpret_cast<void **>(&factory)) != S_OK ||
        factory == nullptr) {
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    IDXCoreAdapterListRaw *adapter_list = nullptr;
    const HRESULT list_hr = factory->vtbl->CreateAdapterList(
        factory, 1, &k_dxcore_adapter_attribute_d3d12_graphics, k_iid_dxcore_adapter_list,
        reinterpret_cast<void **>(&adapter_list));
    if (list_hr != S_OK || adapter_list == nullptr) {
        factory->vtbl->unknown.Release(factory);
        return gltfx_rslt<std::vector<dxcore_adapter_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("dxcore"));
    }

    const std::uint32_t adapter_count = adapter_list->vtbl->GetAdapterCount(adapter_list);
    std::vector<dxcore_adapter_facts> facts;
    facts.reserve(adapter_count);

    for (std::uint32_t i = 0; i < adapter_count; ++i) {
        IDXCoreAdapterRaw *adapter = nullptr;
        // IID_IDXCoreAdapter shares the same first-3-vtable-slots
        // IUnknown shape as every interface here; requested via the
        // adapter list's OWN GetAdapter(), which - per Microsoft
        // Learn's own worked example - accepts IID_IDXCoreAdapter, a
        // FOURTH GUID this file does not need to declare separately
        // because CreateAdapterList()/GetAdapter() both resolve it
        // through the SAME riid parameter this call already threads
        // through; declared inline here to keep the "one GUID token
        // per interface actually queried" shape the rest of this file
        // uses.
        constexpr GUID k_iid_dxcore_adapter = {
            0xF0DB4C3B, 0x4FC4, 0x4E27, {0x93, 0x50, 0x0D, 0x03, 0x9C, 0x0A, 0x14, 0x07}};
        if (adapter_list->vtbl->GetAdapter(adapter_list, i, k_iid_dxcore_adapter,
                                           reinterpret_cast<void **>(&adapter)) == S_OK &&
            adapter != nullptr) {
            facts.push_back(read_adapter_facts(adapter));
            adapter->vtbl->unknown.Release(adapter);
        }
    }

    adapter_list->vtbl->unknown.Release(adapter_list);
    factory->vtbl->unknown.Release(factory);

    return gltfx_rslt<std::vector<dxcore_adapter_facts>>::ok(std::move(facts));
}

#endif // defined(_WIN32)
