// SPDX-License-Identifier: AGPL-3.0-or-later
#include "window_capture_rules.hpp"

namespace glintfx::capture_tool {

namespace {

constexpr std::size_t k_occlusion_point_count = 5;

} // namespace

verdict judge_window_count(std::size_t found) {
    return {.pass = found == 1, .text = "janelas=" + std::to_string(found)};
}

verdict judge_client_on_screen(const pixel_rect &client, const pixel_rect &virtual_screen) {
    const bool empty = client.right <= client.left || client.bottom <= client.top;
    const bool inside = client.left >= virtual_screen.left && client.top >= virtual_screen.top &&
                        client.right <= virtual_screen.right &&
                        client.bottom <= virtual_screen.bottom;
    if (empty || !inside) {
        return {.pass = false, .text = "FORA DA TELA"};
    }
    return {.pass = true, .text = "area cliente dentro da tela"};
}

std::vector<screen_point> occlusion_points(const pixel_rect &client) {
    const int last_x = client.right - 1;
    const int last_y = client.bottom - 1;
    return {{.x = client.left, .y = client.top},
            {.x = last_x, .y = client.top},
            {.x = client.left, .y = last_y},
            {.x = last_x, .y = last_y},
            {.x = client.left + (client.right - client.left) / 2,
             .y = client.top + (client.bottom - client.top) / 2}};
}

verdict judge_occlusion(const std::vector<occlusion_probe> &probes) {
    if (probes.size() != k_occlusion_point_count) {
        return {.pass = false, .text = "pontos=" + std::to_string(probes.size())};
    }
    for (const occlusion_probe &probe : probes) {
        if (!probe.owned_by_target) {
            return {.pass = false, .text = "OCLUIDA por classe=" + probe.owner_class};
        }
    }
    return {.pass = true, .text = "5 pontos da propria janela"};
}

verdict judge_capture_readiness(const window_facts &facts, const pixel_rect &client,
                                const pixel_rect &virtual_screen,
                                const std::vector<occlusion_probe> &probes) {
    if (!facts.visible) {
        return {.pass = false, .text = "INVISIVEL"};
    }
    if (facts.iconic) {
        return {.pass = false, .text = "ICONICA"};
    }
    const verdict on_screen = judge_client_on_screen(client, virtual_screen);
    if (!on_screen.pass) {
        return on_screen;
    }
    return judge_occlusion(probes);
}

std::string capture_meta_text(int width, int height) {
    return "width=" + std::to_string(width) + "\nheight=" + std::to_string(height) +
           "\nstride=" + std::to_string(width * 4) + "\nformat=1\n";
}

} // namespace glintfx::capture_tool
