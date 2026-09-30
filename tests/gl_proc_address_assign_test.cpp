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

using glintfx::render::gl_context_proc_address_fn;
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

// D-W7D-15: the form that carries a `void *user`, which is what the loader of a CONTEXT uses. The
// resolver receives exactly the pointer the caller passed, on every call, so it can tell WHICH
// context it is resolving for.
namespace {

struct fake_context {
    int calls = 0;
    const char *last_name = nullptr;
};

void *fake_context_resolver(void *user, const char *name) {
    auto *context = static_cast<fake_context *>(user);
    ++context->calls;
    context->last_name = name;
    if (std::strcmp(name, "glActiveTexture") == 0) {
        return reinterpret_cast<void *>(&fake_gl_active_texture);
    }
    return nullptr;
}

} // namespace

GLINTFX_TEST(the_user_pointer_reaches_the_resolver_and_a_known_name_resolves) {
    fake_context context;
    fake_gl_fn resolved = nullptr;
    const gl_context_proc_address_fn resolver = &fake_context_resolver;
    const bool ok = try_assign_gl_function_pointer(resolved, resolver, &context, "glActiveTexture");
    GLINTFX_CHECK(ok);
    GLINTFX_CHECK(resolved == &fake_gl_active_texture);
    GLINTFX_CHECK_EQ(context.calls, 1);
    GLINTFX_CHECK(std::strcmp(context.last_name, "glActiveTexture") == 0);
}

GLINTFX_TEST(with_a_user_pointer_an_unknown_name_sets_null_and_returns_false) {
    fake_context context;
    fake_gl_fn resolved = &fake_gl_active_texture; // deliberately non-null before the call
    const gl_context_proc_address_fn resolver = &fake_context_resolver;
    const bool ok = try_assign_gl_function_pointer(resolved, resolver, &context, "glNoSuchThing");
    GLINTFX_CHECK(!ok);
    GLINTFX_CHECK(resolved == nullptr);
    GLINTFX_CHECK_EQ(context.calls, 1);
}

GLINTFX_TEST(two_contexts_are_told_apart_by_their_user_pointer) {
    fake_context first;
    fake_context second;
    fake_gl_fn resolved = nullptr;
    const gl_context_proc_address_fn resolver = &fake_context_resolver;
    (void)try_assign_gl_function_pointer(resolved, resolver, &first, "glActiveTexture");
    (void)try_assign_gl_function_pointer(resolved, resolver, &second, "glActiveTexture");
    (void)try_assign_gl_function_pointer(resolved, resolver, &second, "glActiveTexture");
    GLINTFX_CHECK_EQ(first.calls, 1);
    GLINTFX_CHECK_EQ(second.calls, 2);
}
