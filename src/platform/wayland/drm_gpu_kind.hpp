// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/wayland/drm_device_facts.hpp"

// platform/wayland/drm_gpu_kind.hpp - GL-GPU-KIND (docs/plano-w6b-
// fatias-5.md sec. 4.1, D-W6b-30; docs/plano-w6b-fatias-5b-revisao.md
// sec. 1.1/3, D-W6b-38): the PURE Linux classifier - "given what the
// kernel already answered about the render node's driver, and nothing
// else, what gltfx_gpu_kind is this" - the kernel is the ONE source of
// truth (GODS_LAWS.md L-19's own "quem identifica as placas de video e
// o OS", the 06/09/2026 order this whole fatia executes): no fabricant
// table, no text pattern-matching, no bytes-of-memory heuristic (the
// mistake revisao.md sec. 1.4 M8 names).
//
// WAYLAND-DOMAIN, NOT WAYLAND-ONLY: takes only drm_device_facts (plain
// data, no OS header), compiles and is tested on all five platforms
// (tests/drm_gpu_kind_test.cpp, unguarded) - the same shape window_
// configure_sequence.hpp/frame_callback_sequence.hpp already
// established for this directory.
//
// THE DRIVER NAME TABLE IS CLOSED, AND EACH LINE IS A QUESTION TO THE
// KERNEL, NEVER A VERDICT (revisao.md sec. 3, "resposta a pergunta 4"):
// `amdgpu` -> AMDGPU_IDS_FLAGS_FUSION ("is this an APU"); `i915`/`xe`
// -> "does this device have its own VRAM region"; `nouveau` -> the
// kernel's own NVIF `platform` (or, on failure, the flatter GETPARAM
// `bus_type`) - both are the kernel naming the bus the device sits on,
// never our own guess; `nvidia-drm` -> the kernel HAS NO ANSWER (sec.
// 1.1 L6: the proprietary driver reports bus type and model, never
// class) - falls through to the two vias the LIBER ordered (sec. 1.5,
// D-W6b-38: exclusion by a neighbour DRM device the kernel DID
// classify, gpu_kind_exclusion.hpp; then GL_NVX_gpu_memory_info's own
// answer about THIS device's memory pool, memory_separation_kind.hpp)
// - both live in platform/gl/ because they run on Windows too (D-W6b-
// 37 regra 4's own reserve for when DXCore itself does not answer
// IsIntegrated). Any other driver name (`tegra`, `virtio`, `vc4`,
// `panfrost`, `msm`, `vmwgfx`, and anything not yet written, `nova`
// included, revisao.md sec. 1.6 N6) falls through the same way -
// `unknown` IS THE HONEST DEFAULT (gpu.hpp's own header comment),
// never a chuted guess.
//
// classify_drm_gpu() ITSELF NEVER APPLIES THE TWO VIAS - it only
// answers "unknown" when the kernel's own answer, for THIS device
// alone, does not resolve to shared/dedicated; the CALLER (egl_
// context_adapter.cpp, this fatia) is the one place that walks kernel
// -> via 1 -> via 2 -> unknown, because only the caller has the WHOLE
// enumeration (via 1's own "some OTHER entry") and a live GL context
// (via 2's own GL_NVX_gpu_memory_info read).

namespace glintfx::platform {

[[nodiscard]] gltfx_gpu_kind classify_drm_gpu(const drm_device_facts &facts) noexcept;

} // namespace glintfx::platform
