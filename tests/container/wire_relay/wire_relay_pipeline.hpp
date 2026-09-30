// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_message.hpp"
#include "wire_object_table.hpp"
#include "wire_rule_engine.hpp"

#include <optional>

// wire_relay_pipeline.hpp - the ORCHESTRATION atom that composes the
// object table and the rule engine, exactly as WL-ACK-SMOKE-BLUNT's
// A2 selftest proved case by case (docs/plano-w7c.md SS3.A). Promoted
// out of the test file in A3 (docs/plano-w7c-adendo-revalidacao.md
// SS3.A): wire_relay_main.cpp (the real relay binary, A3) and wire_
// relay_a2_selftest.cpp (the selftest) both call the SAME function
// here, so the two never drift - the exact defect class GODS_LAWS.md
// L-17's "gemeo" doctrine exists to prevent.
//
// This is deliberately NOT the transport/decode loop (that stays
// wire_transport::relay_until_eof() for the ruleless direction, and
// wire_relay_main.cpp's own loop for the ruled one) - only the "what
// does THIS message mean for the object table and the rule engine"
// step, the part A1/A2's own atoms cannot know about each other
// without something gluing them.
namespace glintfx::test::wire_relay {

struct wire_relay_pipeline {
    wire_object_table table;
    wire_rule_engine engine;
};

// Classifies `message` (wire_object_table::observe) and, for the
// small set of wl_surface/xdg_surface requests and events R1/R2 care
// about, feeds the rule engine. Returns a violation when THIS message
// just broke R1 or R2.
[[nodiscard]] std::optional<rule_violation>
observe_and_evaluate(wire_relay_pipeline &pipe, const decoded_message &message, bool from_client);

} // namespace glintfx::test::wire_relay
