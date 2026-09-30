// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/port/power_source_port.hpp"
#include "platform/wayland/selected_power_source_adapter.hpp"

// selected_power_source_adapter_check.cpp - GFX-PRESET, fatia P2 (GODS_LAWS.md
// L-19/L-40), the exact sibling of selected_gl_context_adapter_check.cpp one
// file over: the internal TU that proves the SELECTION, not just the adapter
// type, satisfies platform::power_source_adapter_port. If a future edit drifts
// the adapter from that concept (a signature typo, a method renamed on one side
// only), THIS FILE fails to compile on every build that reaches
// src/platform/wayland/. No behavior lives here: the static_assert below is
// discharged entirely at compile time.

namespace glintfx::platform {

static_assert(power_source_adapter_port<selected_power_source_adapter>,
              "selected_power_source_adapter must satisfy "
              "power_source_adapter_port - GODS_LAWS.md "
              "L-19/L-40: the compile-time selection wired in a type that does "
              "not satisfy the "
              "port contract");

} // namespace glintfx::platform
