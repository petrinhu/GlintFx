// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/log/event.hpp>

#include "core/log/event_data.hpp"

// core/log/event.cpp - CL-3 of CORE-LOG: the exported accessors that
// read gltfx_log_event_data's real layout - see event.hpp's own
// header comment for why these live out-of-line (the layout they read
// is private, not visible to a consumer's own translation unit).

namespace glintfx {

gltfx_log_severity gltfx_log_event::severity() const noexcept { return m_data->severity; }

std::string_view gltfx_log_event::category() const noexcept { return m_data->category; }

std::string_view gltfx_log_event::name() const noexcept { return m_data->name; }

std::span<const gltfx_log_field> gltfx_log_event::fields() const noexcept { return m_data->fields; }

} // namespace glintfx
