// SPDX-License-Identifier: AGPL-3.0-or-later
//
// mat3.cpp - the once-per-frame build of the matrix that reaches the
// graphics card. READ include/glintfx/core/mat3.hpp FIRST: the storage
// order below is COLUMN BY COLUMN, which looks transposed next to a
// textbook and is not a mistake.

#include <cmath>

#include <glintfx/core/mat3.hpp>

namespace glintfx {

gltfx_mat3 gltfx_mat3_from_transform(gltfx_transform t) noexcept {
    // Everything is computed in DOUBLE precision - the world side -
    // and only the nine finished numbers are narrowed on the way out.
    const double cosine = std::cos(t.rotation.radians);
    const double sine = std::sin(t.rotation.radians);

    // Scale first, then rotation: the scale factor of an axis rides on
    // the column that axis owns. Translation is applied last, so it
    // enters untouched by either.
    const double column0_row0 = t.scale.x * cosine;
    const double column0_row1 = t.scale.x * sine;
    const double column1_row0 = -t.scale.y * sine;
    const double column1_row1 = t.scale.y * cosine;

    return gltfx_mat3{.column_major = {
                          // column 0
                          static_cast<float>(column0_row0),
                          static_cast<float>(column0_row1),
                          0.0F,
                          // column 1
                          static_cast<float>(column1_row0),
                          static_cast<float>(column1_row1),
                          0.0F,
                          // column 2 - the translation, contiguous at
                          // the end, which is the signature of
                          // column-major storage.
                          static_cast<float>(t.translation.x),
                          static_cast<float>(t.translation.y),
                          1.0F,
                      }};
}

} // namespace glintfx
