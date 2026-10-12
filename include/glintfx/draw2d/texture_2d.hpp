// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/export.hpp>

// draw2d/texture_2d.hpp - R2D-TEXTURE. A texture on the
// graphics card: a move-only handle, opaque (PIMPL), in the same shape as
// gltfx_renderer_2d. Created by gltfx_renderer_2d::create_texture()
// (renderer_2d.hpp), the way SDL_CreateTexture takes its renderer: the
// texture belongs to ONE renderer and to that renderer's context.
//
// LIFETIME (docs/api-conventions.md R3, R5):
//   - The texture does NOT outlive its renderer. Destroying the renderer
//     while a texture is alive is a precondition violation (the same
//     rule as the window and the context).

namespace glintfx {

class gltfx_renderer_2d;
struct texture_2d_impl;

class gltfx_texture_2d {
  public:
    gltfx_texture_2d(const gltfx_texture_2d &) = delete;
    gltfx_texture_2d &operator=(const gltfx_texture_2d &) = delete;
    GLINTFX_API gltfx_texture_2d(gltfx_texture_2d &&other) noexcept;
    GLINTFX_API gltfx_texture_2d &operator=(gltfx_texture_2d &&other) noexcept;
    GLINTFX_API ~gltfx_texture_2d();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;
    // Size in texels, fixed at creation.
    [[nodiscard]] GLINTFX_API std::uint32_t width() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint32_t height() const noexcept;

  private:
    friend class gltfx_renderer_2d;
    explicit gltfx_texture_2d(texture_2d_impl *interior) noexcept : impl(interior) {}
    texture_2d_impl *impl = nullptr;
};

} // namespace glintfx
