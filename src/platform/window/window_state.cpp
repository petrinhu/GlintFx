// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/window/window_state.hpp"

#include <cstdint>

namespace glintfx::platform {

namespace {

// Round-to-nearest, wide multiply so buffer_scale*dpi never overflows
// before the divide (GODS_LAWS.md L-17: one named helper, not the
// formula spelled out twice for width and height).
[[nodiscard]] std::uint32_t scale_dimension(std::uint32_t value, std::uint32_t numerator,
                                            std::uint32_t denominator) noexcept {
    const std::uint64_t wide =
        (static_cast<std::uint64_t>(value) * numerator + (denominator / 2)) / denominator;
    return static_cast<std::uint32_t>(wide);
}

} // namespace

void window_state::apply_logical_size(std::uint32_t width, std::uint32_t height) noexcept {
    m_logical_size = window_size{width, height};
}

void window_state::apply_buffer_scale(std::uint32_t scale) noexcept { m_buffer_scale = scale; }

void window_state::apply_dpi(std::uint32_t dpi) noexcept { m_dpi = dpi; }

void window_state::set_state(window_state_bit bit, bool on) noexcept {
    switch (bit) {
    case window_state_bit::active:
        m_active = on;
        return;
    case window_state_bit::maximized:
        m_maximized = on;
        return;
    case window_state_bit::fullscreen:
        m_fullscreen = on;
        return;
    }
}

void window_state::request_close() noexcept { m_close_requested = true; }

window_size window_state::logical_size() const noexcept { return m_logical_size; }

window_size window_state::pixel_size() const noexcept {
    const std::uint32_t numerator = m_buffer_scale * m_dpi;
    return window_size{
        scale_dimension(m_logical_size.width, numerator, 96),
        scale_dimension(m_logical_size.height, numerator, 96),
    };
}

bool window_state::state(window_state_bit bit) const noexcept {
    switch (bit) {
    case window_state_bit::active:
        return m_active;
    case window_state_bit::maximized:
        return m_maximized;
    case window_state_bit::fullscreen:
        return m_fullscreen;
    }
    return false;
}

bool window_state::close_requested() const noexcept { return m_close_requested; }

} // namespace glintfx::platform
