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

// The display's 8-bit-per-channel format (design choice, see this
// file's header comment) - NOT linear, NOT the canonical type: this
// is what a monitor actually receives, and what #rrggbb/rgb() text
// spells out digit by digit. Exists ONLY at the boundary between
// gltfx_rgba and the outside world (a texture byte, a #rrggbb parse
// result, a vertex attribute upload); nothing INSIDE the library
// computes in this type.
struct gltfx_rgba8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

static_assert(sizeof(gltfx_rgba8) == 4 * sizeof(std::uint8_t),
              "gltfx_rgba8 footprint is frozen ABI, GODS_LAWS.md L-19/L-26 (CORE-COLOR)");
static_assert(alignof(gltfx_rgba8) == alignof(std::uint8_t),
              "gltfx_rgba8 alignment is frozen ABI, GODS_LAWS.md L-19/L-26 (CORE-COLOR)");
static_assert(std::is_trivially_copyable_v<gltfx_rgba8>,
              "gltfx_rgba8 must stay trivially copyable, CORE-COLOR (no owned resource)");

// Decision 6, first half: round-trip with the display's 8-bit format.
// gltfx_rgba_from_srgb8() applies the sRGB opto-electronic transfer
// function per COLOR channel (IEC 61966-2-1 - public, standard math,
// read under GODS_LAWS.md L-29, never a line of RmlUi/SDL3 code) to
// convert FROM the display-encoded byte INTO decision 3's linear
// light; alpha is copied straight through with NO transfer function,
// because alpha is a coverage fraction, not light, in this convention
// AND in CSS Color 4's (SS13.1/13.3 never apply a color-space
// transform to alpha, only to r/g/b).
//
// gltfx_rgba_to_srgb8() does the inverse and, being the one direction
// that can receive an out-of-[0,1] linear value (decision 1's
// headroom above white, or a negative, out-of-gamut component
// decision 1's own rationale accepts), CLAMPS before encoding - not
// an error, just the same lossy amputation any real display
// eventually performs; there is no fallible signature in this header
// (see decision 6's own text above - color_test.cpp proves the clamp
// live, not just in prose).
[[nodiscard]] GLINTFX_API gltfx_rgba gltfx_rgba_from_srgb8(gltfx_rgba8 encoded) noexcept;
[[nodiscard]] GLINTFX_API gltfx_rgba8 gltfx_rgba_to_srgb8(gltfx_rgba color) noexcept;

} // namespace glintfx
