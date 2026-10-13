// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_release_queue.hpp - R2D-TEXTURE, fatia T3 (D-W8-288): the textures a renderer
// holds for release. A texture destroyed while its renderer has a frame open is NOT released at
// once: pieces already queued may still draw with it, and releasing it first would mean sending the
// batch, a barrier that changes the order of layers. The renderer holds the record here and
// releases every one when the frame ends (finish_frame()), at the next begin_frame() of an
// abandoned frame, or when the renderer is destroyed; each release makes the texture's context
// current first (texture_release.hpp, release_texture_now()).
//
// The list runs through the records themselves (texture_2d_impl::next_held), so holding a texture
// asks for no memory and can never fail. The owner releases what it holds before it is destroyed.
namespace glintfx::draw2d {

class texture_release_queue {
  public:
    texture_release_queue() = default;
    texture_release_queue(const texture_release_queue &) = delete;
    texture_release_queue &operator=(const texture_release_queue &) = delete;
    texture_release_queue(texture_release_queue &&) = delete;
    texture_release_queue &operator=(texture_release_queue &&) = delete;
    ~texture_release_queue() = default;

    // Takes `texture` (not null) at the end of the queue; release_held() releases it.
    void hold(glintfx::texture_2d_impl *texture) noexcept;

    // Releases every held texture, in the order they came, and leaves the queue empty. With nothing
    // held, it does nothing.
    void release_held() noexcept;

    [[nodiscard]] bool empty() const noexcept;

  private:
    glintfx::texture_2d_impl *first = nullptr;
    glintfx::texture_2d_impl *last = nullptr;
};

} // namespace glintfx::draw2d
