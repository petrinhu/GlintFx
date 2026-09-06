// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

#if defined(_WIN32)
#include "platform/win32/selected_gl_context_adapter.hpp"
#else
#include "platform/wayland/selected_gl_context_adapter.hpp"
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
    std::vector<gltfx_gfx_option_entry> current_values;
};

} // namespace glintfx
