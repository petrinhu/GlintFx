// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>

#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_facade_shadows.cpp - R2D-SPRITE-SHADE H3b-2 (docs/auditoria-api-sprite-shade.md,
// sec. 2): the public methods of the SHADOW family, in their own file as the family opens. Each
// method is a forward to the frame machine (renderer_2d_impl_shadows.cpp), with the same moved-from
// precondition as the rest of the facade.
namespace glintfx {

// cppcheck-suppress-begin passedByValue
// reason: gltfx_quad_world is a VALUE type, and fill_shadow() takes it by value as
// docs/auditoria-api-sprite-shade.md fixes (D-W8-354): the copy is a few doubles, and a const
// reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::fill_shadow(gltfx_quad_world shadow, gltfx_rgba tint,
                                    gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::fill_shadow() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->fill_shadow(shadow, tint, layer);
}
// cppcheck-suppress-end passedByValue

} // namespace glintfx
