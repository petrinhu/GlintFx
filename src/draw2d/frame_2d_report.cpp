// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/frame_2d_report.hpp>

namespace glintfx {

std::string_view gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal reason) noexcept {
    switch (reason) {
    case gltfx_draw_2d_refusal::none:
        return "none";
    case gltfx_draw_2d_refusal::not_a_number:
        return "not_a_number";
    case gltfx_draw_2d_refusal::infinite:
        return "infinite";
    case gltfx_draw_2d_refusal::negative_size:
        return "negative_size";
    }
    return "unknown"; // a value a NEWER glintfx produced (docs/api-conventions.md R4)
}

} // namespace glintfx
