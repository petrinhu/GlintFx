// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
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

// GODS_LAWS.md L-27/docs/api-conventions.md R3 witness: forces the
// std::vector::push_back() inside resolve_gfx_open_only_fixation()'s
// own `fix_now` branch to fail with std::bad_alloc, and proves the
// noexcept function degrades to `alloc_failed` instead of calling
// std::terminate() (the exact "a lib NUNCA aborta o processo do
// consumidor" the leader's OOM decision forbids - err_context_test.
// cpp's own with_path()/with_rejected_value() cases are the house
// precedent for this exact shape). Internal linkage (same as err_
// context_test.cpp's own identically-named flag): only this TU's own
// operator new/delete overrides below and the test case at the bottom
// of this file ever touch it - global replacement is required for
// THOSE two, never for the flag itself.
bool g_force_alloc_failure = false;

} // namespace

// Global replacement, same shape as err_context_test.cpp's own -
// gfx_open_only_fixation.cpp carries no GLINTFX_API (L-19, "nada e
// exportado") and is recompiled straight into THIS test binary (tests/
// CMakeLists.txt's own target_sources() for gfx_open_only_fixation_
// test), so there is no DLL boundary to cross and no need for that
// file's own win_dll_alloc_hook.hpp companion.
void *operator new(std::size_t size) {
    if (g_force_alloc_failure) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

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

// INBOX (drenagem de 06/09/2026): "uma funcao que promete nunca falhar
// pode derrubar o processo do consumidor por falta de memoria" - the
// `fix_now` branch (nothing fixed yet) is the one that grows `result.
// fixed` with push_back(); forcing operator new to throw mid-loop used
// to escape this noexcept function and call std::terminate(). Armed
// only around the one call under test, so the harness's own printing
// above/below never sees a forced failure.
GLINTFX_TEST(gfx_open_only_fixation_out_of_memory_degrades_instead_of_terminating) {
    const std::vector<gltfx_gfx_option_entry> requested{{gltfx_gfx_option::msaa_samples, 4}};
    g_force_alloc_failure = true;
    const gfx_open_only_fixation_result result =
        resolve_gfx_open_only_fixation(std::nullopt, std::span(requested));
    g_force_alloc_failure = false;
    GLINTFX_CHECK(result.outcome == gfx_open_only_fixation_outcome::alloc_failed);
    GLINTFX_CHECK(result.fixed.empty());
}
