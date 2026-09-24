// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_rule_engine.hpp"

namespace glintfx::test::wire_relay {

wire_rule_engine::surface_state &wire_rule_engine::state_for(std::uint32_t xdg_surface_id) {
    return m_surfaces[xdg_surface_id];
}

void wire_rule_engine::note_configure_sent(std::uint32_t xdg_surface_id, std::uint32_t serial) {
    state_for(xdg_surface_id).pending_serials.insert(serial);
}

std::optional<rule_violation> wire_rule_engine::note_ack_configure(std::uint32_t xdg_surface_id,
                                                                   std::uint32_t serial) {
    ++m_rules_evaluated;
    surface_state &state = state_for(xdg_surface_id);
    if (state.pending_serials.find(serial) == state.pending_serials.end()) {
        return rule_violation{xdg_surface_id, xdg_surface_error::invalid_serial};
    }
    state.pending_serials.clear();
    state.ever_acknowledged = true;
    return std::nullopt;
}

void wire_rule_engine::note_buffer_attached(std::uint32_t xdg_surface_id) {
    state_for(xdg_surface_id).has_pending_buffer = true;
}

std::optional<rule_violation> wire_rule_engine::note_commit(std::uint32_t xdg_surface_id) {
    ++m_rules_evaluated;
    surface_state &state = state_for(xdg_surface_id);
    bool violates = state.has_pending_buffer && !state.ever_acknowledged;
    state.has_pending_buffer = false;
    if (violates) {
        return rule_violation{xdg_surface_id, xdg_surface_error::unconfigured_buffer};
    }
    return std::nullopt;
}

} // namespace glintfx::test::wire_relay
