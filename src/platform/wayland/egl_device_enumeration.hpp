// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/wayland/egl_device_query.hpp"

// platform/wayland/egl_device_enumeration.hpp - GL-GPU-KIND (docs/
// plano-w6b-fatias-5.md sec. 4.1, F3, D-W6b-33): eglQueryDevicesEXT
// over the WHOLE machine, deduplicated (egl_device_dedup.hpp) and, for
// enumerate_gpus_egl() below, classified purely by the kernel (drm_
// gpu_kind.hpp) - NO exclusion/memory-separation via applied here (see
// gpu.hpp's own gltfx_gpu_enumeration class comment: this answers "what
// does the system say about EVERY GPU", a DIFFERENT question from
// gltfx_gl_context::gpu()'s own "what does THIS context's adapter
// conclude about the one it is running on").
//
// `name` IS EMPTY FOR EVERY ENTRY ON LINUX, DECLARED, NOT A DEFECT: EGL
// has no per-device "friendly name" query outside of a live rendering
// context (GL_RENDERER is per-CONTEXT, and this enumeration never opens
// a context on any GPU besides the one already current) - unlike
// Windows, where DXCore's own DriverDescription names every adapter
// WITHOUT needing one (D-W6b-37's own gpu_enumeration_facade.cpp
// wiring). THIS IS A DIVERGENCE (GODS_LAWS.md L-04: declared AND
// counted) between the two platforms' own gltfx_gpu_enumeration::at()
// answers, on top of the six the plan's own §8 already tracks -
// flagged here for the plan's own ledger to pick up, not silently
// absorbed.

namespace glintfx::platform {

// Internal: the raw, post-dedup device facts - shared by enumerate_
// gpus_egl() below AND by egl_context_adapter.cpp's own current-
// context classification (which needs to find WHICH surviving entry
// is the device THIS context is running on, to apply via 1's own "some
// OTHER entry is `shared`" rule).
[[nodiscard]] gltfx_rslt<std::vector<egl_device_facts>> enumerate_egl_devices() noexcept;

// Public surface behind gltfx_gpu_enumeration::query() (gpu_
// enumeration_facade.cpp) on Linux - `names_out` is owning storage
// RESERVED to its final size BEFORE any gltfx_gpu_info::name string_
// view is taken from it (gpu_enumeration_impl.hpp's own header
// comment: no reallocation may ever invalidate a view already handed
// out).
[[nodiscard]] gltfx_rslt<std::vector<gltfx_gpu_info>>
enumerate_gpus_egl(std::vector<std::string> &names_out) noexcept;

} // namespace glintfx::platform
