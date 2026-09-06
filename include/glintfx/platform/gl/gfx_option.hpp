// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// platform/gl/gfx_option.hpp - C-OPT (docs/plano-w6b-placa-e-laco.md
// fatia 2a, D-W6b-16/17, GODS_LAWS.md L-19/L-26): the MECHANISM the
// leader's own three orders of 06/09/2026 resolve into - "quero o
// maior numero possivel de opcoes graficas" against "biblioteca para
// distribuicao, promessa minima" (this project's own LEI ZERO). The
// two never had to fight: what freezes here, forever, is the SHAPE of
// asking for an option and of asking whether one exists; the OPTIONS
// THEMSELVES are rows of an APPEND-ONLY table (gfx_option_registry.hpp,
// this fatia) that grows without ever touching this header again.
//
// THREE THINGS FROZEN HERE, NO MORE (mirrors gfss/property.hpp's own
// "what this header freezes" discipline, same reasoning: a registry
// header names its OWN contract explicitly so the next editor does not
// have to reconstruct it from the diff):
//
//   1. gltfx_gfx_option - the id. std::uint16_t, ONE explicit numeric
//      value per enumerator, NEVER reordered, NEVER reused once
//      shipped (the same append-only contract gltfx_err_code already
//      carries, GODS_LAWS.md L-26). tests/tools/check_gfx_option_ids.py
//      is the CI gate that reproves a reorder or a reused number - see
//      that script's own header for the ledger it keeps once the first
//      option is ever retired.
//   2. gltfx_gfx_option_entry - the ONE pair (id, value) every opening
//      list and every gltfx_gl_context::set_option() call is made of
//      (D-W6b-17: the descriptor freezes to exactly {options, count},
//      never a field per option). `value` is always a plain
//      std::int64_t: `toggle` reads 0/1, `choice` reads the chosen
//      value's own numeric id, `integer` reads the number itself -
//      never floating point (the same "no floating point where an
//      exact edge matters" decision this project's own clock already
//      made, 27/08/2026).
//   3. gltfx_gfx_option_info - the STATIC facts a consumer can read
//      about an option WITHOUT any open context: its name (the DATA
//      contract, item 4 below), its kind, WHEN it may be set, and the
//      closed numeric range a value has to sit inside.
//
// WHAT THIS HEADER DELIBERATELY DOES NOT FREEZE: which options THIS
// SYSTEM honors right now (gltfx_gl_context::option_support(), the
// fatia 2b handle) - min_value/max_value/default_value here describe
// the OPTION as this library's v1 knows it, not what a given driver on
// a given machine accepts. Nor does it freeze what each option actually
// DOES (that lives in the two adapters, fatias 3/4, and in the portable
// matrix, fatia 9) - this header only lets a consumer DISCOVER what
// exists and validate a value's SHAPE before ever touching a context.
//
// 4. THE NAME IS A DATA CONTRACT (docs/api-conventions.md's own third
// register, alongside a C++ identifier and a sheet spelling):
// gltfx_gfx_option_info::name is exactly the string a consumer's own
// saved settings file is expected to write and read back - "vsync",
// "frame_rate_cap", and so on (docs/plano-w6b-placa-e-laco.md sec.
// 11.2's own "nome (dado, L-26)" column). Once an option ships, this
// string never changes meaning; a name retired for one reason is never
// handed to a different option later (the same discipline gltfx_err_
// code's own enumerator names already carry).
//
// gltfx_gfx_option_by_name() IS FALLIBLE ON PURPOSE (docs/api-
// conventions.md R1): a consumer's saved settings file can carry a
// name this build's table has never heard of (an older glintfx, or a
// typo) - gltfx_err_code::not_found is exactly the code that answers
// "I looked, and it is not here" without inventing a bespoke code for
// this one lookup.
//
// gltfx_gfx_option_count()/gltfx_gfx_option_at()/gltfx_gfx_option_
// describe() ARE NEVER-FAILING, TOTAL FUNCTIONS instead (docs/api-
// conventions.md R4's "never undefined behavior" convention, the SAME
// shape gltfx_gfss_property_name() already uses for an id outside its
// own table): an index past gltfx_gfx_option_count(), or a `id` a
// future glintfx build knows and this one does not, degrades to a
// default-constructed gltfx_gfx_option_info - empty name, `toggle`
// kind, `read_only` when (the one `when` a consumer can never
// mistake for "I may write this") - never a crash, never a thrown
// exception across this boundary.
//
// gltfx_gfx_option_describe() IS NOT NAMED gltfx_gfx_option_info()
// (CTO correction, docs/plano-w6b-placa-e-laco.md, team-lead briefing
// of 06/09/2026): a free FUNCTION and the STRUCT gltfx_gfx_option_info
// sharing one identifier would make the function HIDE the type in
// C++'s own unqualified name lookup - every consumer wanting to name
// the struct itself (a return type, a local variable) would have to
// write `struct gltfx_gfx_option_info` to get past the function, and
// tests/tools/check_public_name_collision.py's own function-vs-type
// extraction reads exactly this shape as a name worth flagging. The
// TYPE keeps the name the plan's own decision table names it by
// (gltfx_gfx_option_info); the FUNCTION that returns one by id is
// named for what it does - it *describes* an option - never the noun
// the type already owns.

