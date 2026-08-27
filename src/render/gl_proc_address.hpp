// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// gl_proc_address.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-17, L-19,
// L-31).
//
// Single subject: the one mechanism the GENERATED GL 3.3 core loader
// (src/render's build-tree gl_functions.cpp, GODS_LAWS.md L-07 EXCECAO
// No 1) calls, once per function, to resolve one pointer. Hand-
// written, TDD'd (tests/gl_proc_address_assign_test.cpp), unlike the
// generated caller.
//
// gl_proc_address_fn is a PLAIN function pointer, not a concept-
// constrained template: this is a single bootstrap call made ~344
// times TOTAL, once at context-creation time, never in a per-frame hot
// path - GODS_LAWS.md L-19's "porta e concept resolvida em compile-
// time" targets a PORT with several operations, tested by swapping a
// fake adapter (window, input); a one-argument resolver function with
// zero state has no adapter to swap and no vtable to avoid (a raw
// function pointer already has neither). The concrete platform
// function this parameter is bound to - eglGetProcAddress on Linux,
// wglGetProcAddress on Windows (GODS_LAWS.md L-31) - is chosen by the
// CALLER (a later fatia, GL-CONTEXT), never by this file.
namespace glintfx::render {

using gl_proc_address_fn = void *(*)(const char *name);

// Resolves ONE function pointer by NAME through `get_proc_address`,
// writing the result into `out` (cast to Out's own pointer type) and
// returning whether the driver actually had it. This is the ENTIRE
// mechanism the generated loader repeats 344 times - see this file's
// own header comment for why it is a hand-written, tested helper
// instead of generated code: the SAME three lines, correctly written
// once, are more reliable than the same three lines emitted 344 times
// by a generator whose own text-assembly bugs would otherwise need
// discovering by reading generated output instead of a focused test.
//
// A null result from get_proc_address is the NORMAL failure case
// (docs/api-conventions.md's own methodology: an older driver, or a
// context created without every core 3.3 entry point, is not a
// programmer error) - `out` is set to nullptr in that case too, never
// left holding a stale or uninitialized pointer a caller might call
// through by mistake.
template <typename Out>
[[nodiscard]] bool try_assign_gl_function_pointer(Out &out, gl_proc_address_fn get_proc_address,
                                                  const char *name) noexcept {
    void *resolved = get_proc_address(name);
    out = reinterpret_cast<Out>(resolved);
    return resolved != nullptr;
}

} // namespace glintfx::render
