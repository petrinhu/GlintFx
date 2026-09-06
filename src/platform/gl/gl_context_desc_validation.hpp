// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>

// platform/gl/gl_context_desc_validation.hpp - GL-CONTEXT (docs/plano-
// w6b-placa-e-laco.md fatia 2b, D-W6b-14/16/17, GODS_LAWS.md L-17/
// L-19/L-22): the ONE pure atom gltfx_gl_context::open() (gl_context_
// facade.cpp) calls, BEFORE gfx_open_only_fixation.hpp (fatia 2a) ever
// sees the request and BEFORE any adapter is asked whether it supports
// anything - validates the WHOLE gltfx_gl_context_desc a caller
// assembled, entry by entry, against the append-only options table
// (gfx_option_registry.hpp).
//
// FOUR REASONS THE WHOLE DESCRIPTOR IS REFUSED, ALL
// gltfx_err_code::invalid_argument WITH rejected_value() NAMING THE
// OPTION (docs/api-conventions.md R7 - a token, never a sentence):
//
//   1. `options == nullptr` while `option_count > 0` - never
//      dereferenced (rejected_value() == "options").
//   2. Any single entry gfx_option_validation.hpp's own validate_gfx_
//      option_entry() itself refuses (an id outside the table, a value
//      outside its own range, or a `read_only` entry - see that
//      header's own comment for the four reasons; `already_open` is
//      always `false` here, since this only ever runs at open() time).
//   3. The SAME id appears twice in the list - the FIRST duplicate
//      found (in list order) is what rejected_value() names; a list
//      that assigns two different values to one option has no honest
//      "which one wins" answer, so this atom refuses instead of
//      picking one silently.
//   4. `gpu_preference` (gfx_option.hpp) requested as anything OTHER
//      than `no_preference` (value 0) - D-W6b-14's own reserved-but-
//      unhonored porta: the field exists in the table so a FUTURE
//      GL-GPU-PREFERENCE fatia can honor it without ever growing this
//      descriptor's own layout, but until that fatia lands, asking for
//      it is refused BY NAME instead of silently doing nothing (the
//      exact defect D-W6b-14's own comment calls "o pior dos mundos").
//
// WHAT THIS ATOM DOES NOT DO: it never touches gfx_open_only_fixation
// (a SEPARATE, later step in open()'s own sequence, sec. 14.2 - "a
// fixacao vem antes do suporte" is about fixation-vs-adapter-support,
// not about this shape-level validation, which runs first of all
// three); it never asks an adapter whether an option is `unsupported_
// here` (that answer needs a concrete system, and this atom is pure).

namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void> validate_gl_context_desc(const gltfx_gl_context_desc &desc) noexcept;

} // namespace glintfx::platform
