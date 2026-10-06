// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_relay_pipeline.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

std::optional<rule_violation>
observe_and_evaluate(wire_relay_pipeline &pipe, const decoded_message &message, bool from_client) {
    pipe.table.observe(message, from_client);
    const known_interface source = pipe.table.interface_of(message.header.object_id);

    if (from_client && source == known_interface::wl_surface && message.header.opcode == 1) {
        std::uint32_t buffer_id = 0;
        if (message.payload.size() >= sizeof(buffer_id)) {
            std::memcpy(&buffer_id, message.payload.data(), sizeof(buffer_id));
        }
        const auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
        if (buffer_id != 0 && xdg_id) {
            pipe.engine.note_buffer_attached(*xdg_id);
        }
        return std::nullopt;
    }
    if (from_client && source == known_interface::wl_surface && message.header.opcode == 6) {
        const auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
        return xdg_id ? pipe.engine.note_commit(*xdg_id) : std::nullopt;
    }
    if (from_client && source == known_interface::xdg_surface && message.header.opcode == 4) {
        std::uint32_t serial = 0;
        if (message.payload.size() >= sizeof(serial)) {
            std::memcpy(&serial, message.payload.data(), sizeof(serial));
        }
        return pipe.engine.note_ack_configure(message.header.object_id, serial);
    }
    if (!from_client && source == known_interface::xdg_surface && message.header.opcode == 0) {
        std::uint32_t serial = 0;
        if (message.payload.size() >= sizeof(serial)) {
            std::memcpy(&serial, message.payload.data(), sizeof(serial));
        }
        pipe.engine.note_configure_sent(message.header.object_id, serial);
    }
    return std::nullopt;
}

} // namespace glintfx::test::wire_relay
