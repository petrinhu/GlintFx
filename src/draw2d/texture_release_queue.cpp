// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_release_queue.hpp"

#include "draw2d/texture_release.hpp"

// draw2d/texture_release_queue.cpp - R2D-TEXTURE, fatia T3 (D-W8-288): the queue of
// texture_release_queue.hpp. The list runs through the records (next_held), so holding asks for no
// memory: the next record is read BEFORE the current one is released, because release_texture_now()
// frees it.
namespace glintfx::draw2d {

void texture_release_queue::hold(glintfx::texture_2d_impl *texture) noexcept {
    texture->next_held = nullptr;
    if (first == nullptr) {
        first = texture;
    } else {
        last->next_held = texture;
    }
    last = texture;
}

void texture_release_queue::release_held() noexcept {
    // The queue is emptied BEFORE the loop: a release that held again would start a new list and
    // not walk into a freed record.
    glintfx::texture_2d_impl *texture = first;
    first = nullptr;
    last = nullptr;
    while (texture != nullptr) {
        glintfx::texture_2d_impl *const next = texture->next_held;
        release_texture_now(texture);
        texture = next;
    }
}

bool texture_release_queue::empty() const noexcept { return first == nullptr; }

} // namespace glintfx::draw2d
