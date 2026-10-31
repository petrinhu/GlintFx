// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/shadow_group.hpp"

namespace glintfx::draw2d {

run_passes passes_of_run(stencil_role role, bool shadow_came) noexcept {
    if (role == stencil_role::shadow) {
        // Only the first shadow of the segment clears: a later one must keep the marks.
        return run_passes{!shadow_came, {stencil_pass::mark_shadow, stencil_pass::plain}, 1};
    }
    if (!shadow_came) {
        return run_passes{false, {stencil_pass::plain, stencil_pass::plain}, 1};
    }
    return run_passes{false, {stencil_pass::plain, stencil_pass::release_marks}, 2};
}

bool shadow_came_after(stencil_role role, bool shadow_came) noexcept {
    return shadow_came || role == stencil_role::shadow;
}

} // namespace glintfx::draw2d
