// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// hostile_gawk_macros_shim.hpp - CORE-LOG-CI DEFEITO 3 (sibling of
// hostile_win32_macros_shim.hpp, same directory, same reason: GODS_
// LAWS.md L-04, "comportamento igual em todo sistema").
//
// WHY A SHIM INSTEAD OF ONLY THE REAL gawkapi.h: the collision that
// broke this project's own CI (run 34329543846, jobs "Arch -
// compartilhado"/"Arch - estatico"/"CachyOS - compartilhado"/"CachyOS
// - estatico") only fired because THOSE two distros happen to ship the
// `gawk` package (Fedora/Ubuntu do not) - check_public_name_collision.
// py already scans whatever gawkapi.h a given machine's REAL compiler
// search path happens to contain, but that makes the finding
// PLATFORM-DEPENDENT BY ACCIDENT, not by design: a machine without
// `gawk` installed would never exercise this collision class again,
// even after a future name reintroduced it. This shim makes the SAME
// hostile shape unconditional on all five platforms, the same role
// hostile_win32_macros_shim.hpp already plays for `near`/`far` on the
// four non-Windows targets.
//
// THE FORM MATTERS (measured, cgit.git.savannah.gnu.org/cgit/gawk.git/
// plain/gawkapi.h:888): gawk's own `warning` is an OBJECT-LIKE macro
// that expands to an EXPRESSION (`api->api_warning`), never to a bare
// replacement identifier. A shim written as `#define warning
// gltfx_something_else` would not reproduce the real hazard - it
// would silently RENAME the enumerator wherever `warning` appears, and
// `severity.hpp`'s own `enum class` would keep compiling clean,
// proving nothing. The real macro turns `warn = 400,`-shaped source
// into a syntax error the moment it substitutes an EXPRESSION where an
// enumerator declaration expects a bare identifier - this shim
// reproduces exactly that shape, unconditionally guarded (`#ifndef
// GAWK`, gawkapi.h's own guard) the same way the real header is.
//
// DELIBERATELY NARROW (GODS_LAWS.md L-17 "regra de 3", same discipline
// hostile_win32_macros_shim.hpp's own header comment already states):
// only `warning`, the ONE name this project's own severity vocabulary
// ever collided with for real. gawkapi.h carries ~55 more lowercase
// names (`fatal`, `nonfatal`, `lintwarn`, `emalloc`, ...); none of them
// is glintfx vocabulary today, so none is reproduced here - this shim
// grows the moment a real collision, or a real risk of one, is found,
// never speculatively.
#ifndef GAWK

#ifndef warning
#define warning (hostile_gawk_api->api_warning)
#endif

#endif // !GAWK
