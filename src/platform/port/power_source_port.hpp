// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <concepts>

#include "platform/port/power_source.hpp"

// power_source_port.hpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md
// sec. 5.1, D-W6b-34, GODS_LAWS.md L-19): the compile-time contract a concrete
// power-source reader (Wayland reads /sys/class/power_supply, Win32 calls
// GetSystemPowerStatus) is checked against, in the
// selected_power_source_adapter_check.cpp of each backend. It includes the
// VALUE type (power_source.hpp, created by P1) and adds only the concept: the
// pure automatic rule needs the type and never this port (L-17/L-19).
//
// ONE method, `read()`, `const` and `noexcept`: it answers "where does the
// power come from RIGHT NOW", fresh at every call (the library rereads on every
// read of `power_source`, `suggested_preset` or `auto_choice_reason`, and on
// every `preset = automatic`; never in a loop, never on its own - D-W6b-34).
// Any failure to read is the ordinary answer `unknown`, never an error: an
// unreadable supply says nothing about the machine.
//
// NOT pinned_adapter<A> (adapter_pin.hpp): a reader hands no address to the
// operating system and registers no callback, so the immobility that concept
// exists for is not at stake here.
namespace glintfx::platform {

template <typename A>
concept power_source_adapter_port = requires(const A &adapter) {
    { adapter.read() } noexcept -> std::same_as<gltfx_power_source>;
};

} // namespace glintfx::platform
