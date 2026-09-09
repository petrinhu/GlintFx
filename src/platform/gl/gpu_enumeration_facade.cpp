// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/platform/gl/gpu.hpp>

#include <new>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gpu_enumeration_impl.hpp"

#if defined(_WIN32)
#include "platform/win32/dxcore_adapter_enumeration.hpp"
#include "platform/win32/dxcore_gpu_kind.hpp"
#else
#include "platform/wayland/egl_device_enumeration.hpp"
#endif

// gpu_enumeration_facade.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md
// sec. 4.3, D-W6b-33): gltfx_gpu_enumeration's own PIMPL body, the SAME
// #if-by-platform-directory shape gl_context_facade.cpp already uses
// one file over.
//
// ON WINDOWS, query() classifies EVERY adapter DXCore enumerates
// (classify_dxcore_gpu(list, i), `matched=i` - each entry is asked
// about ITSELF, unlike gltfx_gl_context::gpu()'s own single "which
// adapter is THIS context on" match) and names each from its own
// DriverDescription - no live context needed for either. ON LINUX,
// enumerate_gpus_egl() (egl_device_enumeration.cpp) already does the
// equivalent kernel-only classification and hands back a ready gltfx_
// gpu_info list, `name` empty for every entry (that file's own header
// comment names why - a declared, counted L-04 divergence).

namespace glintfx {

gltfx_rslt<gltfx_gpu_enumeration> gltfx_gpu_enumeration::query() noexcept {
    // WIN-DEBUG-CTORALLOC (07/09/2026, gemeo do achado em gfx_open_
    // only_fixation.cpp): `new (std::nothrow)` only guards the raw
    // ALLOCATION - if `gpu_enumeration_impl`'s own constructor throws
    // (its two std::vector members' default construction, under the
    // same MSVC Debug risk this whole sweep is about), the exception
    // still propagates through a successful nothrow-new, memory freed
    // by the matching delete, straight past this noexcept function.
    gpu_enumeration_impl *impl = nullptr;
    try {
        impl = new (std::nothrow) gpu_enumeration_impl();
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<gltfx_gpu_enumeration>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    if (impl == nullptr) {
        return gltfx_rslt<gltfx_gpu_enumeration>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

#if defined(_WIN32)
    const gltfx_rslt<std::vector<dxcore_adapter_facts>> adapters = enumerate_dxcore_adapters();
    if (adapters.has_error()) {
        const gltfx_err error = adapters.err();
        delete impl;
        return gltfx_rslt<gltfx_gpu_enumeration>::err(error);
    }

    const std::vector<dxcore_adapter_facts> &list = adapters.value();
    // Reserved to its FINAL size before a single gltfx_gpu_info is
    // built (gpu_enumeration_impl.hpp's own header comment) - no
    // push_back below ever reallocates impl->names.
    //
    // GODS_LAWS.md L-22/L-17 (varredura de 07/09/2026, mesma familia
    // de bugs de gfx_open_only_fixation.cpp): assign()/reserve()/
    // push_back() below, and the std::string COPY-assignment `impl->
    // names[i] = list[i].description`, can all throw std::bad_alloc
    // despite this function's own noexcept. query() already returns
    // gltfx_rslt<T> (the same shape the `adapters.has_error()` early
    // return above already uses), so the honest desfecho is the
    // error, not a half-built enumeration. NAO PROVADO NESTA MAQUINA
    // (nomeado, nao escondido): este ramo so' compila com _WIN32
    // definido - sem um build Windows real disponivel nesta sessao
    // (outro agente ativo em src/platform/win32/ no momento desta
    // varredura, GODS_LAWS.md L-11), o job "Windows - Debug"/"Windows
    // - Lint" reais e' quem prova isto, o mesmo idioma que o achado
    // WIN-NOEXCEPT-ESCAPE (dxcore_adapter_enumeration.cpp/wgl_context_
    // adapter.cpp, este mesmo dia) ja documenta para a classe inteira.
    try {
        impl->names.assign(list.size(), std::string{});
        impl->entries.reserve(list.size());
        for (std::size_t i = 0; i < list.size(); ++i) {
            impl->names[i] = list[i].description;
            const gltfx_gpu_kind kind = platform::classify_dxcore_gpu(list, i);
            impl->entries.push_back(
                gltfx_gpu_info{.kind = kind,
                               .name = impl->names[i],
                               .enumeration_index = static_cast<std::uint32_t>(i)});
        }
    } catch (const std::bad_alloc &) {
        delete impl;
        return gltfx_rslt<gltfx_gpu_enumeration>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
#else
    // enumerate_gpus_egl() fills impl->names IN PLACE (by reference)
    // and its own returned entries' string_views already point into
    // it - never copied through a temporary that would dangle them.
    gltfx_rslt<std::vector<gltfx_gpu_info>> enumerated = platform::enumerate_gpus_egl(impl->names);
    if (enumerated.has_error()) {
        const gltfx_err &error = enumerated.err();
        delete impl;
        return gltfx_rslt<gltfx_gpu_enumeration>::err(error);
    }
    impl->entries = std::move(enumerated.value());
#endif

    return gltfx_rslt<gltfx_gpu_enumeration>::ok(gltfx_gpu_enumeration(impl));
}

gltfx_gpu_enumeration::gltfx_gpu_enumeration(gltfx_gpu_enumeration &&other) noexcept
    : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_gpu_enumeration &gltfx_gpu_enumeration::operator=(gltfx_gpu_enumeration &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

gltfx_gpu_enumeration::~gltfx_gpu_enumeration() { delete m_impl; }

std::size_t gltfx_gpu_enumeration::count() const noexcept {
    return m_impl == nullptr ? 0 : m_impl->entries.size();
}

gltfx_gpu_info gltfx_gpu_enumeration::at(std::size_t index) const noexcept {
    if (m_impl == nullptr || index >= m_impl->entries.size()) {
        return gltfx_gpu_info{};
    }
    return m_impl->entries[index];
}

} // namespace glintfx
