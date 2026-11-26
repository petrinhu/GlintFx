// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/image_resolution.hpp"

// image_resolution.cpp - R2D-IMAGE-RESOLVER, fatia R2b: see image_resolution.hpp. No allocation and
// no exception: the reference lives on the stack of resolve(), and the resolver type is noexcept.
namespace glintfx::draw2d {

gltfx_image_resolver installed_image_resolver::install(gltfx_image_resolver resolver) noexcept {
    const gltfx_image_resolver previous = installed;
    installed = resolver;
    return previous;
}

gltfx_image_resolution installed_image_resolver::resolve(std::string_view text) const noexcept {
    if (installed.function == nullptr) {
        return {}; // no resolver: no owner, and nothing to call
    }
    const gltfx_image_reference reference{text};
    return installed.function(installed.context, &reference);
}

} // namespace glintfx::draw2d
