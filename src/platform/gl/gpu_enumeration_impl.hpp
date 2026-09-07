// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include <glintfx/platform/gl/gpu.hpp>

// gpu_enumeration_impl.hpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md
// sec. 4.3, D-W6b-33): the concrete type glintfx::gpu_enumeration_impl
// (forward-declared, opaque, in the public include/glintfx/platform/
// gl/gpu.hpp) actually IS - the same shape gl_context_impl.hpp already
// gives gltfx_gl_context, one file over.
//
// `names` OWNS THE STORAGE `entries[i].name` POINTS INTO: reserved (or
// assign()ed) to its FINAL size in gpu_enumeration_facade.cpp's own
// query() BEFORE a single gltfx_gpu_info is constructed - std::vector
// growth after that point would move every std::string it holds,
// silently invalidating every string_view already handed out (a
// std::string under the small-string-optimization threshold lives
// INSIDE the vector's own element storage, not on a separate heap
// allocation the move would leave untouched).

namespace glintfx {

struct gpu_enumeration_impl {
    std::vector<gltfx_gpu_info> entries;
    std::vector<std::string> names;
};

} // namespace glintfx
