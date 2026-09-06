// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>

// platform/gl/gl_version_policy.hpp - GL-CONTEXT (docs/plano-w6b-
// placa-e-laco.md fatia 2b, sec. 4/D-W6b-4, GODS_LAWS.md L-04/L-17/
// L-31): the ONE pure atom the two adapters (fatias 3/4) call, AFTER a
// context is created, to decide whether what the driver actually gave
// back satisfies this library's own frozen promise - "OpenGL 3.3
// core, never forward-compatible" (L-31; D-W6b-4's own "sem forward-
// compatible: a flag derruba drivers que dao 3.3 por compatibilidade
// sem ganho nenhum").
//
// TAKES PLAIN DATA, KNOWS NOTHING OF EGL/WGL: `major`/`minor` come
// from GL_MAJOR_VERSION/GL_MINOR_VERSION, `profile_mask` from
// GL_CONTEXT_PROFILE_MASK - both queried by glGetIntegerv() on the
// now-current context, by whichever adapter calls this. The two named
// bit constants below mirror the real GL enum values bit-for-bit
// (khronos.org GL_CONTEXT_PROFILE_MASK: bit 0 = core, bit 1 =
// compatibility) - this project's own dependency-zero rule (GODS_
// LAWS.md L-07) means <GL/gl.h>'s own GL_CONTEXT_CORE_PROFILE_BIT is
// not something this atom ever includes; it names the two bits it
// needs, once, with the number cited, the same "declared by hand,
// number in the source, never copied from a vendored header" technique
// docs/plano-w6b-placa-e-laco.md sec. 9 already commits this project
// to for the Win32 WGL constants.
//
// ACCEPTS major.minor >= 3.3 AND THE CORE BIT SET (D-W6b-4's own
// promise), refuses everything else with gltfx_err_code::platform_
// failure and rejected_value() == "gl_version" - platform_failure, not
// invalid_argument, because the CALLER (a consumer's own gltfx_gl_
// context_desc) never asked for a version at all in v1 (L-31: the
// version itself is not a knob a consumer can request or has
// mis-shaped); what actually failed is the SYSTEM handing back
// something this library's own promise cannot stand on, the same
// category of failure display_backend_port's own real adapters
// already report through platform_failure when a system call itself
// refuses.

namespace glintfx::platform {

inline constexpr std::uint32_t k_gl_context_core_profile_bit = 0x00000001;
inline constexpr std::uint32_t k_gl_context_compatibility_profile_bit = 0x00000002;

[[nodiscard]] gltfx_rslt<void> validate_gl_context_version(std::uint32_t major, std::uint32_t minor,
                                                           std::uint32_t profile_mask) noexcept;

} // namespace glintfx::platform
