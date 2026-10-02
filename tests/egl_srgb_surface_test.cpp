// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <print>

#include "platform/wayland/egl_srgb_surface.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// egl_srgb_surface_test.cpp - D-SRGB-2 (D-SRGB2-7, slice S3): what a failed eglCreateWindowSurface
// WITH the sRGB colorspace asked means, proved without a driver over the whole space of EGL error
// codes. The expected values are written from the rule of D-SRGB2-7, never read back from the code:
//   - EGL_BAD_MATCH or EGL_BAD_ATTRIBUTE: the SAME surface is tried WITHOUT the colorspace (the
//   probe).
//     The probe worked: "no sRGB" (unsupported). The probe failed too: a platform failure carrying
//     the code of the SECOND error.
//   - any other code (EGL_BAD_ALLOC, EGL_BAD_NATIVE_WINDOW, ...): a platform failure carrying that
//     code, no probe, and the probe's answer is never read.
// EGL 1.5 gives EGL_BAD_MATCH also for a native window whose format does not match the config, so
// the code alone is ambiguous: that is why the probe exists.

using glintfx::platform::classify_egl_srgb_surface_failure;
using glintfx::platform::egl_srgb_failure_needs_probe;
using glintfx::platform::egl_srgb_probe;
using glintfx::platform::egl_srgb_surface_outcome;
using glintfx::platform::egl_srgb_surface_verdict;

namespace {

// Every error code <EGL/egl.h> defines for a call, plus success.
constexpr std::array<EGLint, 15> k_codes = {
    EGL_SUCCESS,       EGL_NOT_INITIALIZED, EGL_BAD_ACCESS,        EGL_BAD_ALLOC,
    EGL_BAD_ATTRIBUTE, EGL_BAD_CONFIG,      EGL_BAD_CONTEXT,       EGL_BAD_CURRENT_SURFACE,
    EGL_BAD_DISPLAY,   EGL_BAD_MATCH,       EGL_BAD_NATIVE_PIXMAP, EGL_BAD_NATIVE_WINDOW,
    EGL_BAD_PARAMETER, EGL_BAD_SURFACE,     EGL_CONTEXT_LOST,
};

bool is_probe_code(EGLint code) { return code == EGL_BAD_MATCH || code == EGL_BAD_ATTRIBUTE; }

} // namespace

GLINTFX_TEST(egl_srgb_surface_bad_alloc_is_never_read_as_no_srgb) {
    // The defect of egl_context_adapter.cpp:603-610 (55c8079): ANY failure became "the driver has
    // no sRGB". A memory failure is not a statement about the driver.
    for (const bool probe_created : {false, true}) {
        const egl_srgb_probe probe{probe_created, EGL_BAD_MATCH};
        const egl_srgb_surface_verdict verdict =
            classify_egl_srgb_surface_failure(EGL_BAD_ALLOC, probe);
        GLINTFX_CHECK(verdict.outcome == egl_srgb_surface_outcome::surface_failure);
        GLINTFX_CHECK_EQ(verdict.os_error_code, EGL_BAD_ALLOC);
    }
    GLINTFX_CHECK(!egl_srgb_failure_needs_probe(EGL_BAD_ALLOC));
}

GLINTFX_TEST(egl_srgb_surface_bad_match_and_bad_attribute_ask_for_the_probe) {
    GLINTFX_CHECK(egl_srgb_failure_needs_probe(EGL_BAD_MATCH));
    GLINTFX_CHECK(egl_srgb_failure_needs_probe(EGL_BAD_ATTRIBUTE));
}

GLINTFX_TEST(egl_srgb_surface_probe_that_works_means_no_srgb) {
    for (const EGLint first : {EGL_BAD_MATCH, EGL_BAD_ATTRIBUTE}) {
        const egl_srgb_surface_verdict verdict =
            classify_egl_srgb_surface_failure(first, egl_srgb_probe{true, EGL_SUCCESS});
        GLINTFX_CHECK(verdict.outcome == egl_srgb_surface_outcome::srgb_unsupported);
    }
}

GLINTFX_TEST(egl_srgb_surface_probe_that_fails_is_a_platform_failure_with_the_second_code) {
    for (const EGLint first : {EGL_BAD_MATCH, EGL_BAD_ATTRIBUTE}) {
        for (const EGLint second : {EGL_BAD_MATCH, EGL_BAD_NATIVE_WINDOW, EGL_BAD_ALLOC}) {
            const egl_srgb_surface_verdict verdict =
                classify_egl_srgb_surface_failure(first, egl_srgb_probe{false, second});
            GLINTFX_CHECK(verdict.outcome == egl_srgb_surface_outcome::surface_failure);
            GLINTFX_CHECK_EQ(verdict.os_error_code, second);
        }
    }
}

GLINTFX_TEST(egl_srgb_surface_whole_space_of_codes_and_probes) {
    // 15 first codes x (1 probe-created + 15 probe codes) = 240 combinations, each checked against
    // the rule of D-SRGB2-7. L-40: fewer than 240 checked fails.
    std::size_t checked = 0;
    for (const EGLint first : k_codes) {
        for (std::size_t i = 0; i <= k_codes.size(); ++i) {
            const bool created = (i == k_codes.size());
            const egl_srgb_probe probe{created, created ? EGL_SUCCESS : k_codes[i]};
            const egl_srgb_surface_verdict verdict =
                classify_egl_srgb_surface_failure(first, probe);
            const bool probed = is_probe_code(first);
            GLINTFX_CHECK_EQ(egl_srgb_failure_needs_probe(first), probed);
            if (probed && created) {
                GLINTFX_CHECK(verdict.outcome == egl_srgb_surface_outcome::srgb_unsupported);
            } else {
                GLINTFX_CHECK(verdict.outcome == egl_srgb_surface_outcome::surface_failure);
                GLINTFX_CHECK_EQ(verdict.os_error_code, probed ? probe.error : first);
            }
            ++checked;
        }
    }
    std::println("egl_srgb_surface: {} combinations checked", checked);
    GLINTFX_CHECK_EQ(checked, std::size_t{240});
}
