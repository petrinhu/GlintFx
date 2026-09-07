// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glintfx/platform/gl/gfx_option.hpp>

// platform/gl/gfx_open_only_fixation.hpp - C-OPT (docs/plano-w6b-
// placa-e-laco.md sec. 14.1/14.2, D-W6b-25, GODS_LAWS.md L-17/L-19):
// the pure atom behind "a second context on the same window with a
// different open_only option is refused, identically, on every
// system, before any adapter is asked" - the fix for the Windows/
// Wayland divergence the CTO's own reopening review found (`SetPixel
// Format` may only run once per Win32 window; a Wayland `EGLSurface`
// would otherwise silently accept a different config on the same
// `wl_surface`).
//
// WHAT "FIXATION" MEANS, IN ONE SENTENCE: the FIRST gltfx_gl_context
// that successfully opens over a window freezes, on that window, the
// value of EVERY `open_only` option (the one requested, or the
// table's own default when none was requested) - every LATER open()
// on the SAME window is only accepted if it asks for the identical
// set, and refused, by name, the moment one differs. `live`/`read_
// only` options never enter fixation at all (D-W6b-25's own rule 1)
// - they belong to gfx_option_validation.hpp's own job, not this
// one's.
//
// THIS ATOM DOES NOT KNOW WHAT A WINDOW IS: it takes the fixed set (or
// "nothing fixed yet") and the requested list as plain data, and
// returns a decision - fatia 2b's own gltfx_gl_context::open() is what
// reads/writes the actual fixation a gltfx_window carries (via the
// window_internal_access passkey, the same idiom display_internal_
// access already uses), never this file.
//
// "FIRST DIVERGENT OPTION IN ID ORDER" (sec. 14.1's own wording,
// verbatim): resolve_gfx_open_only_fixation() walks the registry's own
// id-ascending order (gfx_option_registry.hpp's k_gfx_option_table is
// already sorted that way) and returns the FIRST open_only option
// whose resolved value disagrees - never "the last one found", never
// an unordered set of every mismatch, so the refusal is deterministic
// across two builds that iterate a hash set differently.

namespace glintfx::platform {

enum class gfx_open_only_fixation_outcome : std::uint8_t {
    // Nothing was fixed on this window yet - `fixed` below carries the
    // COMPLETE set (every open_only option, requested value or
    // default) the caller is meant to write onto the window.
    fix_now,
    // Something was already fixed, and every open_only option's
    // resolved value (requested, or default when omitted) agrees with
    // it.
    accept,
    // Something was already fixed, and `refused_id` is the FIRST
    // open_only option (in id order) whose resolved value disagrees.
    refuse,
    // docs/api-conventions.md R3 ("a lib NUNCA aborta o processo do
    // consumidor"): growing `fixed` below ran out of memory. Never
    // `fix_now`/`accept`/`refuse` - the caller (gl_context_facade.cpp)
    // must not read `fixed`/`refused_id`, both meaningless here, and
    // must translate this into gltfx_err_code::out_of_memory instead
    // of opening a context on a half-built fixation.
    alloc_failed,
};

struct gfx_open_only_fixation_result {
    gfx_open_only_fixation_outcome outcome = gfx_open_only_fixation_outcome::accept;
    // Meaningful iff outcome == fix_now.
    std::vector<gltfx_gfx_option_entry> fixed;
    // Meaningful iff outcome == refuse. Arbitrary otherwise (vsync's
    // own id 0 sorts first - see gfx_option.hpp's own default-
    // construction comment for gltfx_gfx_option_entry).
    gltfx_gfx_option refused_id = gltfx_gfx_option::vsync;
};

// `already_fixed`: std::nullopt means "nothing fixed on this window
// yet" - a present, possibly EMPTY span still means "a first context
// already opened and fixed the defaults" (an empty span never arises
// in practice, since fix_now always returns every open_only option,
// but the type does not forbid it, and this function does not assume
// non-emptiness).
[[nodiscard]] gfx_open_only_fixation_result
resolve_gfx_open_only_fixation(std::optional<std::span<const gltfx_gfx_option_entry>> already_fixed,
                               std::span<const gltfx_gfx_option_entry> requested) noexcept;

} // namespace glintfx::platform
