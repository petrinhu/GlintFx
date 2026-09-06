// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/gl_context_adapter_port.hpp"
#include "platform/win32/selected_gl_context_adapter.hpp"

// selected_gl_context_adapter_check.cpp - X-WGL (docs/plano-w6b-placa-
// e-laco.md fatia 4, D-W6b-1, GODS_LAWS.md L-19/L-40), the exact Win32
// sibling of src/platform/wayland/selected_gl_context_adapter_check.cpp
// one directory over: the internal TU that proves the SELECTION, not
// just the adapter type, satisfies platform::gl_context_adapter_port.
// This project has no Windows toolchain to compile this file locally
// (GODS_LAWS.md L-27, the same declared limitation every other win32/
// file in this project already carries) - the windows CI job is what
// actually discharges the compiler on it.
//
// No behavior lives in this file - it contributes nothing to glintfx_
// library's own object code beyond the static_assert below, which the
// compiler discharges entirely at compile time.

namespace glintfx::platform {

static_assert(gl_context_adapter_port<selected_gl_context_adapter>,
              "selected_gl_context_adapter must satisfy gl_context_adapter_port - GODS_LAWS.md "
              "L-19/L-40/X-WGL: the compile-time selection wired in a type that does not satisfy "
              "the port contract");

} // namespace glintfx::platform
