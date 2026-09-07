// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

// platform/wayland/drm_device_facts.hpp - GL-GPU-KIND (docs/plano-
// w6b-fatias-5.md sec. 4.1, D-W6b-30; docs/plano-w6b-fatias-5b-
// revisao.md sec. 1.1/3, D-W6b-38): PLAIN DATA the kernel's own DRM
// ioctl answers on the render node the EGL device query (egl_device_
// query.hpp) handed back - what read_drm_device_facts() (drm_device_
// facts.cpp, Linux-only) fills, and what drm_gpu_kind.hpp's pure
// classify_drm_gpu() consumes.
//
// WAYLAND-DOMAIN, NOT WAYLAND-ONLY (the same distinction window_
// configure_sequence.hpp's own header comment already draws, tests/
// CMakeLists.txt's own comment on that test repeats): this struct is
// plain data with no OS header reachable from it - it compiles on all
// five target platforms, and drm_gpu_kind.cpp's own test proves that
// by linking only this header, never drm_device_facts.cpp itself
// (which DOES touch <fcntl.h>/<sys/ioctl.h>/<drm/*.h>, Linux-only).
//
// `nouveau_platform`/`nouveau_bus_type` (revisao.md sec. 1.1 L7, sec.
// 1.6 N4): the TWO separate kernel questions D-W6b-38 asks the
// `nouveau` driver, in order, before falling back to the exclusion/
// memory-separation vias - `-1` means "not consulted" (either the
// driver was not `nouveau`, or the ioctl this fact came from failed),
// never a real platform/bus-type value (both real enumerations start
// at 0, so `-1` cannot collide).
struct drm_device_facts {
    bool opened = false;
    std::string driver;
    bool query_ok = false;
    bool has_device_local_memory = false;
    bool amdgpu_fusion = false;
    int nouveau_platform = -1;
    int nouveau_bus_type = -1;
};

// read_drm_device_facts() - Linux-only implementation (drm_device_
// facts.cpp): open(O_RDWR|O_CLOEXEC) on `node_path`, DRM_IOCTL_VERSION
// for the driver name, then the ONE query D-W6b-30/38 name for that
// driver (AMDGPU_INFO_DEV_INFO, DRM_I915_QUERY_MEMORY_REGIONS,
// DRM_XE_DEVICE_QUERY_MEM_REGIONS, or the two NVIF/GETPARAM nouveau
// calls) - closes the fd in every return path (GODS_LAWS.md L-11: no
// leaked fd, no process spawned per item).
[[nodiscard]] drm_device_facts read_drm_device_facts(std::string_view node_path) noexcept;
