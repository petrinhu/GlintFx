// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

// numeric_lexeme.hpp - shared plumbing for GFSS-COLOR-PARSE and
// GFSS-VALUE (TODO.md, GODS_LAWS.md L-17/L-27/L-40): answers exactly
// one question - "what double does a <number-token>/<percentage-token>
// lexeme this project's OWN tokenizer produced actually mean?" -
// distinct from every file that DECIDES what a value's NATURE is
// (color_parse.cpp's own byte-quantization step, value_parse.cpp's own
// gltfx_gfss_value_kind dispatch): this file only converts text to a
// double, saturating on overflow, never rejecting it and never picking
// a nature for it.
//
// MOVED HERE FROM color_parse.cpp ON GFSS-VALUE (GODS_LAWS.md L-17,
// L-27 marked as this fatia's own INFERENCE, not a fact the service
// order named by file): decode_number_lexeme()/decode_percentage_
// lexeme() were born inside GFSS-COLOR-PARSE's own anonymous namespace
// (color_parse.cpp), private to that one translation unit - correct
// when this project had exactly one consumer of "<number-token> lexeme
// to double". GFSS-VALUE needs the SAME conversion for its own
// <number>/<integer>/<length> natures (a bare number and a dimension's
// own magnitude are the identical CSS Syntax Module Level 3 4.3.12
// grammar color's rgb()/hsl() arguments already decode), and this
// project already measured what duplicating this exact logic costs: TWO
// rounds of adversarial review (this file's own .cpp, "SECOND
// CORRECTION"/"CORRECTED HERE" comments) were needed to make the
// overflow-saturation path correct at all. Re-deriving that a second
// time in a new file would re-open the same review cost for no new
// behavior - GODS_LAWS.md L-17's "assunto novo vira modulo proprio"
// applied to a subject that was ALREADY its own subject, just not yet
// given its own file. THIS IS A PURE MOVE: every byte of logic below is
// unchanged from color_parse.cpp's own commit history; the ONLY
// difference is that the two entry points now have external (but
// hidden, non-GLINTFX_API) linkage instead of anonymous-namespace
// linkage, so a second translation unit (value_parse.cpp) can declare
// and call them - color_parse.cpp's own existing test suite
// (gfss_color_parse_test.cpp) is this move's regression net: it stays
// green, unchanged, because the two functions' OWN behavior never
// changed, only where they live.
//
// STILL PRIVATE, STILL NOT GLINTFX_API (GODS_LAWS.md L-19: "o header
// nasce interno"): this header lives in src/gfss/, not include/glintfx/
// - the SAME reason color_parse.hpp/named_colors.hpp/selector_parse.hpp
// are not promoted yet. A test that needs to call these functions
// directly compiles this .cpp a second time into its own executable
// (tests/CMakeLists.txt's own established technique for every
// not-yet-GLINTFX_API .cpp in this track), never links the hidden copy
// inside glintfx_library's own .so.

namespace glintfx::style::detail {

// Converts a <number-token>/<percentage-token> BODY (no unit, no '%'
// sign - a caller with a percentage lexeme strips the trailing '%'
// first, or calls decode_percentage_lexeme() below instead) to a
// double, saturating to +-std::numeric_limits<double>::max()/lowest()
// on overflow rather than fabricating an arbitrary value or crashing
// the consumer's process on hostile external text (ESCOPO.md SS2,
// CORE-ERROR decision 1: "the library never aborts the consumer's
// process", the SAME principle that governs out-of-memory extended to
// hostile input). See numeric_lexeme.cpp's own header comment for the
// full derivation - it is delicate enough that a two-line summary here
// would not save a reader from having to read the real thing before
// touching it.
[[nodiscard]] double decode_number_lexeme(std::string_view lexeme) noexcept;

// Same conversion, for a <percentage-token>'s OWN lexeme (WITH the
// trailing '%' still attached - this function strips it, then defers
// to decode_number_lexeme() above). Returns the RAW percentage
// magnitude a caller would read off the token text - "50%" decodes to
// 50.0, never 0.5 - the SAME convention CSS Syntax Module Level 3's own
// "a percentage-token's value is set to the value of the number
// component" uses: multiplying by 0.01 is a RESOLUTION step (GFSS-
// RESOLVE, TODO.md, ESCOPO.md SS4's own declared "% atravessa
// preservado" boundary), never something this decode step does on its
// own.
[[nodiscard]] double decode_percentage_lexeme(std::string_view lexeme) noexcept;

} // namespace glintfx::style::detail