namespace glintfx {

// APPEND-ONLY, FOREVER (item 1 above). v1 ships exactly the eight ids
// docs/plano-w6b-placa-e-laco.md sec. 11.2 ratifies, in the SAME order
// as that table's own rows - tests/tools/check_gfx_option_ids.py
// reproves a build where this enum's own declaration order stops
// matching its own numeric values. std::uint16_t is deliberately wider
// than today's eight members need (clang-tidy's own performance-enum-
// size would suggest std::uint8_t) - the SAME reasoning gfss/property.
// hpp's own gltfx_gfss_property already documents: this is the ONE id
// in this registry that is APPEND-ONLY FOREVER and PUBLISHED as a
// contract, for a consumer base that is public and unknown (LEI ZERO),
// and std::uint8_t caps at 256 members total, ever.
enum class gltfx_gfx_option : std::uint16_t { // NOLINT(performance-enum-size) reason: see the
                                              // paragraph above
    vsync = 0,
    frame_rate_cap = 1,
    gpu_preference = 2,
    msaa_samples = 3,
    srgb_framebuffer = 4,
    preset = 5,
    auto_choice_reason = 6,
    power_source = 7,
};

// The three shapes a value can take (D-W6b-16 (2)): `toggle` reads
// 0/1, `choice` reads the numeric id of the chosen value (documented
// per option, e.g. vsync's 0=off/1=on/2=adaptive), `integer` reads a
// plain number (e.g. frame_rate_cap's Hz). None of the three is ever
// floating point. std::uint8_t: unlike gltfx_gfx_option above, this is
// a closed, structural vocabulary (a value's SHAPE), never appended to
// the way the option list itself grows.
enum class gltfx_gfx_option_kind : std::uint8_t {
    toggle,
    integer,
    choice,
};

// WHEN an option may be read or written (D-W6b-16 (3)): `open_only`
// belongs in the list a context opens with and never changes for the
// life of that context; `live` may be set at any time after opening,
// through set_option() (the handle fatia 2b freezes); `read_only` is
// never accepted from a consumer in either place - it is written by
// the library itself and only ever read back. std::uint8_t: same
// closed, structural vocabulary as gltfx_gfx_option_kind above.
enum class gltfx_gfx_option_when : std::uint8_t {
    open_only,
    live,
    read_only,
};

// What a CONCRETE system (a real context, fatia 2b) answers when asked
// whether it honors a given option - distinct from gltfx_gfx_option_
// info below, which answers what the LIBRARY knows about the option in
// the abstract, with no system in the picture yet. std::uint8_t: same
// closed, structural vocabulary as gltfx_gfx_option_kind above.
enum class gltfx_gfx_option_support : std::uint8_t {
    unsupported_here,
    supported,
    read_only_here,
};

// The ONE pair every opening list and every set_option() call is made
// of (item 2 above). Default-constructed to `{vsync, 0}` - an
// arbitrary, harmless placeholder (vsync's own id 0 sorts first), not
// a claim that omitting `id` means "vsync".
struct gltfx_gfx_option_entry {
    gltfx_gfx_option id = gltfx_gfx_option::vsync;
    std::int64_t value = 0;
};

// The static facts the LIBRARY knows about one option, with no system
// in the picture (item 3 above). `min_value`/`max_value` are spelled
// out in full - never the bare `min`/`max` a hostile `<windows.h>`
// without `NOMINMAX` turns into function-like macros (docs/api-
// conventions.md R6, the same collision class version.hpp's own major/
// minor fields were already renamed to avoid).
struct gltfx_gfx_option_info {
    gltfx_gfx_option id = gltfx_gfx_option::vsync;
    std::string_view name;
    gltfx_gfx_option_kind kind = gltfx_gfx_option_kind::toggle;
    gltfx_gfx_option_when when = gltfx_gfx_option_when::read_only;
    std::int64_t min_value = 0;
    std::int64_t max_value = 0;
    std::int64_t default_value = 0;
};

// How many options THIS BUILD's registry knows - never a literal a
// consumer hardcodes, since the table only ever grows (D-W6b-21).
[[nodiscard]] GLINTFX_API std::size_t gltfx_gfx_option_count() noexcept;

// The option at `index` (0 <= index < gltfx_gfx_option_count()), in the
// registry's own id order. `index` outside that range degrades to a
// default-constructed gltfx_gfx_option_info (docs/api-conventions.md
// R4) - never undefined behavior, never a thrown exception.
[[nodiscard]] GLINTFX_API gltfx_gfx_option_info gltfx_gfx_option_at(std::size_t index) noexcept;

// The static facts about `id`, with no system in the picture. An `id`
// outside this build's own table (produced, for example, by a newer
// glintfx crossing the .so boundary into an older one) degrades the
// same way gltfx_gfx_option_at() does above - see this header's own
// top comment for why this function is not named gltfx_gfx_option_
// info() despite returning one.
[[nodiscard]] GLINTFX_API gltfx_gfx_option_info
gltfx_gfx_option_describe(gltfx_gfx_option id) noexcept;

// Resolves `name` (the DATA contract, item 4 above) back to its id -
// fallible on purpose (see this header's own top comment): a name this
// build's table does not recognize returns gltfx_err_code::not_found,
// never a fabricated id.
[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_gfx_option>
gltfx_gfx_option_by_name(std::string_view name) noexcept;

} // namespace glintfx
