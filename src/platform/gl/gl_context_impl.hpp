// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

#if defined(_WIN32)
#include "platform/win32/selected_gl_context_adapter.hpp"
#include "platform/win32/selected_power_source_adapter.hpp"
#else
#include "platform/wayland/selected_gl_context_adapter.hpp"
#include "platform/wayland/selected_power_source_adapter.hpp"
#endif

// gl_context_impl.hpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, D-W6b-1, GODS_LAWS.md L-17/L-19): the concrete type
// glintfx::gl_context_impl (forward-declared, opaque, in the public
// include/glintfx/platform/gl/context.hpp) actually IS - the same
// shape display_impl.hpp/window_impl.hpp already give their own
// public handles, one directory over.
//
// selected_gl_context_adapter DOES NOT EXIST YET (declared here, by
// this SAME fatia, ONLY as the #include this file expects to find):
// src/platform/wayland/selected_gl_context_adapter.hpp (fatia 3) and
// src/platform/win32/selected_gl_context_adapter.hpp (fatia 4) are
// each a one-line `using` alias, the exact same shape selected_window_
// adapter.hpp already has one directory over - see src/platform/gl/
// CMakeLists.txt's own header comment for why gl_context_facade.cpp
// (the ONE translation unit that includes THIS header) is not yet
// wired into glintfx_library's own target_sources().
//
// current_values IS THE PER-CONTEXT "WHAT gltfx_gl_context::option()
// READS BACK" TABLE (D-W6b-2's own set_option()/option() pair,
// context.hpp): populated once, at open() time, with every row of the
// options registry (gfx_option_registry.hpp) at either its resolved
// open_only value (from gfx_open_only_fixation.hpp's own `fixed` set)
// or, for a `live`/`read_only` row, whatever the opening list requested
// or the registry's own default - and updated in place by set_option()
// afterwards. Deliberately NOT the registry's own inline table itself
// (that is SHARED, read-only, one copy for the whole process); this is
// the mutable, PER-CONTEXT mirror of it, the same "shared static table,
// per-instance live copy" split gfx_open_only_fixation.hpp's own
// `fixed` vector already establishes for the open_only subset alone.
namespace glintfx {

struct gl_context_impl {
    platform::selected_gl_context_adapter adapter;
    // Where the machine's power comes from, reread at EVERY read of `power_source`,
    // `suggested_preset` or `auto_choice_reason` and at every `preset = automatic` (D-W6b-34):
    // never cached, never in a loop, never on its own. Stateless.
    platform::selected_power_source_adapter power;
    std::vector<gltfx_gfx_option_entry> current_values;
};

// The resolver of THIS context, in the exact shape the loader of src/render/ takes (D-W7D-15,
// gl_context_proc_address_fn): `user` is the gl_context_impl the drawing layer got from
// gl_context_internal_access::get(), and the answer is what the adapter's own proc_address() gives
// (eglGetProcAddress on Linux, wglGetProcAddress with the opengl32 fallback on Windows). The
// context must be current on the calling thread, as for any GL function pointer. Inline, so the
// drawing layer (src/draw2d/) needs nothing of the platform adapters but this header.
[[nodiscard]] inline void *gl_context_resolve_proc(void *user, const char *name) noexcept {
    return static_cast<gl_context_impl *>(user)->adapter.proc_address(name);
}

// The size of the drawing surface of THIS context in physical pixels {width, height} (what the
// drawing layer gives glViewport), through the adapter's own surface_pixel_size(). `user` is the
// gl_context_impl, as for gl_context_resolve_proc().
[[nodiscard]] inline std::pair<std::uint32_t, std::uint32_t>
gl_context_surface_pixel_size(const void *user) noexcept {
    return static_cast<const gl_context_impl *>(user)->adapter.surface_pixel_size();
}

} // namespace glintfx
