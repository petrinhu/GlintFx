// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

// platform/gl/gpu.hpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, sec. 10, D-W6b-13/14, GODS_LAWS.md L-19/L-26): the ONE
// thing v1 promises about which GPU renders a context - INFORMING,
// never CHOOSING (sec. 10.1's own busca: choosing a GPU is a request
// the executable itself has to make on Windows - exported variables an
// .exe carries, not a .dll - and a best-effort environment variable
// read by the DRIVER on Linux; neither is something this library can
// guarantee alone, on either system).
//
// `unknown` IS THE HONEST DEFAULT, NEVER "PROVAVELMENTE INTEGRADA"
// (D-W6b-13, verbatim in the plan): a system that never told this
// library which GPU it is using reports `unknown`, not a guess. The
// classification RULES themselves (sec. 10.1 D-W6b-15: `AMDGPU_IDS_
// FLAGS_FUSION`, DXGI's `HIGH_PERFORMANCE`/`MINIMUM_POWER` ordering,
// and so on) are NOT this header's job - they live in the two
// platform-specific atoms fatia 5b adds (drm_gpu_kind.{hpp,cpp},
// dxgi_gpu_kind.{hpp,cpp}); this header only freezes the SHAPE of the
// answer, gltfx_gl_context::gpu() (context.hpp, this fatia) is the ONE
// place a consumer reads it from.
//
// gltfx_gpu_enumeration (the "several GPUs, on demand" handle,
// D-W6b-20) is DELIBERATELY NOT HERE YET - it belongs to fatia 5b,
// which needs a live context to know which GPU a driver actually
// picked before enumerating the rest. Adding it there is a compatible
// growth of this same header, never a redesign of what freezes today.

namespace glintfx {

// Four values, closed (sec. 10.1's own "as regras sao FECHADAS", plus
// D-W6b-13's own contract): `unknown` is both the numeric first value
// AND gltfx_gpu_info's own default-constructed state below - the same
// "a value nobody set reads back as the honest default" convention
// gltfx_gfx_option_when::read_only plays for gltfx_gfx_option_info
// (gfx_option.hpp). std::uint8_t: a closed, structural vocabulary that
// never grows the way gltfx_gfx_option's own append-only ids do.
enum class gltfx_gpu_kind : std::uint8_t {
    unknown,
    software,
    shared,
    dedicated,
};

// What a concrete, open gltfx_gl_context knows about the GPU it is
// rendering on, right now (D-W6b-13's own consolidation of the two
// separate accessors an earlier draft of this plan proposed - gpu_
// kind()/gpu_name() - into the ONE value type gltfx_gl_context::gpu()
// returns). `name` is whatever GL_RENDERER the driver reports - always
// available once a context is current, even when `kind` itself is
// `unknown` - so a consumer always has SOMETHING to put in a support
// log, even on a system this library cannot classify.
//
// `name` IS NEVER INTERPRETED BY TEXT (sec. 10's CTO instruction,
// verbatim): GL_RENDERER has no standardized format, changes without
// notice between driver versions, and can fail to identify anything
// meaningful at all - this is exactly the mistake the busca's own
// finding names ("alguem no mundo real acusou de software um driver
// acelerado por conter certa palavra no nome"). `kind` is decided by
// TYPE (the platform-specific atoms of fatia 5b), never by scanning
// this string - `name` exists for a human support log, not for
// program logic.
//
// `enumeration_index` is the position this GPU would have in
// gltfx_gpu_enumeration::at() (fatia 5b) - meaningful only once that
// handle exists; until then it is always 0 (the current, and only,
// GPU this v1 has any way to describe). NOT spelled `index` (docs/
// api-conventions.md R6): the bare form collides with a REAL, active
// macro this project's own five target platforms can still reach
// transitively through a windowing system header this project's own
// dependency-zero rule does not forbid a CONSUMER from also including
// alongside glintfx - a legacy pre-POSIX BSD compatibility `#define`
// aliasing `index` to `strchr` - found live by tests/tools/check_
// public_name_collision.py's own real-compiler scan, the same class of
// finding that already renamed gltfx_err_code away from `error_code`.
struct gltfx_gpu_info {
    gltfx_gpu_kind kind = gltfx_gpu_kind::unknown;
    std::string_view name;
    std::uint32_t enumeration_index = 0;
};

} // namespace glintfx
