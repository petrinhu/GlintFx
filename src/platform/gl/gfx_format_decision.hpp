// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/platform/gl/gfx_option.hpp>

// platform/gl/gfx_format_decision.hpp - D-SRGB-2 (D-SRGB2-1 and D-SRGB2-3), slice S2: what a
// context adapter decides about the `open_only` format options from what the driver answered.
// Header-only, pure, no OS header: the EGL adapter and the WGL adapter both call it, so the order
// of the refusals and the rule of option_support(srgb_framebuffer) CANNOT differ between the two
// systems (L-04): there is only one copy.
//
// THE ORDER OF THE REFUSALS (D-SRGB2-3), the first rule that matches decides:
//   (1) sRGB asked and the driver does not advertise it: srgb_framebuffer;
//   (2) no format with everything asked: srgb_framebuffer when sRGB was asked and a format without
//   it
//       exists (the sRGB is what is missing); else msaa_samples when MSAA was asked; else
//       no_format;
//   (3) sRGB asked and not confirmed on the format that will be used: srgb_framebuffer;
//   (4) none.
// A combination that cannot happen in the world follows the same literal order, with no special
// case, no assert and no abort: an input the matching rule does not read is ignored.

namespace glintfx::platform {

// Which option (or none) the adapter refuses to open with.
enum class gfx_format_refusal : std::uint8_t {
    none,             // open
    srgb_framebuffer, // refuse, naming the sRGB option
    msaa_samples,     // refuse, naming the MSAA option
    no_format,        // refuse, no pixel format at all (a platform failure, not an option)
};

// What the driver answered. On EGL the sRGB is not in the choose list, so
// `format_found_without_srgb` equals `format_found` and `srgb_confirmed` comes from the surface; on
// WGL it comes from the query of the pixel format that will be bound.
struct gfx_format_facts {
    bool msaa_requested = false;
    bool srgb_requested = false;
    bool srgb_advertised = false;
    bool format_found = false;
    bool format_found_without_srgb = false;
    bool srgb_confirmed = false;
};

// Rule (2): which refusal when no format matches everything that was asked.
[[nodiscard]] inline gfx_format_refusal
refusal_when_no_format_found(const gfx_format_facts &facts) noexcept {
    if (facts.srgb_requested && facts.format_found_without_srgb) {
        return gfx_format_refusal::srgb_framebuffer;
    }
    return facts.msaa_requested ? gfx_format_refusal::msaa_samples : gfx_format_refusal::no_format;
}

// The refusal the adapter must report, or `none` to open.
[[nodiscard]] inline gfx_format_refusal decide_gfx_format(const gfx_format_facts &facts) noexcept {
    if (facts.srgb_requested && !facts.srgb_advertised) {
        return gfx_format_refusal::srgb_framebuffer;
    }
    if (!facts.format_found) {
        return refusal_when_no_format_found(facts);
    }
    if (facts.srgb_requested && !facts.srgb_confirmed) {
        return gfx_format_refusal::srgb_framebuffer;
    }
    return gfx_format_refusal::none;
}

// D-SRGB2-1: option_support(srgb_framebuffer), the same rule on both systems. Asked and opened: it
// is supported only if it was confirmed. Not asked: supported if and only if the driver ADVERTISES
// the extension (EGL_KHR_gl_colorspace, WGL_ARB_framebuffer_sRGB or WGL_EXT_framebuffer_sRGB).
[[nodiscard]] inline gltfx_gfx_option_support srgb_option_support(bool requested, bool advertised,
                                                                  bool confirmed) noexcept {
    const bool honored = requested ? confirmed : advertised;
    return honored ? gltfx_gfx_option_support::supported
                   : gltfx_gfx_option_support::unsupported_here;
}

} // namespace glintfx::platform
