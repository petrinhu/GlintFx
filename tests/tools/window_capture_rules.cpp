// SPDX-License-Identifier: AGPL-3.0-or-later
#include "window_capture_rules.hpp"

#include <string>

namespace glintfx::capture_tool {

int verdict_code(std::string_view name) {
    for (const std::string_view entry : k_verdict_code_table) {
        const std::size_t separator = entry.find('=');
        if (!name.empty() && entry.substr(separator + 1) == name) {
            return std::stoi(std::string(entry.substr(0, separator)));
        }
    }
    return -1;
}

namespace {

constexpr std::size_t k_occlusion_point_count = 5;

} // namespace

verdict judge_window_count(std::size_t found) {
    return {.pass = found == 1,
            .text = "janelas=" + std::to_string(found),
            .code = found == 1 ? 0 : verdict_code("JANELAS")};
}

verdict judge_client_on_screen(const pixel_rect &client, const pixel_rect &virtual_screen) {
    const bool empty = client.right <= client.left || client.bottom <= client.top;
    const bool inside = client.left >= virtual_screen.left && client.top >= virtual_screen.top &&
                        client.right <= virtual_screen.right &&
                        client.bottom <= virtual_screen.bottom;
    if (empty || !inside) {
        return {.pass = false, .text = "FORA DA TELA", .code = verdict_code("FORA_DA_TELA")};
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
        return {.pass = false,
                .text = "OCLUIDA pontos=" + std::to_string(probes.size()),
                .code = verdict_code("OCLUIDA")};
    }
    for (const occlusion_probe &probe : probes) {
        if (!probe.owned_by_target) {
            return {.pass = false,
                    .text = "OCLUIDA por classe=" + probe.owner_class,
                    .code = verdict_code("OCLUIDA")};
        }
    }
    return {.pass = true, .text = "5 pontos da propria janela"};
}

verdict judge_capture_readiness(const window_facts &facts, const pixel_rect &client,
                                const pixel_rect &virtual_screen,
                                const std::vector<occlusion_probe> &probes) {
    if (!facts.visible) {
        return {.pass = false, .text = "INVISIVEL", .code = verdict_code("INVISIVEL")};
    }
    if (facts.iconic) {
        return {.pass = false, .text = "ICONICA", .code = verdict_code("ICONICA")};
    }
    verdict on_screen = judge_client_on_screen(client, virtual_screen);
    if (!on_screen.pass) {
        return on_screen;
    }
    return judge_occlusion(probes);
}

capture_plan plan_captures(const verdict &readiness) {
    return {.printwindow = readiness.pass, .bitblt = readiness.pass};
}

verdict judge_capture_refused(std::string_view mechanism, unsigned long error) {
    return {.pass = false,
            .text = "CAPTURA RECUSADA mecanismo=" + std::string(mechanism) +
                    " erro=" + std::to_string(error),
            .code = verdict_code("CAPTURA_RECUSADA")};
}

std::string describe_dwm_flush(int index, std::optional<long> result) {
    const std::string label = "dwmflush" + std::to_string(index) + "=";
    if (!result.has_value()) {
        return label + "indisponivel";
    }
    return label + std::to_string(*result) + (*result < 0 ? " (HRESULT de falha)" : "");
}

std::string describe_fixture_overstay(close_attempt attempt, int budget_ms) {
    const std::string budget = "FIXTURE nao saiu em " + std::to_string(budget_ms) + " ms";
    switch (attempt) {
    case close_attempt::close_posted:
        return budget + " apos WM_CLOSE: TerminateProcess";
    case close_attempt::close_not_delivered:
        return budget + " (a janela foi identificada, mas o PostMessageW recusou o WM_CLOSE): "
                        "TerminateProcess";
    case close_attempt::window_not_identified:
        return budget +
               " (a fixture apresentou, mas nenhuma janela unica dela foi identificada e nenhum "
               "WM_CLOSE foi postado): TerminateProcess";
    case close_attempt::fixture_not_presented:
        break;
    }
    return budget + " (a fixture nao apresentou, a janela nem foi procurada e nenhum WM_CLOSE foi "
                    "postado): TerminateProcess";
}

std::string capture_meta_text(int width, int height) {
    return "width=" + std::to_string(width) + "\nheight=" + std::to_string(height) +
           "\nstride=" + std::to_string(width * 4) + "\nformat=1\n";
}

} // namespace glintfx::capture_tool
