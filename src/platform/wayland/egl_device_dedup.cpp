// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_device_dedup.hpp"

namespace glintfx::platform {

std::vector<std::size_t> dedup_egl_devices(std::span<const egl_device_facts> devices) noexcept {
    std::vector<std::size_t> survivors;
    std::vector<std::string> seen_keys;

    for (std::size_t i = 0; i < devices.size(); ++i) {
        const egl_device_facts &device = devices[i];
        const std::string &key =
            !device.render_node.empty() ? device.render_node : device.primary_node;

        if (key.empty()) {
            // No node at all (e.g. a software device) - never collapses
            // with anything else, order preserved.
            survivors.push_back(i);
            continue;
        }

        bool already_seen = false;
        for (const std::string &seen_key : seen_keys) {
            if (seen_key == key) {
                already_seen = true;
                break;
            }
        }

        if (!already_seen) {
            seen_keys.push_back(key);
            survivors.push_back(i);
        }
    }

    return survivors;
}

} // namespace glintfx::platform
