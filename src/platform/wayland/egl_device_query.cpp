// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_device_query.hpp"

#include <cstring>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// egl_device_query.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec.
// 4.1; docs/plano-w6b-fatias-5b-revisao.md sec. 1.1/1.3, F3): the real
// EGL_EXT_device_query/EGL_EXT_device_base calls - EVERY proc address
// resolved through eglGetProcAddress(), the SAME technique egl_
// context_adapter.cpp's own header comment already documents (never a
// static link: the extension is not guaranteed present).
//
// EGL_MESA_device_software HAS NO NUMERIC TOKEN (it names an EXTENSION
// STRING, not an attribute or enum) - std::strstr() over the space-
// separated list eglQueryDeviceStringEXT(device, EGL_EXTENSIONS)
// returns is the documented way to test for it (the same shape any
// glGetString(GL_EXTENSIONS) scan already uses elsewhere in this
// project).

namespace {

using egl_query_display_attrib_fn = EGLBoolean (*)(EGLDisplay, EGLint, EGLAttrib *);
using egl_query_device_string_fn = const char *(*)(EGLDeviceEXT, EGLint);

} // namespace

egl_device_facts query_egl_device_facts(void *egl_device) noexcept {
    egl_device_facts facts;

    auto query_device_string =
        reinterpret_cast<egl_query_device_string_fn>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    if (query_device_string == nullptr) {
        return facts; // queried=false
    }

    auto device = static_cast<EGLDeviceEXT>(egl_device);
    facts.queried = true;

    if (const char *extensions = query_device_string(device, EGL_EXTENSIONS);
        extensions != nullptr) {
        facts.software = std::strstr(extensions, "EGL_MESA_device_software") != nullptr;
    }
    if (const char *render_node = query_device_string(device, EGL_DRM_RENDER_NODE_FILE_EXT);
        render_node != nullptr) {
        facts.render_node = render_node;
    }
    if (const char *primary_node = query_device_string(device, EGL_DRM_DEVICE_FILE_EXT);
        primary_node != nullptr) {
        facts.primary_node = primary_node;
    }

    return facts;
}

egl_device_facts query_egl_display_device(void *egl_display) noexcept {
    auto query_display_attrib = reinterpret_cast<egl_query_display_attrib_fn>(
        eglGetProcAddress("eglQueryDisplayAttribEXT"));
    if (query_display_attrib == nullptr) {
        return egl_device_facts{}; // queried=false
    }

    EGLAttrib device_attrib = 0;
    if (query_display_attrib(static_cast<EGLDisplay>(egl_display), EGL_DEVICE_EXT,
                             &device_attrib) != EGL_TRUE) {
        return egl_device_facts{}; // queried=false
    }

    // EGL_EXT_device_query's own eglQueryDisplayAttribEXT(dpy, EGL_DEVICE_EXT, &value)
    // hands back the EGLDeviceEXT handle (an opaque pointer) PACKED INSIDE an EGLAttrib
    // (an intptr_t-sized integer - EGLAttrib exists precisely so a pointer-valued
    // attribute survives 64-bit round-trip, unlike the older EGLint). There is no
    // original pointer here to do provenance-preserving arithmetic on (the refactor this
    // checker's own docs suggest,
    // https://clang.llvm.org/extra/clang-tidy/checks/performance/no-int-to-ptr.html) - the
    // extension gives back a bare integer, and this cast is the ONLY way to recover the
    // handle it names. Same structural case, same house convention, one directory over:
    // tests/container/egl_device_reader_probe.cpp suppresses the analogous checker for
    // the dlsym-style function-to-object-pointer cast every GL loader relies on - a
    // different checker, the same "an external C API's contract forces this cast"
    // reasoning (approved by the líder, achado da revisão adversarial de 06-07/09/2026).
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: EGLAttrib packs an opaque handle, see above
    return query_egl_device_facts(reinterpret_cast<void *>(device_attrib));
}
