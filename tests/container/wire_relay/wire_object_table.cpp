// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_object_table.hpp"

#include <cstring>
#include <string>

namespace glintfx::test::wire_relay {

namespace {

// Wayland wire integer/string readers - bounds-checked. A payload
// this project's own decoder already length-framed can still be
// shorter than a well-formed argument list demands if the sender is
// lying about the opcode's argument shape; every reader here returns
// false instead of reading past the end (never a crash on garbage).
bool read_u32(const std::vector<std::uint8_t> &payload, std::size_t offset, std::uint32_t &out) {
    if (offset + sizeof(out) > payload.size()) {
        return false;
    }
    std::memcpy(&out, payload.data() + offset, sizeof(out));
    return true;
}

bool read_wire_string(const std::vector<std::uint8_t> &payload, std::size_t offset,
                      std::string &out, std::size_t &next_offset) {
    std::uint32_t stored_len = 0; // includes the trailing NUL
    if (!read_u32(payload, offset, stored_len) || stored_len == 0) {
        return false;
    }
    std::size_t text_len = stored_len - 1;
    std::size_t start = offset + 4;
    if (start + stored_len > payload.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char *>(payload.data() + start), text_len);
    std::size_t padded = (static_cast<std::size_t>(stored_len) + 3) & ~std::size_t(3);
    next_offset = start + padded;
    return true;
}

} // namespace

known_interface interface_from_name(std::string_view name) {
    if (name == "wl_display")
        return known_interface::wl_display;
    if (name == "wl_registry")
        return known_interface::wl_registry;
    if (name == "wl_compositor")
        return known_interface::wl_compositor;
    if (name == "wl_surface")
        return known_interface::wl_surface;
    if (name == "xdg_wm_base")
        return known_interface::xdg_wm_base;
    if (name == "xdg_surface")
        return known_interface::xdg_surface;
    if (name == "xdg_toplevel")
        return known_interface::xdg_toplevel;
    if (name == "wl_shm")
        return known_interface::wl_shm;
    if (name == "wl_shm_pool")
        return known_interface::wl_shm_pool;
    if (name == "wl_buffer")
        return known_interface::wl_buffer;
    if (name == "wl_callback")
        return known_interface::wl_callback;
    return known_interface::unknown;
}

std::size_t fd_argument_count(known_interface interface, std::uint16_t opcode) {
    if (interface == known_interface::wl_shm && opcode == 0) {
        return 1; // wl_shm.create_pool(new_id, fd, int32 size)
    }
    return 0;
}

wire_object_table::wire_object_table() { m_objects[1] = known_interface::wl_display; }

void wire_object_table::declare(std::uint32_t object_id, known_interface interface) {
    if (interface == known_interface::unknown) {
        ++m_unknown_objects;
    }
    m_objects[object_id] = interface;
}

void wire_object_table::retire(std::uint32_t object_id) { m_objects.erase(object_id); }

known_interface wire_object_table::interface_of(std::uint32_t object_id) const {
    auto it = m_objects.find(object_id);
    return it == m_objects.end() ? known_interface::unknown : it->second;
}

std::optional<std::uint32_t> wire_object_table::xdg_surface_for(std::uint32_t surface_id) const {
    auto it = m_xdg_surface_of_wl_surface.find(surface_id);
    if (it == m_xdg_surface_of_wl_surface.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::uint32_t>
wire_object_table::declare_child(const std::vector<std::uint8_t> &payload, known_interface child) {
    std::uint32_t new_id = 0;
    if (!read_u32(payload, 0, new_id)) {
        return std::nullopt;
    }
    declare(new_id, child);
    return new_id;
}

std::optional<std::uint32_t>
wire_object_table::observe_registry_bind(const std::vector<std::uint8_t> &payload) {
    // wl_registry.bind(uint name, new_id<interface, version, id>): the
    // one request in this table whose new_id names its own interface
    // on the wire - every other creator below produces a FIXED
    // interface by protocol design.
    std::string interface_name;
    std::size_t after_name = 0;
    if (!read_wire_string(payload, 4, interface_name, after_name)) {
        return std::nullopt;
    }
    std::uint32_t new_id = 0;
    if (!read_u32(payload, after_name + 4, new_id)) { // after_name: version (u32), then id
        return std::nullopt;
    }
    declare(new_id, interface_from_name(interface_name));
    return new_id;
}

std::optional<std::uint32_t>
wire_object_table::observe_get_xdg_surface(const std::vector<std::uint8_t> &payload) {
    std::uint32_t new_id = 0;
    std::uint32_t surface_id = 0;
    if (!read_u32(payload, 0, new_id) || !read_u32(payload, 4, surface_id)) {
        return std::nullopt;
    }
    declare(new_id, known_interface::xdg_surface);
    m_xdg_surface_of_wl_surface[surface_id] = new_id;
    return new_id;
}

std::optional<std::uint32_t> wire_object_table::observe(const decoded_message &message,
                                                        bool from_client) {
    known_interface source = interface_of(message.header.object_id);

    // wl_display opcode 1: get_registry (client REQUEST, creates wl_
    // registry) going one way, delete_id (server EVENT, retires an
    // id) going the other - same interface, same opcode, different
    // direction (see this method's own header comment).
    if (source == known_interface::wl_display && message.header.opcode == 1) {
        if (from_client) {
            return declare_child(message.payload, known_interface::wl_registry);
        }
        std::uint32_t retired_id = 0;
        if (read_u32(message.payload, 0, retired_id)) {
            retire(retired_id);
        }
        return std::nullopt;
    }
    if (!from_client) {
        return std::nullopt; // every other creator below is a request
    }
    if (source == known_interface::wl_registry && message.header.opcode == 0) {
        return observe_registry_bind(message.payload);
    }
    if (source == known_interface::wl_compositor && message.header.opcode == 0) {
        return declare_child(message.payload, known_interface::wl_surface);
    }
    if (source == known_interface::xdg_wm_base && message.header.opcode == 2) {
        return observe_get_xdg_surface(message.payload);
    }
    if (source == known_interface::wl_shm && message.header.opcode == 0) {
        return declare_child(message.payload, known_interface::wl_shm_pool);
    }
    return std::nullopt;
}

} // namespace glintfx::test::wire_relay
