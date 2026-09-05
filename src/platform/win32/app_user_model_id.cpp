// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/app_user_model_id.hpp"

#if defined(_WIN32)

#include <cstddef>
#include <string>

#include <objbase.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// app_user_model_id.cpp - see app_user_model_id.hpp's own header
// comment for scope, mechanism (SHGetPropertyStoreForWindow/PKEY_
// AppUserModel_ID/InitPropVariantFromString, each cited there with its
// own learn.microsoft.com page), and the two declared limitations
// (never run on a Windows machine; property removal on close() left
// open). The four libraries this atom needs - user32 (already linked
// by the rest of win32/), shell32 (SHGetPropertyStoreForWindow),
// ole32 (CoInitializeEx/CoUninitialize/PropVariantClear) and propsys
// (InitPropVariantFromString, and PKEY_AppUserModel_ID's own storage -
// propkey.h declares it `extern` without INITGUID, so the symbol's
// definition comes from Propsys.lib, not from this translation unit) -
// are exactly the four docs/plano-w6a-janela.md fatia 9's own row
// names ("Linka user32, shell32, ole32, propsys").

namespace glintfx::platform {

namespace {

// window_desc_validation.hpp's own validate_window_text_field() has
// already proved `value` is well-formed UTF-8 before win32_window_
// adapter::open() ever calls this function - MB_ERR_INVALID_CHARS
// stays on anyway (refuse rather than silently substitute a decode
// failure, the same Win32-side half of D-W6a-23's own divergence this
// project's window_desc_validation.hpp exists to close on the OTHER
// side).
[[nodiscard]] bool widen_utf8(std::string_view value, std::wstring &out) noexcept {
    if (value.empty()) {
        out.clear();
        return true;
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                             static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(needed));
    const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), out.data(), needed);
    return written == needed;
}

} // namespace

gltfx_rslt<void> set_window_application_id(HWND window, std::string_view application_id) noexcept {
    std::wstring wide_id;
    if (!widen_utf8(application_id, wide_id)) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("application_id"));
    }

    // CoInitializeEx tolerating RPC_E_CHANGED_MODE (app_user_model_id.
    // hpp's own "COM" paragraph): S_OK/S_FALSE means THIS call added a
    // reference (and therefore owns the matching CoUninitialize()
    // below); RPC_E_CHANGED_MODE means some other code on this thread
    // already initialized COM in a different concurrency model - still
    // usable, just not ours to tear down.
    const HRESULT com_hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool com_owned = (com_hr == S_OK || com_hr == S_FALSE);
    const bool com_usable = SUCCEEDED(com_hr) || com_hr == RPC_E_CHANGED_MODE;
    if (!com_usable) {
        // os_error_code() carries a raw platform diagnostic number
        // (core/err.hpp's own convention); an HRESULT fits the same
        // int64_t slot GetLastError() uses elsewhere in this project,
        // never interpreted by glintfx itself either way.
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(com_hr));
    }

    IPropertyStore *store = nullptr;
    const HRESULT store_hr = ::SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(&store));
    if (FAILED(store_hr) || store == nullptr) {
        if (com_owned) {
            ::CoUninitialize();
        }
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(store_hr));
    }

    PROPVARIANT value{};
    const HRESULT init_hr = ::InitPropVariantFromString(wide_id.c_str(), &value);
    if (FAILED(init_hr)) {
        store->Release();
        if (com_owned) {
            ::CoUninitialize();
        }
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(init_hr));
    }

    // SHGetPropertyStoreForWindow's own documented Remarks (app_user_
    // model_id.hpp's own "MECHANISM" paragraph): SetValue() stores the
    // property immediately - no Commit() call needed or made here.
    const HRESULT set_hr = store->SetValue(PKEY_AppUserModel_ID, value);
    ::PropVariantClear(&value);
    store->Release();
    if (com_owned) {
        ::CoUninitialize();
    }

    if (FAILED(set_hr)) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_os_error_code(set_hr));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform

#endif // defined(_WIN32)
