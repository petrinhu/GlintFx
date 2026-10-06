// SPDX-License-Identifier: AGPL-3.0-or-later
#include <optional>
#include <string>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "tools/window_capture_rules.hpp"

// window_capture_rules_test.cpp - QA-SCREEN-CAPTURE C2b-1 (D-W8-36, D-W8-37, D-W8-40): the pure
// rules of the Windows window-capture tool, proven on every system (no system call in them).
// Expected values are written by hand from the decision's own text, never read back from the
// code: window count 0/1/2, client area against the virtual screen (one pixel out rejects), the
// five occlusion points and their verdict, and the exact .meta text of an XRGB8888 capture.

namespace {

using glintfx::capture_tool::occlusion_probe;
using glintfx::capture_tool::pixel_rect;
using glintfx::capture_tool::screen_point;

constexpr pixel_rect k_screen{.left = 0, .top = 0, .right = 1024, .bottom = 768};
constexpr pixel_rect k_client{.left = 60, .top = 83, .right = 380, .bottom = 323};

std::vector<occlusion_probe> all_owned() {
    std::vector<occlusion_probe> probes;
    for (const screen_point &point : glintfx::capture_tool::occlusion_points(k_client)) {
        probes.push_back({.point = point, .owned_by_target = true, .owner_class = "Target"});
    }
    return probes;
}

} // namespace

GLINTFX_TEST(window_count_one_passes_and_prints_the_count) {
    const auto verdict = glintfx::capture_tool::judge_window_count(1);
    GLINTFX_CHECK(verdict.pass);
    GLINTFX_CHECK_EQ(verdict.text, std::string("janelas=1"));
}

GLINTFX_TEST(window_count_zero_rejects_and_prints_the_count) {
    const auto verdict = glintfx::capture_tool::judge_window_count(0);
    GLINTFX_CHECK(!verdict.pass);
    GLINTFX_CHECK_EQ(verdict.text, std::string("janelas=0"));
}

GLINTFX_TEST(window_count_two_rejects_and_prints_the_count) {
    const auto verdict = glintfx::capture_tool::judge_window_count(2);
    GLINTFX_CHECK(!verdict.pass);
    GLINTFX_CHECK_EQ(verdict.text, std::string("janelas=2"));
}

GLINTFX_TEST(client_area_inside_the_screen_passes) {
    GLINTFX_CHECK(glintfx::capture_tool::judge_client_on_screen(k_client, k_screen).pass);
    // Touching the edges exactly is still inside (right/bottom are exclusive).
    GLINTFX_CHECK(glintfx::capture_tool::judge_client_on_screen(k_screen, k_screen).pass);
}

GLINTFX_TEST(client_area_one_pixel_out_on_any_side_rejects_naming_the_screen) {
    const pixel_rect left_out{.left = -1, .top = 10, .right = 100, .bottom = 100};
    const pixel_rect top_out{.left = 10, .top = -1, .right = 100, .bottom = 100};
    const pixel_rect right_out{.left = 10, .top = 10, .right = 1025, .bottom = 100};
    const pixel_rect bottom_out{.left = 10, .top = 10, .right = 100, .bottom = 769};
    for (const pixel_rect &client : {left_out, top_out, right_out, bottom_out}) {
        const auto verdict = glintfx::capture_tool::judge_client_on_screen(client, k_screen);
        GLINTFX_CHECK(!verdict.pass);
        GLINTFX_CHECK_EQ(verdict.text, std::string("FORA DA TELA"));
    }
}

GLINTFX_TEST(client_area_empty_rejects_even_when_it_sits_on_the_screen) {
    const pixel_rect empty{.left = 50, .top = 50, .right = 50, .bottom = 120};
    GLINTFX_CHECK(!glintfx::capture_tool::judge_client_on_screen(empty, k_screen).pass);
}

GLINTFX_TEST(client_area_with_zero_height_rejects_even_when_the_width_is_positive) {
    const pixel_rect flat{.left = 50, .top = 50, .right = 120, .bottom = 50};
    GLINTFX_CHECK(!glintfx::capture_tool::judge_client_on_screen(flat, k_screen).pass);
}

GLINTFX_TEST(client_area_on_a_virtual_screen_with_negative_origin_passes) {
    const pixel_rect wide{.left = -1920, .top = 0, .right = 1920, .bottom = 1080};
    const pixel_rect on_left_monitor{.left = -1900, .top = 100, .right = -1580, .bottom = 340};
    GLINTFX_CHECK(glintfx::capture_tool::judge_client_on_screen(on_left_monitor, wide).pass);
}

GLINTFX_TEST(occlusion_points_are_the_four_last_pixels_of_the_corners_and_the_center) {
    const std::vector<screen_point> points = glintfx::capture_tool::occlusion_points(k_client);
    GLINTFX_CHECK_EQ(points.size(), std::size_t{5});
    GLINTFX_CHECK_EQ(points[0].x, 60);
    GLINTFX_CHECK_EQ(points[0].y, 83);
    GLINTFX_CHECK_EQ(points[1].x, 379);
    GLINTFX_CHECK_EQ(points[1].y, 83);
    GLINTFX_CHECK_EQ(points[2].x, 60);
    GLINTFX_CHECK_EQ(points[2].y, 322);
    GLINTFX_CHECK_EQ(points[3].x, 379);
    GLINTFX_CHECK_EQ(points[3].y, 322);
    GLINTFX_CHECK_EQ(points[4].x, 220);
    GLINTFX_CHECK_EQ(points[4].y, 203);
}

GLINTFX_TEST(occlusion_all_five_points_owned_by_the_target_passes) {
    GLINTFX_CHECK(glintfx::capture_tool::judge_occlusion(all_owned()).pass);
}

