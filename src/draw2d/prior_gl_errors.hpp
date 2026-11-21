// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "gl_functions.hpp"

// prior_gl_errors.hpp - D-B3c-2 (DECISOES_AUTONOMAS.md; W8 deep verification I-2, D-W8-2018).
// glGetError answers the FIRST error of a queue that also holds the CONSUMER's own. Every library
// entry that later reads the queue for its own errors drains it first, and each error drained is
// SAID in the log, never swallowed and never blamed on the library: the event
// draw2d_prior_gl_error (category "draw2d", severity warn) with the unsigned field gl_error. The
// drain reads the queue at most k_max_gl_error_reads times, so a GL that never empties it (a lost
// context) cannot hold the caller. The entries that share it: vertex_stream (creation and upload),
// texture creation, texture update and the stencil question of the surface.

namespace glintfx::draw2d {

inline constexpr std::string_view k_prior_gl_error_event = "draw2d_prior_gl_error";
inline constexpr std::string_view k_gl_error_field = "gl_error";
// The most queue reads one drain or one check makes: a GL that never empties its queue cannot hold
// us.
inline constexpr int k_max_gl_error_reads = 16;

void drain_prior_gl_errors(const render::gl_function_table &gl) noexcept;

} // namespace glintfx::draw2d
