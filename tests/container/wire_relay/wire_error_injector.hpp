// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_rule_engine.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

// wire_error_injector.hpp - the INJECTOR atom of the wire relay
// (docs/plano-w7c.md SS3.A). Encodes the wl_display.error(object_id,
// code, message) event the strict compositor this relay imitates
// would send when R1/R2 trip - wl_display's own wire id (1) and
// opcode (0) are protocol-fixed; object_id inside the payload is
// rule_violation::offending_object_id ("o objeto certo": the
// xdg_surface that broke the rule, never wl_display's own id).
namespace glintfx::test::wire_relay {

inline constexpr std::uint32_t wl_display_object_id = 1;
inline constexpr std::uint16_t wl_display_error_event_opcode = 0;

[[nodiscard]] std::vector<std::uint8_t> encode_display_error(const rule_violation &violation,
                                                             std::string_view message);

} // namespace glintfx::test::wire_relay
