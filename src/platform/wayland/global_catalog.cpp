// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/global_catalog.hpp"

#include <algorithm>

// global_catalog.cpp - see global_catalog.hpp's own header comment for
// scope. std::vector<wayland_global> plus linear search, not a map
// keyed by name: this catalog's whole lifetime is "populated by one
// registry roundtrip, read a handful of times, occasionally patched by
// a global_remove event" (fatia B/D's own scope) - a realistic
// compositor's registry burst is a few dozen globals, and a
// std::unordered_map<uint32_t, wayland_global> would spend more on
// hashing/bucket overhead than the linear scans below ever cost at
// this size. Revisit only if a later fatia's own measurement says
// otherwise (GODS_LAWS.md L-33's "regra de 3": no abstraction before
// three real occurrences of a real cost).

namespace glintfx::platform {

void global_catalog::insert(std::uint32_t name, std::string interface, std::uint32_t version) {
    const auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                  [name](const wayland_global &g) { return g.name == name; });
    if (it != m_globals.end()) {
        it->interface = std::move(interface);
        it->version = version;
        return;
    }
    m_globals.push_back(wayland_global{name, std::move(interface), version});
}

void global_catalog::remove(std::uint32_t name) noexcept {
    const auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                  [name](const wayland_global &g) { return g.name == name; });
    if (it != m_globals.end()) {
        m_globals.erase(it);
    }
}

const wayland_global *global_catalog::find_by_interface(std::string_view interface) const noexcept {
    const auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                  [interface](const wayland_global &g) { return g.interface == interface; });
    return it != m_globals.end() ? &(*it) : nullptr;
}

const wayland_global *global_catalog::find_by_name(std::uint32_t name) const noexcept {
    const auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                  [name](const wayland_global &g) { return g.name == name; });
    return it != m_globals.end() ? &(*it) : nullptr;
}

std::uint32_t global_catalog::clamp_version(std::uint32_t supported, std::uint32_t announced) noexcept {
    return supported < announced ? supported : announced;
}

} // namespace glintfx::platform
