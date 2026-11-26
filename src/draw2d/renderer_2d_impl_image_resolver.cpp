// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/renderer_2d_impl.hpp"

// renderer_2d_impl_image_resolver.cpp - R2D-IMAGE-RESOLVER, fatia R2b: the image resolver of the
// frame machine, in a file of its own, as the ellipse and the textured methods have theirs. The
// first paint bridge (GFSS-OBJECT-FIT or R2D-NINEPATCH) is the one that asks the resolver from a
// draw call; in this version only set_image_resolver() reaches it.
namespace glintfx {

gltfx_image_resolver renderer_2d_impl::set_image_resolver(gltfx_image_resolver resolver) noexcept {
    return image_resolver.install(resolver);
}

} // namespace glintfx
