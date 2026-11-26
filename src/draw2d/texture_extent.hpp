// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/texture_extent.hpp - R2D-TEXTURE, fatia T4b-A-fix1 (D-W8-499): the size of a texture, in
// texels, the one type both the coordinates (texture_region_uv.hpp) and the range check
// (texture_region_check.hpp) read. It lives here, with no atom as its owner, so neither of the two
// includes the other for it.
namespace glintfx::draw2d {

struct texture_extent {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

} // namespace glintfx::draw2d
