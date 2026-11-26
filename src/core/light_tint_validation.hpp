// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/light_tint.hpp>
#include <glintfx/core/vec2.hpp>

// core/light_tint_validation.hpp - R2D-SPRITE-SHADE, H4 (docs/auditoria-api-sprite-shade.md,
// section 2.1; decisions D-W8-325, D-W8-355, D-W8-380 and D-W8-397). Which input of
// gltfx_light_tint() is refused, and by which name. One subject, taken out of light_tint.cpp, which
// only computes the tints (GODS_LAWS.md L-17: a sentence with no "and"). NOT under
// include/glintfx/: internal, not frozen ABI. Pure: no state, no allocation, no operating system.
//
// Every refusal is a gltfx_err with code invalid_argument and the refused field's name in
// rejected_value(), the names of core/light_tint.hpp's own refusal paragraph. The checks run in
// this order, which the public contract does NOT promise (each test case breaks one field only):
// the size of out_tints, the vertices, the ambient, then each light in the order of the span, its
// position, range, intensity, colour and fall-off.
namespace glintfx {

// True when the red, green and blue of `light` are finite and zero or more: the ambient, and the
// colour of a light. The alpha is not read (light has no opacity, D-W8-397). A NaN fails, an
// infinity fails, -0.0 passes (it compares equal to zero).
[[nodiscard]] bool is_light_amount(gltfx_rgba light) noexcept;

// The whole input of gltfx_light_tint(), in the order above. `tint_count` is out_tints.size().
[[nodiscard]] gltfx_rslt<void>
validate_light_tint_input(std::span<const gltfx_vec2_world> vertices,
                          std::span<const gltfx_light_source_2d> lights, gltfx_rgba ambient,
                          std::size_t tint_count) noexcept;

} // namespace glintfx
