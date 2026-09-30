// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <utility>

#include <glintfx/core/err.hpp>

// platform/gl/gl_context_access.hpp - R2D-BATCH, fatia B4 (PLANO-errata.md sec. 18 (b), D-API-06):
// the ONLY header of src/platform/ the drawing layer (src/draw2d/) includes. It gives the layer
// what it needs from the INTERIOR of a graphics context - the resolver of GL functions, the size of
// the surface and the making-current - as free functions over the opaque interior pointer that
// gl_context_internal_access::get() returns (context.hpp), and it includes NOTHING but the stdlib
// and the public error type: not the adapter of the platform, not a header of the operating system
// (GODS_LAWS.md L-19).
//
// It takes the INTERIOR, not the handle, on purpose: the renderer holds what the context handle
// points to, so moving the handle while the renderer is alive stays harmless (B0, D-API-06); a
// function over the handle would dangle the moment the handle moved.
//
// Defined in src/platform/gl/gl_context_facade.cpp, over the inline accessors of
// gl_context_impl.hpp.
namespace glintfx {

// The resolver of a context, in the shape the generated GL loader takes
// (render::gl_context_proc_address_fn): `interior` is the pointer this header's other functions
// take, and the answer is what the platform's own proc-address call gives for `name` (null when the
// driver does not have it). The context must be current on the calling thread, as for any GL
// function pointer.
[[nodiscard]] void *gl_context_interior_resolve(void *interior, const char *name) noexcept;

// The size of the drawing surface in PHYSICAL pixels {width, height}; {0, 0} without a surface.
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t>
gl_context_interior_surface_size(void *interior) noexcept;

// Makes the context current on the calling thread; the context's own error when it cannot.
[[nodiscard]] gltfx_rslt<void> gl_context_interior_make_current(void *interior) noexcept;

} // namespace glintfx
