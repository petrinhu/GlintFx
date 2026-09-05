// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/display_backend_port.hpp"
#include "platform/port/display_connection_port.hpp"
#include "platform/win32/selected_display_adapter.hpp"

// selected_display_adapter_check.cpp - WIN-DISPLAY, the exact win32/
// sibling of src/platform/wayland/selected_display_adapter_check.cpp
// (CTO plan sec. 1.1.3/2.2): the internal TU that proves the
// SELECTION, not just the adapter type, is correct. If a future edit
// to selected_display_adapter.hpp ever aliases a type that no longer
// satisfies display_connection_port (a typo, a signature that drifted
// while the concept did not, or vice versa), THIS FILE fails to
// compile on every build that reaches src/platform/win32/ -
// GODS_LAWS.md L-19/L-40: a build that silently links an empty or
// mismatched platform layer is exactly the failure mode this
// project's laws forbid, and a compile error here is how "the build
// breaks instead of compiling vazio" is enforced for the SELECTION
// mechanism specifically, as distinct from the adapter type itself.
//
// This static_assert is also this fatia's own "vermelho antes do
// verde" (GODS_LAWS.md L-20): written and wired into
// src/platform/win32/CMakeLists.txt BEFORE win32_display_adapter's own
// three members existed, this file failed to compile - the concept
// requires open()/close()/is_open() with exact signatures, and an
// incomplete or absent type satisfies none of it. Only once display_
// adapter.hpp/.cpp (this fatia's own production code) supplied all
// three does this static_assert - and the configure/build step that
// depends on it - turn green. That red-then-green sequence could only
// be PROVEN by reading, not by running (no Windows toolchain on this
// machine, see display_adapter.hpp's own header comment); the windows
// CI job is what actually discharges the compiler on it.
//
// SECOND static_assert added by X-1' (docs/plano-w6a-janela.md fatia
// 7, this fatia's own "TU de static_assert" line): display_backend_
// port.hpp (src/platform/port/, W-D'/fatia 6, sibling fatia landing
// CONCURRENTLY with this one) requires everything display_connection_
// port already requires PLUS a noexcept pump_events() returning
// gltfx_rslt<void> - that header's own comment names THIS adapter and
// THIS fatia by number as the one thing standing between it and being
// satisfied on Windows ("win32_display_adapter... is EXPECTED to
// [satisfy display_backend_port] on Windows only once WIN-DISPLAY's
// own pump_events() lands"). This adapter's own pump_events()
// (display_adapter.hpp/.cpp, this same fatia) is what turns that
// expectation into a proven fact rather than a promise the two fatias
// would otherwise have no compile-time link between. No separate
// "backend" composition type exists yet on EITHER platform (Wayland's
// own selected_display_adapter.hpp still aliases wayland_display_
// adapter directly, display_backend_port.hpp's own header comment
// says a composed type is free to replace it LATER without touching
// this file) - so this project deliberately does not invent one here
// either; the adapter itself is the backend until a real composition
// (shell, seat) exists to justify one.
//
// No behavior lives in this file - it contributes nothing to
// glintfx_library's own object code beyond the two static_asserts
// below, which the compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(display_connection_port<selected_display_adapter>,
              "selected_display_adapter must satisfy display_connection_port - "
              "GODS_LAWS.md L-19/WIN-DISPLAY: the compile-time selection wired in a "
              "type that does not satisfy the port contract");

static_assert(display_backend_port<selected_display_adapter>,
              "selected_display_adapter must satisfy display_backend_port - "
              "GODS_LAWS.md L-19/WIN-WINDOW fatia 7 (X-1'): pump_events() must exist "
              "and return gltfx_rslt<void> noexcept, the exact capability display_backend_"
              "port.hpp's own header comment names this fatia as owing it");

} // namespace glintfx::platform
