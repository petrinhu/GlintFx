// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <type_traits>

#include <glintfx/export.hpp>

// core/color.hpp - gltfx_rgba (CORE-COLOR, TODO.md, GODS_LAWS.md
// L-19/L-26): the frozen-layout color value type. Value type of the
// core layer (GODS_LAWS.md L-19's opacity clause is scoped to "handle
// e subsistema com estado" - this is neither; the layout stable IS
// the contract, the same exception glintfx::version already uses).
//
// SIX DECISIONS OF THE PROJECT LEADER, 26/08/2026, via AskUserQuestion
// (see /var/tmp/glintfx-plan/core-color-opcoes.md for the full option
// analysis these answer - CTO Caetano, L-27 provenance):
//
//   1. FOUR 32-BIT FLOATS, 16 BYTES - not small integers. brilho
//      intenso e soma de luz need to represent light BRIGHTER than
//      white, and oklch() describes colors OUTSIDE the display's
//      gamut - both would be amputated at birth by an 8-bit-per-
//      channel layout.
//   2. ALPHA LIVES INSIDE THIS TYPE, not bolted on later - adding a
//      channel afterwards would change sizeof(), an ABI break
//      (GODS_LAWS.md L-26) this project can still avoid by deciding
//      once, now.
//   3. THE NUMBERS ARE PHYSICAL LIGHT, LINEAR SCALE - not the
//      display-encoded (gamma/sRGB-curve) value a monitor receives.
//      This is the ONLY convention under which summing two lights
//      (additive blending) gives the physically correct result (CSS
//      Color 4 SS13.1: "the CIE XYZ, display-p3-linear or srgb-linear
//      color spaces are appropriate... because they are linear in
//      light intensity"). CONSEQUENCE A HUMAN WILL FIND SURPRISING,
//      documented here on purpose: display-encoded gray #808080 does
//      NOT become the linear number 0.5 - see
//      gltfx_rgba_from_srgb8() below, and color_test.cpp's round-trip
//      test, which fixes the exact number this project committed to.
//   4. PUBLIC NAME: `gltfx_rgba` - the same short prefix already
//      chosen for gltfx_err/gltfx_rslt, for the same reason
//      (docs/api-conventions.md R6: an unprefixed name risks
//      colliding with a name the standard library or a system header
//      already uses).
//   5. STRAIGHT ALPHA, never premultiplied, inside this type - the
//      same convention CSS Color 4 SS13.3 documents ("changing the
//      color components to premultiplied form -> linearly
//      interpolating -> undoing premultiplication"): the standard
//      this project models premultiplies only TRANSIENTLY, for the
//      duration of one computation, then undoes it.
//      gltfx_rgba_premultiplied() is that transient conversion, not a
//      second stored form.
//   6. ONLY THE MINIMUM CONVERSIONS FREEZE IN THIS SLICE: round-trip
//      with the display's 8-bit format (gltfx_rgba_from_srgb8/
//      gltfx_rgba_to_srgb8) and the premultiply helper
//      (gltfx_rgba_premultiplied). Parsing #rrggbb/rgb()/hsl()/
//      oklch() text is OUT OF SCOPE - it is its own slice in the RCSS
//      track (GODS_LAWS.md L-28) and returns a FALLIBLE
//      gltfx_rslt<gltfx_rgba>, unlike everything in this header,
//      none of which can fail.
//
// gltfx_rgba8 (design choice made HERE, NOT one of the six decisions
// above - the leader decided the canonical type is continuous/linear;
// nothing in his six decisions names how the 8-bit round-trip
// SIGNATURE should look, marked here as INFERENCE, GODS_LAWS.md
// L-27): CONTRACT.md SS6.2 caps a function at 4 parameters and tells
// the author to wrap the rest in a struct; a per-channel
// std::uint8_t r/g/b/a OUTPUT would need FIVE parameters (the input
// color plus four out-params) to convert TO the display's format. A
// packed integer would need only one, but hides byte order behind an
// implicit convention a consumer has to memorize or get wrong (the
// exact class of bug an explicit field name exists to prevent -
// CONTRACT.md SS12.12, POLA). gltfx_rgba8 is also, incidentally, the
// exact memory layout a GL_UNSIGNED_BYTE-normalized vertex attribute
// wants: four contiguous bytes, no gap.

namespace glintfx {

// Decisions 1+2+3+4+5, all at once: four floats, r/g/b/a in that
// order (matching the type's own name), alpha inside, linear light,
// straight (never premultiplied). Trivial aggregate, same shape as
// glintfx::version - no user-declared special member to default (Rule
// of Zero, CONTRACT.md SS2.2's "Rule of Five/Zero": the compiler-
// generated copy/move/destroy are already exactly correct for four
// plain floats with no owned resource; declaring them by hand would
// only add text nobody reads without changing behavior).
struct gltfx_rgba {
    float r;
    float g;
    float b;
    float a;
};

// Frozen footprint (GODS_LAWS.md L-19/L-26, decision 1): the unit is
// sizeof(float)/alignof(float), not a literal byte count, the same
// relative-unit technique err.hpp's own footprint assertion uses for
// pointers. The PER-FIELD order lock (r first, g second, ...) lives
// in color_test.cpp, following the offsetof() precedent
// version_test.cpp already established for glintfx::version - see
// that file's own comment for why sizeof() alone cannot catch a field
// reorder or a narrowed field.
static_assert(sizeof(gltfx_rgba) == 4 * sizeof(float),
              "gltfx_rgba footprint is frozen ABI, GODS_LAWS.md L-19/L-26 (CORE-COLOR)");
static_assert(alignof(gltfx_rgba) == alignof(float),
              "gltfx_rgba alignment is frozen ABI, GODS_LAWS.md L-19/L-26 (CORE-COLOR)");
static_assert(std::is_trivially_copyable_v<gltfx_rgba>,
              "gltfx_rgba must stay trivially copyable, CORE-COLOR (no owned resource)");

} // namespace glintfx
