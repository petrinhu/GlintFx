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
//
// NEVER DEFINED UNDER MSVC (the #ifndef _MSC_VER guard below, CORE-
// LOG-CI, run 34352638143, jobs "Windows - compartilhado"/"Windows -
// estatico"/"Windows - Debug"): the shim used to be unconditional, and
// it broke Microsoft's OWN standard library headers, not glintfx's -
// header_hygiene_test.cpp includes this shim before <cstdint>, and
// MSVC's own <stdint.h>/<cstdint>/internal STL headers use `#pragma
// warning(push)` pervasively. Once `warning` is a macro that expands
// to an EXPRESSION, `#pragma warning(push)` stops parsing as a
// pragma at all - the preprocessor substitutes inside it, and the
// result is syntax garbage (measured: warning C4081 "expected
// identifier; found '('" at every one of stdint.h:15, stdint.h:16,
// stdint.h:136, cstdint:14, cstdint:15, cstdint:56 and an internal STL
// header, then error C2220 because -WX turns that warning into a hard
// failure). This is not a glintfx header breaking; it's Microsoft's
// own headers breaking, because this shim never gave them a way to
// opt out.
//
// GUARD CRITERION IS THE COMPILER (_MSC_VER), NOT THE PLATFORM
// (_WIN32): the mechanism is `#pragma warning(...)` being meaningful
// to the COMPILER's own preprocessor and used throughout the
// COMPILER's own shipped headers - a property of MSVC (and of
// clang-cl, which also defines _MSC_VER in its default MSVC-
// compatible mode, and also honors #pragma warning for the same
// reason) as a toolchain, not of Windows as an operating system. This
// project's actual Windows CI leg builds with real cl.exe (vcvarsall.
// bat x64, .github/workflows/ci.yml job `windows`) - _MSC_VER is what
// that toolchain defines, and is the precise, measured condition
// under which <cstdint>'s own #pragma warning lines exist to corrupt;
// a hypothetical Windows+MinGW-GCC toolchain would define _WIN32 but
// not _MSC_VER, and would not carry this specific hazard (MinGW's own
// headers do not use #pragma warning(push) the way MSVC's do), so
// guarding by _WIN32 would be broader than the measured problem.
//
// GODS_LAWS.md L-04 CONSEQUENCE, STATED HERE RATHER THAN LEFT SILENT:
// this shim, and the `warning` collision class it exercises, now runs
// on four of the five CI targets (Fedora, Ubuntu, Arch, CachyOS), not
// five - MSVC's own headers make the fifth impossible to cover THIS
// way (see above). The reduction is recorded in tests/parity_
// exceptions.txt's own free-text section for exactly this shape of
// finding (a real, permanent coverage reduction inside a test that
// still exists, and still passes, on every platform - not a missing
// ctest name, so it does not get one of that file's four-field
// lines).
#ifndef _MSC_VER

#ifndef GAWK

#ifndef warning
#define warning (hostile_gawk_api->api_warning)
#endif

#endif // !GAWK

#endif // !_MSC_VER
