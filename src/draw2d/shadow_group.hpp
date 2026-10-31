// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// draw2d/shadow_group.hpp - R2D-SPRITE-SHADE, slice H3a (docs/auditoria-api-sprite-shade.md
// sec. 4.2, D-W8-319): the PURE atom that says how the stencil is used for one run of a batch, so
// that the shadows of one SEGMENT darken a pixel at most once. A segment is the drawing between two
// barriers, flush() or finish_frame(); one segment is one batch. Its "shadow group" is every shadow
// of the segment, drawn under one rule. It knows nothing of GL: it takes what a run is and whether
// a shadow already came, and gives the passes, so it compiles and runs on all five systems.
//
// THE RULE (the contract of fill_shadow() in the audit, sec. 2): a pixel is darkened by the shadows
// of a segment at most once, until an OPAQUE piece of that segment is drawn over it. Every piece of
// the segment takes part, shadow or not, in paint order:
//   - a shadow marks the pixels it darkens, and darkens only pixels not yet marked;
//   - an ordinary piece drawn after the first shadow paints, and then releases the mark where it
//     is opaque (alpha 1 on that pixel), so a later shadow darkens it again;
//   - an ordinary piece drawn before the first shadow only paints: nothing is marked yet, so the
//     stencil costs nothing until the first shadow of the segment (audit, sec. 4.2);
//   - the stencil is cleared once, before the first shadow of the segment, so no mark crosses a
//     barrier and nothing is assumed of what the consumer left in it.
// How each pass sets GL is gl_state_contract.hpp's; which fragments it discards is
// embedded_program.hpp's.
namespace glintfx::draw2d {

// What a run is to the stencil.
enum class stencil_role : std::uint8_t {
    ordinary = 0, // any piece that is not a shadow
    shadow = 1,   // a shadow: it darkens what is already drawn under it
};

// One pass of a run over the stencil.
enum class stencil_pass : std::uint8_t {
    plain = 0,         // paints as always, stencil test off
    mark_shadow = 1,   // paints where not yet marked, and marks what it paints
    release_marks = 2, // paints nothing, and clears the mark where the piece is opaque
};

// The passes of one run, in the order they are drawn, and whether the stencil is cleared first.
struct run_passes {
    bool clear_stencil_first = false;
    std::array<stencil_pass, 2> passes{};
    std::size_t count = 0;
};

// The passes of a run of `role`, when `shadow_came` says whether a shadow run came before it in the
// same segment (false at the start of every segment).
[[nodiscard]] run_passes passes_of_run(stencil_role role, bool shadow_came) noexcept;

// Whether a shadow has come once a run of `role` is drawn: the `shadow_came` of the next run.
[[nodiscard]] bool shadow_came_after(stencil_role role, bool shadow_came) noexcept;

} // namespace glintfx::draw2d
