// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string_view>

#include <glintfx/core/err.hpp>

// platform/win32/app_user_model_id.hpp - X-2 (docs/plano-w6a-janela.md
// fatia 9, D-W6a-21): a SEPARATE atom (this project's own §6 table,
// "o .cpp de application_id e um atomo separado... para uma reprovacao
// de lint ali nao derrubar o adaptador inteiro") - <shellapi.h>/
// <propsys.h>/<propkey.h>/<propvarutil.h> are heavy, macro-hostile
// system headers this one small function isolates, so a clang-tidy
// finding here never blocks window_adapter.cpp's own, unrelated
// translation logic.
//
// MECHANISM (D-W6a-21 option (b), chosen over SetCurrentProcessExplicit
// AppUserModelID (a) because that call is PER-PROCESS, while the
// Wayland side's xdg_toplevel_set_app_id is PER-TOPLEVEL - option (b)
// is the only one that keeps the two backends' application_id
// semantics aligned): SHGetPropertyStoreForWindow(window, ...) to get
// the window's own IPropertyStore (learn.microsoft.com/windows/win32/
// api/shellapi/nf-shellapi-shgetpropertystoreforwindow, header
// shellapi.h, library Shell32.lib), then IPropertyStore::SetValue()
// with PKEY_AppUserModel_ID (learn.microsoft.com/windows/win32/
// properties/props-system-appusermodel-id, "PKEY values are defined in
// Propkey.h") - that same page's own Remarks say SetValue() stores the
// property IMMEDIATELY, "no call to IPropertyStore::Commit is needed".
//
// COM (IPropertyStore is a COM interface): CoInitializeEx(COINIT_
// APARTMENTTHREADED) is called and torn down INSIDE set_window_
// application_id() itself, tolerating RPC_E_CHANGED_MODE (the calling
// thread's COM apartment was already initialized in a DIFFERENT mode
// by the consumer's own code - still usable, this function just never
// owns the uninitialize in that case) - CoUninitialize() is paired
// ONLY when THIS call's own CoInitializeEx returned S_OK or S_FALSE
// (it genuinely added a reference), never on RPC_E_CHANGED_MODE or a
// hard failure.
//
// DECLARED LIMITATION (GODS_LAWS.md L-27, no Windows toolchain on this
// machine - same declared shape display_adapter.cpp's own header
// comment already uses): written and reviewed against Microsoft's
// current documentation, never compiled or run here. window_parity_
// test (fatia 11, P-1) is the first real red/green proof, on the
// server. A SECOND declared gap, left open on purpose rather than
// engineered blind: SHGetPropertyStoreForWindow's own Remarks say "a
// window's properties must be removed before the window is closed" (a
// resource leak otherwise, not a crash) - this fatia does not call
// SetValue() with VT_EMPTY from win32_window_adapter::close(), because
// pairing that teardown correctly needs the same live IPropertyStore
// this function never keeps past its own return. Flagged for the
// orchestrator, not fixed here under a plan that says "decida pelo
// caminho mais conservador e registre".
namespace glintfx::platform {

// `application_id` is UTF-8 and already validated (window_desc_
// validation.hpp's own validate_window_text_field(), called by win32_
// window_adapter::open() BEFORE this function ever sees the bytes,
// same "validated once, common to both backends" discipline apply_
// desc() already documents on the Wayland side) - this function only
// widens it (MultiByteToWideChar) before handing it to InitPropVariant
// FromString(). An EMPTY `application_id` is a caller error here (v1
// never requires one - window_adapter.cpp's own open() simply skips
// calling this function at all when the field is empty, the same
// "empty is always accepted, and simply skips the request" shape
// wayland_window_adapter::apply_desc() already uses for xdg_toplevel_
// set_app_id).
[[nodiscard]] gltfx_rslt<void> set_window_application_id(HWND window,
                                                         std::string_view application_id) noexcept;

} // namespace glintfx::platform

#endif // defined(_WIN32)
