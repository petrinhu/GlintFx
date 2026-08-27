// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstring>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include "render/gl_proc_address.hpp"

// gl_proc_address_assign_test.cpp - GL-LOADER (TODO.md, GODS_LAWS.md
// L-20). Proves the ONE hand-written mechanism the generated GL 3.3
// core loader calls 344 times (src/render/gl_proc_address.hpp's own
// header comment explains why this is hand-written and tested instead
// of generated).
//
// This is a declared DOWNGRADE (GODS_LAWS.md L-09/TESTES.md): there is
// no real GL context to load against yet (GL-CONTEXT has not landed),
// so this proves the RESOLUTION MECHANISM against a fake
// gl_proc_address_fn, never a real eglGetProcAddress/wglGetProcAddress
// call. What it does NOT prove: that a real driver's proc-address
// function, called through this same mechanism, resolves a real
// symbol correctly - that is GL-CONTEXT's own integration test, in a
// later fatia, run inside the isolated Wayland container (GODS_LAWS.md
// L-09).

using glintfx::render::gl_proc_address_fn;
using glintfx::render::try_assign_gl_function_pointer;

namespace {

using fake_gl_fn = void (*)(int);
void fake_gl_active_texture(int) {}

void *fake_get_proc_address_known(const char *name) {
    if (std::strcmp(name, "glActiveTexture") == 0) {
        return reinterpret_cast<void *>(&fake_gl_active_texture);
    }
    return nullptr;
}

void *fake_get_proc_address_none(const char * /*name*/) { return nullptr; }

} // namespace

GLINTFX_TEST(resolves_a_known_name_and_returns_true) {
    fake_gl_fn resolved = nullptr;
    const gl_proc_address_fn getter = &fake_get_proc_address_known;
    const bool ok = try_assign_gl_function_pointer(resolved, getter, "glActiveTexture");
    GLINTFX_CHECK(ok);
    GLINTFX_CHECK(resolved == &fake_gl_active_texture);
}

GLINTFX_TEST(unknown_name_sets_the_pointer_to_null_and_returns_false) {
    fake_gl_fn resolved = &fake_gl_active_texture; // deliberately non-null before the call
    const gl_proc_address_fn getter = &fake_get_proc_address_known;
    const bool ok = try_assign_gl_function_pointer(resolved, getter, "glSomeFutureFunction");
    GLINTFX_CHECK(!ok);
    GLINTFX_CHECK(resolved == nullptr);
}

GLINTFX_TEST(a_getter_that_resolves_nothing_fails_every_name) {
    fake_gl_fn resolved = nullptr;
    const gl_proc_address_fn getter = &fake_get_proc_address_none;
    const bool ok = try_assign_gl_function_pointer(resolved, getter, "glActiveTexture");
    GLINTFX_CHECK(!ok);
    GLINTFX_CHECK(resolved == nullptr);
}
