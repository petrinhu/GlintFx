// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// hostile_attr_error_macros_shim.hpp - RSLT-ERR-RENAME (docs/api-
// conventions.md R6, "Correction to this rule's own text" finding,
// 09/09/2026): sibling of hostile_win32_macros_shim.hpp and
// hostile_gawk_macros_shim.hpp, same directory, same reason (GODS_
// LAWS.md L-04, "comportamento igual em todo sistema").
//
// THE REAL COLLISION THIS REPRODUCES: libattr's attr/error_context.h
// (Linux, `libattr-devel`) declares a FUNCTION-LIKE macro, roughly
// (line-continuations elided here so this comment itself never ends a
// line in `\`, which a real backslash-newline would make the compiler
// read as CONTINUING into the next comment line - not the hazard this
// file exists to reproduce):
//   #define error(context, status, errnum, fmt, args...)
//       error_at_line((context), (status), (errnum), __FILE__,
//                     __LINE__, (fmt), ## args)
// - it only fires when the token `error` is immediately followed by
// `(` (unlike gawk's OBJECT-like `warning`, see that shim's own header
// comment for the contrast). It sits behind an opt-in guard
// (`ERROR_CONTEXT_MACROS`) ordinary inclusion never defines, and
// `libattr-devel` is not installed on every one of this project's five
// targets - so, exactly like the gawk shim, reproduce the hostile
// SHAPE unconditionally here instead of depending on whichever machine
// happens to have the real header AND that opt-in guard both set.
//
// THE FORM MATTERS: before this fatia, `gltfx_rslt<T>::error()` was a
// call expression - `r.error()` - and a function-like macro pattern-
// matches on `error` immediately followed by `(`, the same shape
// `r.error()` has. A shim that instead defined an OBJECT-like
// `#define error <something>` would not reproduce the real hazard
// (docs/api-conventions.md R6's own "Correction" finding names this
// exact distinction). This is why `gltfx_rslt<T>::err()` - the name
// this fatia renamed the accessor TO - survives inclusion of this
// shim: `err` is a different token than `error`, so the macro never
// matches it at all, macro or no macro.
//
// NEVER CALLED: the stub only exists so a stray expansion (a future
// regression that reintroduces a bare `error(` call site under this
// shim) still parses as a value the compiler can name in a diagnostic,
// instead of a bare undeclared-identifier error that would point at
// the wrong line.
//
// DELIBERATELY NARROW (GODS_LAWS.md L-17 "regra de 3", same discipline
// the win32/gawk shims already state): only `error`, the one name this
// fatia's own rename was about. Grows the same way those two shims
// already grow: a new hostile name is added here the moment a real
// collision, or a real risk of one, is found - never speculatively.
#ifndef error
#define error(ctx, ...) (hostile_attr_error_context_stub((ctx)__VA_OPT__(, ) __VA_ARGS__))
#endif

inline int hostile_attr_error_context_stub(...) noexcept { return 0; }
