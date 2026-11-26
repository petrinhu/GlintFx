// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/texture_2d.hpp>

#include <cstdint>
#include <utility>

#include "draw2d/texture_2d_impl.hpp"
#include "draw2d/texture_release.hpp"

// draw2d/texture_2d_facade.cpp - R2D-TEXTURE, fatia T2a: the public handle gltfx_texture_2d. Each
// member is a forward: the making lives in texture_upload.hpp, the release in texture_release.hpp,
// the record in texture_2d_impl.hpp. A moved-from handle owns nothing and releases nothing.
namespace glintfx {

gltfx_texture_2d::gltfx_texture_2d(gltfx_texture_2d &&other) noexcept
    : impl(std::exchange(other.impl, nullptr)) {}

gltfx_texture_2d &gltfx_texture_2d::operator=(gltfx_texture_2d &&other) noexcept {
    if (this != &other) {
        draw2d::release_texture_object(impl);
        impl = std::exchange(other.impl, nullptr);
    }
    return *this;
}

gltfx_texture_2d::~gltfx_texture_2d() { draw2d::release_texture_object(impl); }

bool gltfx_texture_2d::is_open() const noexcept { return impl != nullptr; }

std::uint32_t gltfx_texture_2d::width() const noexcept {
    return impl == nullptr ? 0U : impl->width;
}

std::uint32_t gltfx_texture_2d::height() const noexcept {
    return impl == nullptr ? 0U : impl->height;
}

} // namespace glintfx
