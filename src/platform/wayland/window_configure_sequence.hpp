// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <span>

// platform/wayland/window_configure_sequence.hpp - the pure half of
// the xdg_surface/xdg_toplevel configure sequence (W-C', docs/plano-
// w6a-janela.md fatia 4): translating the numbers the spec sends (a
// width/height pair that can legitimately BE 0x0, and an array of
// small integers naming which xdg_toplevel_state values are set) into
// this project's own shared vocabulary, and remembering EXACTLY ONE
// pending serial to ack (D-W5-7).
//
// Deliberately takes raw std::int32_t state codes rather than
// including the generated xdg-shell-client-protocol.h: this class (and
// its test) touches zero Wayland type - the same "pure, testable in
// isolation" property global_catalog.hpp already has, one directory
// over. wayland_window_adapter (fatia W-E, a later slice) is the TU
// that both includes the generated header AND calls apply_configure() -
// it is the one place the XDG_TOPLEVEL_STATE_* the generated header
// names and the plain integers this file hardcodes below have to
// agree, and it is where a future protocol bump would be caught.
//
// THE 0x0 RULE (xdg_shell.xml, xdg_surface.configure/xdg_toplevel.
// configure): a compositor with no size preference sends 0x0 - NOT
// "resize to zero", but "keep whatever size you already have".
// apply_configure() reports that as has_size == false, never as a
// literal {0, 0} a caller could mistake for a real request.
//
// ACK ONCE PER SERIAL, IMPOSSIBLE TO DOUBLE (D-W5-7): each apply_
// configure() call remembers its own serial; take_serial_to_ack()
// reads AND clears it in the same call, so a second call before the
// next configure returns std::nullopt - there is no way to read the
// same serial back and ack it twice by accident.

namespace glintfx::platform {

// The xdg_toplevel_state enumerators (xdg-shell.xml) window_state.hpp
// tracks. The spec also defines resizing (3), tiled_* (5-8) and
// constrained_* (10-13); those are deliberately absent from window_
// state_bit (D-W5-3: "v1 congela so o minimo") and are silently
// ignored by apply_configure() below rather than treated as an error -
// an unknown or newer state code is not a malformed configure event.
// suspended (9, `since` version 6 - XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_
// VERSION in the generated header) is TRACKED, not ignored, as of
// D-W6b-51 (docs/plano-w6b-fatias-6-8.md): a compositor bound at a
// lower version simply never sends this code, so tracking it
// unconditionally here never needs its own version check - the version
// only matters to a CALLER deciding whether the ABSENCE of the bit is
// meaningful (LOOP-RUN's own present_would_skip() probe, a later
// fatia), never to this pure translation.
inline constexpr std::int32_t k_xdg_toplevel_state_maximized = 1;
inline constexpr std::int32_t k_xdg_toplevel_state_fullscreen = 2;
inline constexpr std::int32_t k_xdg_toplevel_state_activated = 4;
inline constexpr std::int32_t k_xdg_toplevel_state_suspended = 9;

struct window_configure_result {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool has_size = false;
    bool maximized = false;
    bool fullscreen = false;
    bool activated = false;
    bool suspended = false;
};

class window_configure_sequence {
  public:
    window_configure_sequence() noexcept = default;

    [[nodiscard]] window_configure_result apply_configure(std::uint32_t width, std::uint32_t height,
                                                          std::span<const std::int32_t> states,
                                                          std::uint32_t serial) noexcept;

    [[nodiscard]] std::optional<std::uint32_t> take_serial_to_ack() noexcept;

  private:
    std::optional<std::uint32_t> m_pending_serial;
};

} // namespace glintfx::platform
