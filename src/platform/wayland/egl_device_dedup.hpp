// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "platform/wayland/egl_device_query.hpp"

// platform/wayland/egl_device_dedup.hpp - GL-GPU-KIND (docs/plano-
// w6b-fatias-5.md sec. 4.1, F3, D-W6b-33): the PURE dedup step -
// eglQueryDevicesEXT hands back ONE EGLDeviceEXT per (device, display
// backend) pair, and this machine's own NVIDIA shows up TWICE (once
// under EGL_PLATFORM_GBM/DEVICE, once under the Mesa fallback that
// fails to create a display) - dedup_egl_devices() collapses that
// before anything downstream (classification, exclusion, the public
// enumeration D-W6b-33 freezes) ever sees a "duas NVIDIA" that is
// really one card.
//
// COLLAPSE KEY: the render node when present, the primary node as a
// reserve when it is not, and an ENTRY WITH NEITHER NEVER COLLAPSES
// (order preserved) - two software devices with no node at all are
// two distinct survivors, never merged by accident of both having an
// empty key.
//
// WAYLAND-DOMAIN, NOT WAYLAND-ONLY: takes only egl_device_facts (plain
// data), compiles and is tested on all five platforms.

namespace glintfx::platform {

[[nodiscard]] std::vector<std::size_t>
dedup_egl_devices(std::span<const egl_device_facts> devices) noexcept;

} // namespace glintfx::platform
