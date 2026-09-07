// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <span>
#include <vector>

#include <glintfx/platform/gl/gpu.hpp>

// platform/gl/gpu_kind_exclusion.hpp - GL-GPU-KIND, "via 1" (docs/
// plano-w6b-fatias-5b-revisao.md sec. 1.5/3, D-W6b-38, the lider's own
// order of 06/09/2026 verbatim: "podemos consultar o OS se tem mais de
// uma placa. Se tiver, perguntamos ao sistema se a OUTRA placa e
// integrada. Se for, por exclusao, a nvidia sera dedicada"): a PURE
// atom over an ALREADY-CLASSIFIED enumeration - it never talks to the
// kernel or a driver itself, it only reasons about what the kernel-
// backed classifier (drm_gpu_kind.hpp) already answered for every
// entry.
//
// THE RULE, EXACTLY (revisao.md sec. 1.5): every `unknown` entry is
// promoted to `dedicated` IF at least one OTHER entry in the same
// enumeration is `shared`, AS ANSWERED BY THE KERNEL (never another
// promoted `unknown`, never a `dedicated` or `software` neighbour -
// only a kernel-confirmed `shared` counts as "the integrated one").
// `shared` and `dedicated` and `software` entries are NEVER rebaixados
// nem promovidos by this atom - only `unknown` entries ever change.
//
// THE PREMISE THIS RULE RESTS ON IS INFERENCE, NOT FACT (revisao.md
// sec. 1.5, the reviewer's own 06/09/2026, 19:51 annotation): "um
// computador tem no maximo uma placa integrada" - if that premise is
// false (two integrated GPUs, one served by a driver the kernel does
// not classify), this atom promotes BOTH `unknown` entries, which is
// wrong. The damage is bounded by design (gpu.hpp's own header
// comment: `kind` is INFORMATION, never a choice) - see this file's
// own .cpp for the exact cell this documents.
//
// PLATFORM-AGNOSTIC ON PURPOSE (D-W6b-38's own header comment for this
// file): it also serves as WGL's own reserve when DXCore itself does
// not answer IsIntegrated (D-W6b-37 regra 4) - the SAME reasoning,
// over the SAME enum, regardless of which kernel or driver produced
// the input.

namespace glintfx::platform {

[[nodiscard]] std::vector<gltfx_gpu_kind>
apply_gpu_kind_exclusion(std::span<const gltfx_gpu_kind> kinds) noexcept;

} // namespace glintfx::platform
