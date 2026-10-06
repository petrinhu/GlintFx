// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>

// draw2d/srgb_encoding.hpp - R2D-BATCH, fatia B4 (docs/plano-w7d.md D-W7D-12,
// docs/auditoria-api-draw2d.md B0 "COLOR, frozen"): the sRGB encoding of a LINEAR light value, for
// the one place the CPU does what the drawing program does on the card - the clear color of a frame
// - when the context option `srgb_framebuffer` is OFF (with it on, the surface encodes for us). The
// standard piecewise curve: 12.92 * x below 0.0031308, and 1.055 * x^(1/2.4) - 0.055 above it. A
// value below zero encodes as zero; a value above one is left above one (the surface clips it, not
// the type).
namespace glintfx::draw2d {

[[nodiscard]] float srgb_encode(float linear) noexcept;

// The three color channels encoded, the alpha untouched (alpha is coverage, not light).
[[nodiscard]] glintfx::gltfx_rgba srgb_encode(glintfx::gltfx_rgba linear) noexcept;

} // namespace glintfx::draw2d
