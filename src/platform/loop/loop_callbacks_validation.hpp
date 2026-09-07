// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/platform/loop/loop.hpp>

// platform/loop/loop_callbacks_validation.hpp - LOOP-RUN fatia 6a
// (docs/plano-w6b-fatias-6-8.md D-W6b-43, GODS_LAWS.md L-17/L-19/L-22):
// the ONE place gltfx_loop::run() (a later fatia, 6b) checks a
// gltfx_loop_callbacks before ever calling into any of them - the same
// "validated once, common to every caller" shape src/platform/window/
// window_desc_validation.hpp's own header comment already documents
// one directory over.
//
// THE ORDER IS THE CONTRACT (D-W6b-43, docs/plano-w6b-fatias-6-8.md
// sec. 3.2, P9): on_frame empty is checked FIRST, then on_render
// empty, then on_event non-empty - a caller with more than one
// violation always sees the SAME field name back, never one that
// depends on which check this file's own implementation happens to
// run first today.

namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void>
validate_loop_callbacks(const gltfx_loop_callbacks &callbacks) noexcept;

} // namespace glintfx::platform
