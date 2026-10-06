// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <EGL/egl.h>

// platform/wayland/egl_srgb_surface.hpp - D-SRGB-2 (D-SRGB2-7, slice S3): what a failed
// eglCreateWindowSurface WITH the sRGB colorspace asked means. Pure, header-only, no driver.
//
// WHY THE CODE ALONE IS NOT ENOUGH: EGL 1.5 (eglCreatePlatformWindowSurface) answers EGL_BAD_MATCH
// both when the config does not support the colorspace asked AND when the native window's format
// does not match the config, and EGL_BAD_ATTRIBUTE for an invalid attribute or value
// (https://registry.khronos.org/EGL/sdk/docs/man/html/eglCreatePlatformWindowSurface.xhtml).
// EGL_KHR_gl_colorspace does not enumerate its own errors. So those two codes are only a SUSPICION
// of "no sRGB", confirmed by trying the SAME surface WITHOUT the colorspace (the probe): if that
// works, the colorspace was the problem. Any other code (EGL_BAD_ALLOC, EGL_BAD_NATIVE_WINDOW, ...)
// says nothing about sRGB and is a platform failure, never "the driver has no sRGB".
//
// The probe is only safe because the first surface does not exist: Mesa's Wayland platform marks
// the wl_egl_window as taken only after every one of the failure points that answer these two codes
// (src/egl/drivers/dri2/platform_wayland.c, dri2_wl_create_window_surface: the colorspace and
// attribute failures precede `window->driver_private = dri2_surf`), so the window can be used
// again.
namespace glintfx::platform {

// What the failure means for the sRGB option.
enum class egl_srgb_surface_outcome : std::uint8_t {
    srgb_unsupported, // refuse with `unsupported`, naming srgb_framebuffer
    surface_failure,  // refuse with `platform_failure`, carrying the EGL error code
};

// The answer of the probe (the same surface without the colorspace). Only read when the first error
// asked for it.
struct egl_srgb_probe {
    bool created = false;
    EGLint error = EGL_SUCCESS; // eglGetError() after a probe that did not create a surface
};

struct egl_srgb_surface_verdict {
    egl_srgb_surface_outcome outcome = egl_srgb_surface_outcome::surface_failure;
    EGLint os_error_code = EGL_SUCCESS; // for surface_failure: the code to carry
};

// Whether the first error is only a suspicion of "no sRGB", to be confirmed by the probe.
[[nodiscard]] inline bool egl_srgb_failure_needs_probe(EGLint first_error) noexcept {
    return first_error == EGL_BAD_MATCH || first_error == EGL_BAD_ATTRIBUTE;
}

// The verdict for a failed sRGB surface. A probe that created a surface means the colorspace was
// the problem; a probe that failed too carries ITS error (the second one).
[[nodiscard]] inline egl_srgb_surface_verdict
classify_egl_srgb_surface_failure(EGLint first_error, const egl_srgb_probe &probe) noexcept {
    if (!egl_srgb_failure_needs_probe(first_error)) {
        return {egl_srgb_surface_outcome::surface_failure, first_error};
    }
    if (probe.created) {
        return {egl_srgb_surface_outcome::srgb_unsupported, EGL_SUCCESS};
    }
    return {egl_srgb_surface_outcome::surface_failure, probe.error};
}

} // namespace glintfx::platform
