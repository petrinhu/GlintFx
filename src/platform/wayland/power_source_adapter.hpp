// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "platform/port/power_source.hpp"
#include "platform/wayland/power_supply_rule.hpp"

// platform/wayland/power_source_adapter.hpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md sec.
// 5.1, D-W6b-34; /var/tmp/cto-w7d/PLANO-errata.md D-P2-1): the Linux reader of where the machine's
// power comes from. It reads /sys/class/power_supply/<supply>/{type,scope,present,online,status}
// with POSIX opendir/readdir/openat/read (the operating system's own API; NO popen, NO process, NO
// library beyond the standard one - GODS_LAWS.md L-07, L-11) and folds each supply, one at a time,
// into the pure rule (power_supply_rule.hpp).
//
// NEVER ALLOCATES (GODS_LAWS.md L-22, R3 degrau 1): every value read from a file sits in a
// 32-character buffer on the stack (the kernel's own vocabulary here is a dozen characters:
// Documentation/ABI/testing/sysfs-class-power); the directory is visited entry by entry, with no
// list; nothing can throw, so read() has no try/catch. A value longer than the buffer is UNREADABLE
// - it arrives at the rule as an empty view, never as a truncated prefix.
//
// WHAT A MISSING FILE MEANS (D-W6b-34): `present` absent -> 1 (a battery is present unless the
// system says otherwise), `scope` absent -> empty (treated as System), `online` absent -> 0, `type`
// and `status` absent -> empty; a number that is not a number reads as the same default. A
// directory that does not exist or cannot be listed reads as `unknown`.
namespace glintfx::platform {

// Folds every supply under `root` (a directory shaped like /sys/class/power_supply, a C string
// because the calls underneath are C) into `tally`. The root is an ARGUMENT only so a test can
// build a fake tree; the adapter below passes the real literal.
void read_power_supplies(const char *root, power_supply_tally &tally) noexcept;

// The answer for one root.
[[nodiscard]] gltfx_power_source read_power_source_at(const char *root) noexcept;

// The concrete reader selected on Linux (selected_power_source_adapter.hpp). Stateless.
class wayland_power_source_adapter {
  public:
    // Fresh at every call. Any failure to read is `unknown`, never an error and never an
    // exception (there is nothing that can throw).
    [[nodiscard]] gltfx_power_source read() const noexcept;
};

} // namespace glintfx::platform
