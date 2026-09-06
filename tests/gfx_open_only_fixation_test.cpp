// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <span>
#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_open_only_fixation.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_open_only_fixation_test.cpp - C-OPT (docs/plano-w6b-placa-e-
// laco.md sec. 14.1/14.2, D-W6b-25, GODS_LAWS.md L-20): the TDD red/
// green witness for glintfx::platform::resolve_gfx_open_only_fixation()
// (src/platform/gl/gfx_open_only_fixation.hpp).
//
// RED, SEEN: before gfx_open_only_fixation.{hpp,cpp} existed, this
// file's own #include line failed to compile.
//
// THE 6-CELL ENUMERATION (sec. 14.2's own wording, verbatim): {nada
// fixado, igual, diferente} x {pedida, omitida}. Exercised below over
// the registry's own three `open_only` options (gpu_preference id 2,
// msaa_samples id 3, srgb_framebuffer id 4 - sec. 11.2), all with
// default 0, using msaa_samples as the ONE option each case varies -
// its own [0, 16] range gives room for a value clearly different from
// its own default (0) without touching the shape gfx_option_
// validation_test.cpp already covers.

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_entry;
using glintfx::platform::gfx_open_only_fixation_outcome;
using glintfx::platform::gfx_open_only_fixation_result;
using glintfx::platform::resolve_gfx_open_only_fixation;

namespace {

// The complete open_only set with every default (gpu_preference=0,
// msaa_samples=0, srgb_framebuffer=0) - what cell 2 (nada fixado,
// omitida) below fixes, and what cell 4 (igual, omitida) reuses as
// "what was already fixed".
const std::vector<gltfx_gfx_option_entry> k_all_default_fixed{
    {gltfx_gfx_option::gpu_preference, 0},
    {gltfx_gfx_option::msaa_samples, 0},
    {gltfx_gfx_option::srgb_framebuffer, 0},
};

// The complete open_only set with msaa_samples fixed at 4 (non-
// default) - what cell 1 (nada fixado, pedida) below fixes, and what
// cells 5/6 (diferente) reuse as "what was already fixed".
const std::vector<gltfx_gfx_option_entry> k_msaa_four_fixed{
    {gltfx_gfx_option::gpu_preference, 0},
    {gltfx_gfx_option::msaa_samples, 4},
    {gltfx_gfx_option::srgb_framebuffer, 0},
};

bool fixed_set_matches(const std::vector<gltfx_gfx_option_entry> &actual,
                       const std::vector<gltfx_gfx_option_entry> &expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (const gltfx_gfx_option_entry &want : expected) {
        bool found = false;
        for (const gltfx_gfx_option_entry &got : actual) {
            if (got.id == want.id && got.value == want.value) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

} // namespace

// Cell (nada fixado, pedida): no context has opened this window yet,
// and msaa_samples is explicitly requested at 4 - fix_now with the
// COMPLETE set, requested value where given, default everywhere else.
GLINTFX_TEST(gfx_open_only_fixation_nothing_fixed_requested_fixes_now_with_the_requested_value) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(fixed_set_matches(result.fixed, k_msaa_four_fixed));
}

// Cell (nada fixado, omitida): no context has opened this window yet,
// and the opening list is empty - fix_now with every open_only option
// at its own default.
GLINTFX_TEST(gfx_open_only_fixation_nothing_fixed_omitted_fixes_now_with_every_default) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::fix_now);
    GLINTFX_CHECK(fixed_set_matches(result.fixed, k_all_default_fixed));
}

// Cell (igual, pedida): a first context already fixed msaa_samples=4,
// and the second open() explicitly asks for the SAME value - accept.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_requested_the_same_value_accepts) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::accept);
}

// Cell (igual, omitida): a first context already fixed every option at
// its own default, and the second open() omits them all (resolving to
// the SAME defaults) - accept.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_omitted_resolving_to_the_same_default_accepts) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_all_default_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::accept);
}

// Cell (diferente, pedida): a first context already fixed
// msaa_samples=4, and the second open() explicitly asks for a
// DIFFERENT value - refuse, naming msaa_samples.
GLINTFX_TEST(gfx_open_only_fixation_already_fixed_requested_a_different_value_refuses_by_id) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 8}};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::refuse);
    GLINTFX_CHECK(result.refused_id == gltfx_gfx_option::msaa_samples);
}

// Cell (diferente, omitida): a first context already fixed
// msaa_samples=4 (non-default), and the second open() omits it -
// resolving to the DEFAULT (0), which disagrees with what is fixed -
// refuse, naming msaa_samples.
GLINTFX_TEST(
    gfx_open_only_fixation_already_fixed_omitted_resolving_to_a_different_default_refuses) {
    const std::vector<gltfx_gfx_option_entry> requested{};
    const gfx_open_only_fixation_result result = resolve_gfx_open_only_fixation(
        std::span<const gltfx_gfx_option_entry>(k_msaa_four_fixed), std::span(requested));
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::refuse);
    GLINTFX_CHECK(result.refused_id == gltfx_gfx_option::msaa_samples);

    std::println("gfx_open_only_fixation: 6/6 cells of {{nada fixado, igual, diferente}} x "
                 "{{pedida, omitida}} checked");
}
