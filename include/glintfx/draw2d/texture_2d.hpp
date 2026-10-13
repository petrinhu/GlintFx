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
//   - Destroying the texture takes the graphics object out of the consumer's
//     hands at once. The renderer keeps the GL name in a list and releases it
//     at finish_frame(), when its context is already current. If a frame is
//     abandoned, the release happens at the next begin_frame(), or when the
//     renderer is destroyed. Destroying the texture OUTSIDE an open frame
//     releases it at once, after making the renderer's context current on the
//     calling thread (the rule begin_frame() follows, D-API-07).
//   - If the release fails, the graphics object leaks, the destructor does not
//     throw and does not return an error, and the event
//     `texture_release_failed` says so (identifier plus typed fields, the same
//     shape as `draw2d_program_rejected`).
//   - A piece already queued that uses the texture still draws with it, and
//     the order of layers does not change. Destruction is NOT a barrier: it
//     does not send the batch (see renderer_2d.hpp, update_texture).
//
// Proved by: renderer_2d_impl_test (destruction with a piece in the queue
// draws the piece and keeps layer order; the release happens at
// finish_frame(); a release outside a frame makes the context current first
// and emits the event on a failing release; pure, with a fake GL table).

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
