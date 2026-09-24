// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

// wire_rule_engine.hpp - the RULE ENGINE atom of the wire relay,
// sub-fatia A2 (docs/plano-w7c.md SS3.A): R1 and R2, the house rules
// a strict compositor (wlroots) enforces that this project's only
// real test compositor (KWin) does not (plan's F4/F5) - see that
// plan's SS2.A research for the sources (xdg-shell.xml's own "error"
// enum on xdg_surface; the SDL3/Zed/Weston/Chromium/VLC citations for
// why "ack only the last of several configures" has to stay legal).
//
// R3 (the house-only rule about relay-INJECTED configures) is A4
// scope, not built here - the plan's own text: "a separacao
// transporte/regras deixa a porta pronta ... sem construir agora".
namespace glintfx::test::wire_relay {

// xdg_surface's own protocol "error" enum values - never renumbered,
// a consumer reading "code 3" off the wire must see what the real
// protocol means by it.
enum class xdg_surface_error : std::uint32_t {
    unconfigured_buffer = 3, // R1
    invalid_serial = 4,      // R2
};

struct rule_violation {
    std::uint32_t offending_object_id = 0;
    xdg_surface_error code = xdg_surface_error::unconfigured_buffer;
};

class wire_rule_engine {
  public:
    // The compositor (real or, in this lab, a fake upstream/injector)
    // sent xdg_surface.configure(serial) for this surface.
    void note_configure_sent(std::uint32_t xdg_surface_id, std::uint32_t serial);

    // R2: violates if serial was never sent for this surface, or was
    // already consumed by an earlier ack_configure. On success,
    // consumes this serial and every earlier one still pending
    // (xdg-shell.xml: ack_configure acknowledges this serial AND
    // every one before it - "confirmar so a ultima de duas passa").
    [[nodiscard]] std::optional<rule_violation> note_ack_configure(std::uint32_t xdg_surface_id,
                                                                   std::uint32_t serial);

    // A non-null buffer got attached to the wl_surface backing this
    // xdg_surface (wl_surface.attach).
    void note_buffer_attached(std::uint32_t xdg_surface_id);

    // R1: violates if a buffer is currently attached and NO configure
    // has ever been acknowledged for this surface yet.
    [[nodiscard]] std::optional<rule_violation> note_commit(std::uint32_t xdg_surface_id);

    [[nodiscard]] std::size_t rules_evaluated() const { return m_rules_evaluated; }

  private:
    struct surface_state {
        std::unordered_set<std::uint32_t> pending_serials;
        bool has_pending_buffer = false;
        bool ever_acknowledged = false;
    };

    [[nodiscard]] surface_state &state_for(std::uint32_t xdg_surface_id);

    std::unordered_map<std::uint32_t, surface_state> m_surfaces;
    std::size_t m_rules_evaluated = 0;
};

} // namespace glintfx::test::wire_relay
