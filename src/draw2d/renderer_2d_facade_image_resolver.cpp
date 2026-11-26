// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>

#include <glintfx/draw2d/image_resolver.hpp>

#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_facade_image_resolver.cpp - R2D-IMAGE-RESOLVER, fatia R2b: the public method of the
// IMAGE RESOLVER, in its own file, as the shape and the texture families have theirs. It forwards
// to the frame machine (renderer_2d_impl.hpp), with the same moved-from precondition as the rest of
// the facade.
namespace glintfx {

gltfx_image_resolver gltfx_renderer_2d::set_image_resolver(gltfx_image_resolver resolver) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::set_image_resolver() called on a moved-from "
                              "renderer - the object no longer owns its drawing state");
    return impl->set_image_resolver(resolver);
}

} // namespace glintfx
