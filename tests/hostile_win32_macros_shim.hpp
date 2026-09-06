// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// hostile_win32_macros_shim.hpp - WL-WINDOW-HANDLE (docs/plano-w6a-
// janela.md fatia 6, "perna do shim hostil": novo vermelho de estreia,
// cópia do header com campo `near` reprova) - the piece fatia 6 named
// and never delivered (confirmed by grep before this fatia started:
// this file did not exist anywhere in the tree).
//
// WHY A SHIM INSTEAD OF ONLY THE REAL <windows.h> (GODS_LAWS.md L-04,
// "comportamento igual em todo sistema"): tests/header_hygiene_test.cpp
// already includes the REAL <windows.h> under `#ifdef _WIN32` and the
// REAL <sys/sysmacros.h> under `#ifdef __linux__` - but <windows.h>
// only EXISTS on Windows, so a hygiene check that only ever runs
// against the genuine header is asymmetric by construction: four of
// five CI targets never exercise this specific collision class at all,
// and a public header that introduces one would only ever be caught on
// the one Windows leg, potentially long after it landed. This shim
// defines the SPECIFIC hostile macro <windef.h> is documented to still
// carry for MS-DOS source compatibility (learn.microsoft.com/windows/
// win32/winprog/windows-data-types and <windef.h> itself: `near`/`far`,
// the legacy segment-qualifier keywords, #define'd to nothing) so the
// SAME collision check runs on all five platforms, not only the one
// that has the real header.
//
// NEVER DEFINED ON AN ACTUAL WINDOWS BUILD (the #ifndef _WIN32 guard
// below): <windows.h> already defines both macros for real there -
// this shim would either be a harmless redefinition or, worse, a
// SECOND definition that could silently drift from the real one. The
// genuine Windows CI leg keeps including the real <windows.h>
// (header_hygiene_test.cpp's own existing #ifdef _WIN32 block); this
// shim only fills the gap on the other four targets.
//
// DELIBERATELY NARROW: only the two macros this fatia's own red-proof
// names (`near`, `far`) - GODS_LAWS.md L-17 "regra de 3", the same
// enumerate-the-closed-space discipline this session's own README
// volatile-numbers gate already applied: a field or parameter named
// literally `near` or `far` anywhere in glintfx's own public headers
// is the exact shape a real Win32 consumer's <windows.h> would
// silently break on. Other <windef.h>/<winnt.h> macro names (IN, OUT,
// ERROR, DELETE, ABSOLUTE, RELATIVE) all expand to real, non-empty
// values on Windows - a MUCH noisier, differently-shaped hazard this
// narrow shim does not attempt to reproduce, and no name from that
// list is documented here as a risk this project's own vocabulary has
// actually run into. Grows the same way header_hygiene_test.cpp's own
// COVERAGE CONTRACT paragraph already grows: a new hostile name is
// added here the moment a real collision, or a real risk of one, is
// found - never speculatively.
#ifndef _WIN32

#ifndef near
#define near
#endif

#ifndef far
#define far
#endif

#endif // !_WIN32