// Each of the five points is looked at: a foreign window over ANY ONE of them rejects, naming the
// class found there (a rule that stopped looking at the center, or only looked at one point, would
// pass a test that swaps a single fixed index).
GLINTFX_TEST(occlusion_a_foreign_window_over_any_single_point_rejects_naming_its_class) {
    for (std::size_t foreign = 0; foreign < 5; ++foreign) {
        std::vector<occlusion_probe> probes = all_owned();
        GLINTFX_CHECK_EQ(probes.size(), std::size_t{5});
        probes[foreign].owned_by_target = false;
        probes[foreign].owner_class = "Cover" + std::to_string(foreign);
        const auto verdict = glintfx::capture_tool::judge_occlusion(probes);
        GLINTFX_CHECK(!verdict.pass);
        GLINTFX_CHECK_EQ(verdict.text, "OCLUIDA por classe=Cover" + std::to_string(foreign));
    }
}

GLINTFX_TEST(occlusion_with_two_foreign_points_names_the_first_one) {
    std::vector<occlusion_probe> probes = all_owned();
    GLINTFX_CHECK_EQ(probes.size(), std::size_t{5});
    probes[1].owned_by_target = false;
    probes[1].owner_class = "First";
    probes[4].owned_by_target = false;
    probes[4].owner_class = "Second";
    GLINTFX_CHECK_EQ(glintfx::capture_tool::judge_occlusion(probes).text,
                     std::string("OCLUIDA por classe=First"));
}

GLINTFX_TEST(occlusion_with_a_count_other_than_five_rejects) {
    std::vector<occlusion_probe> probes = all_owned();
    GLINTFX_CHECK_EQ(probes.size(), std::size_t{5});
    probes.pop_back();
    GLINTFX_CHECK(!glintfx::capture_tool::judge_occlusion(probes).pass);
    GLINTFX_CHECK(!glintfx::capture_tool::judge_occlusion({}).pass);
}

GLINTFX_TEST(capture_meta_text_is_the_xrgb8888_text_with_tight_stride) {
    GLINTFX_CHECK_EQ(glintfx::capture_tool::capture_meta_text(320, 240),
                     std::string("width=320\nheight=240\nstride=1280\nformat=1\n"));
}

// -- the order of the verdicts (D-W8-43): every step has a case in which ALL the later causes are
// also present, so a rule that swaps two adjacent steps changes the text of exactly one case.

namespace {

using glintfx::capture_tool::window_facts;

constexpr window_facts k_shown{.visible = true, .iconic = false};
constexpr pixel_rect k_off_screen{.left = 900, .top = 700, .right = 1300, .bottom = 900};

std::vector<occlusion_probe> one_foreign() {
    std::vector<occlusion_probe> probes = all_owned();
    probes[2].owned_by_target = false;
    probes[2].owner_class = "Cover";
    return probes;
}

std::string readiness(const window_facts &facts, const pixel_rect &client,
                      const std::vector<occlusion_probe> &probes) {
    return glintfx::capture_tool::judge_capture_readiness(facts, client, k_screen, probes).text;
}

} // namespace

GLINTFX_TEST(readiness_passes_when_visible_on_screen_and_uncovered) {
    const auto verdict =
        glintfx::capture_tool::judge_capture_readiness(k_shown, k_client, k_screen, all_owned());
    GLINTFX_CHECK(verdict.pass);
}

GLINTFX_TEST(readiness_invisible_wins_over_every_later_cause) {
    const window_facts hidden_and_iconic{.visible = false, .iconic = true};
    GLINTFX_CHECK_EQ(readiness(hidden_and_iconic, k_off_screen, one_foreign()),
                     std::string("INVISIVEL"));
    GLINTFX_CHECK(!glintfx::capture_tool::judge_capture_readiness(
                       {.visible = false, .iconic = false}, k_client, k_screen, all_owned())
                       .pass);
}

GLINTFX_TEST(readiness_iconic_wins_over_off_screen_and_covered_and_is_not_off_screen) {
    const window_facts minimized{.visible = true, .iconic = true};
    const pixel_rect zero_area{.left = -32000, .top = -32000, .right = -32000, .bottom = -32000};
    GLINTFX_CHECK_EQ(readiness(minimized, zero_area, one_foreign()), std::string("ICONICA"));
    GLINTFX_CHECK_EQ(readiness(minimized, k_client, all_owned()), std::string("ICONICA"));
}

GLINTFX_TEST(readiness_off_screen_wins_over_covered) {
    GLINTFX_CHECK_EQ(readiness(k_shown, k_off_screen, one_foreign()), std::string("FORA DA TELA"));
}

GLINTFX_TEST(readiness_covered_is_named_when_nothing_earlier_applies) {
    GLINTFX_CHECK_EQ(readiness(k_shown, k_client, one_foreign()),
                     std::string("OCLUIDA por classe=Cover"));
}

GLINTFX_TEST(dwm_flush_unavailable_is_said_and_never_hidden) {
    GLINTFX_CHECK_EQ(glintfx::capture_tool::describe_dwm_flush(1, std::nullopt),
                     std::string("dwmflush1=indisponivel"));
}

GLINTFX_TEST(dwm_flush_success_prints_the_zero_hresult) {
    GLINTFX_CHECK_EQ(glintfx::capture_tool::describe_dwm_flush(2, 0L), std::string("dwmflush2=0"));
}

GLINTFX_TEST(dwm_flush_failure_hresult_is_named_as_a_failure) {
    GLINTFX_CHECK_EQ(glintfx::capture_tool::describe_dwm_flush(1, -2147467259L),
                     std::string("dwmflush1=-2147467259 (HRESULT de falha)"));
}
