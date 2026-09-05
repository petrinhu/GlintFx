// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"

// win32_display_refusal_test.cpp - WIN-WINDOW fatia 7 (X-1',
// docs/plano-w6a-janela.md, D-W6a-16): the refusal path
// win32_display_connect_test.cpp's own header comment already names
// as an open question before this fatia ("there is no known way to
// force RegisterClassExW/CreateWindowExW to fail on the windows-
// latest runner") - answered here by pre-registering, from OUTSIDE
// the adapter, the EXACT class name open() is about to ask for
// (win32_display_adapter::class_name_for(), the public seam
// display_adapter.hpp exposes for exactly this purpose), so the
// runner's own RegisterClassExW deterministically refuses with
// ERROR_CLASS_ALREADY_EXISTS (1410) the moment open() tries the same
// name. This is the same mechanism the W5 plan named for a FIXED class
// name (fechar-w1.md's own precursor); D-W6a-16's fix (a name per
// instance) means the test has to compute the SAME per-instance name
// the adapter under test would use, rather than a single well-known
// constant, or this test would provoke nothing.
//
// win32_display_adapter is compiled a SECOND time directly into this
// executable's own object set (tests/CMakeLists.txt, same technique
// win32_display_connect_test.cpp already documents) - glintfx's own
// shared build hides everything without GLINTFX_API, so a separate
// executable linking only against glintfx::glintfx could never
// resolve class_name_for() (nor any other method here) across that
// boundary.

namespace {

// RAII shape copied from tests/asset_load_test.cpp's own
// exclusive_handle_guard (the same precedent
// tests/win32_runner_probe_test.cpp already documents copying) - this
// harness is CASE-FATAL (harness/check.hpp): a failing GLINTFX_CHECK
// unwinds the current case via an exception, so the class THIS TEST
// pre-registers (never through win32_display_adapter, which never
// reaches its own RegisterClassExW call once the name is already
// taken) must be released by a destructor, never by a line of code a
// thrown exception could skip over.
class registered_class_guard {
  public:
    explicit registered_class_guard(ATOM atom) noexcept : m_atom(atom) {}

    registered_class_guard(const registered_class_guard &) = delete;
    registered_class_guard &operator=(const registered_class_guard &) = delete;

    ~registered_class_guard() {
        if (m_atom != 0) {
            ::UnregisterClassW(MAKEINTATOM(m_atom), ::GetModuleHandleW(nullptr));
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return m_atom != 0; }

  private:
    ATOM m_atom;
};

} // namespace

GLINTFX_TEST(win32_open_refuses_when_its_own_class_name_is_already_registered) {
    // The adapter under test is NEVER opened - only its ADDRESS is
    // used, through the same class_name_for() seam open() itself would
    // call, to learn what name it WOULD ask RegisterClassExW for.
    glintfx::platform::win32_display_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());

    const std::array<wchar_t, glintfx::platform::win32_display_adapter::k_class_name_chars>
        predicted_name = glintfx::platform::win32_display_adapter::class_name_for(&adapter);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &::DefWindowProcW;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = predicted_name.data();

    ::SetLastError(0);
    const registered_class_guard pre_registered(::RegisterClassExW(&wc));
    GLINTFX_CHECK(pre_registered.is_valid());

    // open() now asks RegisterClassExW for the SAME name this test
    // just pre-registered - the runner's own class table (per-process,
    // learn.microsoft.com/windows/win32/winmsg/about-window-classes)
    // refuses the second registration deterministically.
    const glintfx::gltfx_rslt<void> opened = adapter.open();

    GLINTFX_CHECK(opened.has_error());
    GLINTFX_CHECK(!adapter.is_open());
    // ERROR_CLASS_ALREADY_EXISTS = 1410 (winerror.h) - the documented
    // GetLastError() value RegisterClassExW sets for exactly this
    // refusal.
    GLINTFX_CHECK(opened.error().os_error_code() == 1410);

    // A refused open() must not have registered (or left registered)
    // anything of its own - it tore down whatever it had created
    // before returning, per display_adapter.hpp's own open() comment.
    // Nothing further to release here: pre_registered's own destructor
    // owns the ONLY class either side of this test ever successfully
    // registered.
    adapter.close();
    GLINTFX_CHECK(!adapter.is_open());
}

#endif // defined(_WIN32)
