// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/platform/gl/gpu.hpp>

// platform/gl/gpu_kind_state.hpp - GL-CONTEXT (docs/plano-w6b-placa-
// e-laco.md fatia 2b, sec. 10.3, D-W6b-13, GODS_LAWS.md L-17/L-19):
// the ONE pure atom both adapters (fatias 3/4) embed to hold "what
// this system has told the library about its own GPU" - what
// gltfx_gl_context::gpu() (context.hpp) ultimately reads back.
//
// SHARED, NOT DUPLICATED PER PLATFORM (GODS_LAWS.md L-17): the RULES
// that decide `shared` vs `dedicated` from a real kernel/DXGI answer
// belong to fatia 5b's own platform-specific atoms (drm_gpu_kind.hpp,
// dxgi_gpu_kind.hpp) - this atom does not know what a DRM device or a
// DXGI adapter is. It only knows how to HOLD whatever the calling
// adapter decides to write, and how to hand it back - the "storage and
// default" half of D-W6b-13's own contract, kept in ONE place so every
// adapter's own "unknown until told otherwise" default is provably the
// same code, not five independently-typed copies.
//
// `unknown` IS THE BIRTH STATE, NEVER PRESUMED SOFTWARE OR HARDWARE
// (D-W6b-13, verbatim: "unknown significa 'o sistema nao disse', nunca
// 'provavelmente integrada'"): a freshly constructed gpu_kind_state
// reads back gltfx_gpu_kind::unknown with an EMPTY name - fatias 3/4
// call learn() only once something real is known (the driver's own
// EGL_MESA_device_software extension, a DXGI adapter flag, and so on
// for fatia 5b's real classification; a bare "software" from GL_
// RENDERER text pattern-matching, sec. 10's own CTO instruction,
// NEVER).
//
// NAME IS NEVER FABRICATED: learn() copies exactly the std::string_
// view it is given (typically GL_RENDERER's own lifetime-static C
// string, valid for as long as the context is current) - an empty
// name stays empty, never replaced with a placeholder.

namespace glintfx::platform {

class gpu_kind_state {
  public:
    gpu_kind_state() noexcept = default;

    // Overwrites both fields at once - a GPU never has a `kind`
    // without a `name` to go with it in this atom's own contract (an
    // empty name is still a legitimate name, per this header's own
    // "NAME IS NEVER FABRICATED" paragraph above).
    void learn(gltfx_gpu_kind kind, std::string_view name) noexcept {
        m_kind = kind;
        m_name = name;
    }

    [[nodiscard]] gltfx_gpu_info read() const noexcept {
        return gltfx_gpu_info{.kind = m_kind, .name = m_name, .enumeration_index = 0};
    }

  private:
    gltfx_gpu_kind m_kind = gltfx_gpu_kind::unknown;
    std::string_view m_name;
};

} // namespace glintfx::platform
