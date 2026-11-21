// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "gl_functions.hpp"

// own_gl_errors.hpp - D-B3c-2, the exit half (W8 deep verification, R2-B fix1, D-W8-2050). After an
// operation of the library, the queue holds the errors of THAT operation: OpenGL keeps one flag per
// kind, so one call can leave more than one. Reading only the first leaves the rest in the queue,
// and the next library entry would say them as the consumer's (draw2d_prior_gl_error). This atom
// empties the queue, at most k_max_gl_error_reads reads (prior_gl_errors.hpp: one limit for the
// drain and for the check), and answers the first error that was in it, 0 for none. The errors it
// reads are the library's own and are not logged: the caller turns the first into its refusal.

namespace glintfx::draw2d {

[[nodiscard]] render::GLenum first_own_gl_error(const render::gl_function_table &gl) noexcept;

} // namespace glintfx::draw2d
