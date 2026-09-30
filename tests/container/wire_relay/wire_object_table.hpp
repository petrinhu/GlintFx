// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_message.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

// wire_object_table.hpp - the OBJECT TABLE atom of the wire relay
// (docs/plano-w7c.md SS3.A: "tabela de objetos (so as interfaces que
// importam: wl_display, wl_registry ..., wl_callback; delete_id
// aposenta o numero; objeto desconhecido e repassado, contado e
// nunca analisado)").
//
// Watches the small set of requests/events that create or retire an
// object of interest, and classifies every other object id as
// unknown - which this relay's strict-mode never tries to interpret,
// only counts and passes through untouched.
namespace glintfx::test::wire_relay {

enum class known_interface {
    wl_display,
    wl_registry,
    wl_compositor,
    wl_surface,
    xdg_wm_base,
    xdg_surface,
    xdg_toplevel,
    wl_shm,
    wl_shm_pool,
    wl_buffer,
    wl_callback,
    unknown,
};

[[nodiscard]] known_interface interface_from_name(std::string_view name);

// Number of file descriptor arguments the given (interface, opcode)
// request/event carries, per the fixed table this relay knows about.
// Today only wl_shm.create_pool(new_id, fd, int32) does (opcode 0) -
// the vehicle WL-ACK-SMOKE-BLUNT A1's "descritor chegando junto do
// pedaco certo" control exercises.
[[nodiscard]] std::size_t fd_argument_count(known_interface interface, std::uint16_t opcode);

class wire_object_table {
  public:
    // Object id 1 is wl_display by protocol convention - never bound,
    // never deleted - bootstrapped here.
    wire_object_table();

    void declare(std::uint32_t object_id, known_interface interface);
    void retire(std::uint32_t object_id);
    [[nodiscard]] known_interface interface_of(std::uint32_t object_id) const;
    [[nodiscard]] std::size_t unknown_object_count() const { return m_unknown_objects; }

    // xdg_wm_base.get_xdg_surface(new_id, surface) is the only place
    // this table learns which xdg_surface wraps which wl_surface -
    // wire_rule_engine keys its R1/R2 state by xdg_surface id (the
    // object xdg_surface.error names), but wl_surface.attach/commit
    // arrive addressed to the wl_surface id, so the caller needs this
    // to translate one into the other.
    [[nodiscard]] std::optional<std::uint32_t> xdg_surface_for(std::uint32_t surface_id) const;

    // Inspects one already-framed message and updates the table:
    // wl_display.get_registry, wl_registry.bind, wl_compositor.create_
    // surface, xdg_wm_base.get_xdg_surface and wl_shm.create_pool each
    // declare a new object; wl_display.delete_id retires one (xdg_
    // surface.get_toplevel and wl_shm_pool.create_buffer are NOT
    // tracked - nothing WL-ACK-SMOKE-BLUNT A1/A2 needs, R1/R2 only
    // ever look at the xdg_surface/wl_surface pair, GODS_LAWS.md L-33:
    // no creator this fatia's own tests never exercise). Returns the
    // new object id when this message created one. Malformed payloads
    // (too short for the arguments this message's (interface, opcode)
    // demands) never crash - they are ignored, exactly like an
    // unknown interface: never analyzed.
    //
    // from_client distinguishes request from event: Wayland numbers
    // requests and events of the SAME interface independently (e.g.
    // wl_display opcode 1 is the get_registry REQUEST going one way
    // and the delete_id EVENT going the other), so a bare observer
    // sitting on the wire cannot classify a message by (interface,
    // opcode) alone - only by which direction it travelled, which is
    // exactly what this relay's two independent per-direction
    // decoders already know.
    std::optional<std::uint32_t> observe(const decoded_message &message, bool from_client);

  private:
    [[nodiscard]] std::optional<std::uint32_t>
    declare_child(const std::vector<std::uint8_t> &payload, known_interface child);
    [[nodiscard]] std::optional<std::uint32_t>
    observe_registry_bind(const std::vector<std::uint8_t> &payload);
    [[nodiscard]] std::optional<std::uint32_t>
    observe_get_xdg_surface(const std::vector<std::uint8_t> &payload);

    std::unordered_map<std::uint32_t, known_interface> m_objects;
    std::unordered_map<std::uint32_t, std::uint32_t> m_xdg_surface_of_wl_surface;
    std::size_t m_unknown_objects = 0;
};

} // namespace glintfx::test::wire_relay
