// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>

// platform/wayland/egl_device_query.hpp - GL-GPU-KIND (docs/plano-
// w6b-fatias-5.md sec. 4.1, F3; docs/plano-w6b-fatias-5b-revisao.md
// sec. 1.1/1.3): PLAIN DATA about ONE EGLDeviceEXT, as EGL itself
// reports it - what query_egl_display_device()/enumerate_gpus_egl()
// (egl_device_query.cpp/egl_device_enumeration.cpp, Linux-only, both
// reach for <EGL/egl.h>/<EGL/eglext.h>) fill in, and what egl_device_
// dedup.hpp's own pure dedup_egl_devices() consumes.
//
// WAYLAND-DOMAIN, NOT WAYLAND-ONLY: this header itself never includes
// an EGL type - it compiles on all five platforms, the same shape
// drm_device_facts.hpp already established one file over.
//
// `render_node`/`primary_node` come from EGL_DRM_RENDER_NODE_FILE_EXT/
// EGL_DRM_DEVICE_FILE_EXT (revisao.md sec. 1.1 F3's own "o no de
// render e o que abre sem grupo `video`" - the render node is always
// preferred, the primary node is only ever a reserve, D-W6b-30). Both
// empty means this EGLDeviceEXT names neither - the `software` device
// (EGL_MESA_device_software) is the one real case that reaches this
// state.
struct egl_device_facts {
    bool queried = false;
    bool software = false;
    std::string render_node;
    std::string primary_node;
};

// query_egl_display_device() - Linux-only implementation (egl_device_
// query.cpp): resolves eglQueryDisplayAttribEXT/eglQueryDeviceStringEXT
// through eglGetProcAddress() (never a static link, the extension is
// not guaranteed) - `queried=false` when either proc address is
// missing, never a crash.
[[nodiscard]] egl_device_facts query_egl_display_device(void *egl_display) noexcept;

// query_egl_device_facts() - the SECOND caller of the SAME "read
// software/render_node/primary_node off an EGLDeviceEXT" logic
// (GODS_LAWS.md L-33, "two callers, one body"): query_egl_display_
// device() above resolves an EGLDisplay to its EGLDeviceEXT first,
// then calls this; egl_device_enumeration.cpp's own enumerate_egl_
// devices() already HAS the EGLDeviceEXT handles from eglQueryDevicesEXT
// and calls this directly, WITHOUT creating a display per device (F3's
// own "criar display do duplicado da NVIDIA imprime aviso e falha").
// `egl_device` is an opaque EGLDeviceEXT (void* here, the same
// technique this header's own struct avoids including <EGL/egl.h>).
[[nodiscard]] egl_device_facts query_egl_device_facts(void *egl_device) noexcept;
