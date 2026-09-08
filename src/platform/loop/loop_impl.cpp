// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/loop/loop_impl.hpp"

#include <new>

#include <glintfx/core/err_code.hpp>

// loop_impl.cpp - LOOP-RUN fatia 6b (docs/plano-w6b-fatias-6-8.md sec.
// 8.3, F17, GODS_LAWS.md L-20/L-22/L-27): allocate_loop_impl()'s only
// definition - see loop_impl.hpp's own header comment on that function
// for why it is its own atom instead of being inlined directly inside
// gltfx_loop::open() (loop_facade.cpp).

namespace glintfx {

gltfx_rslt<loop_impl *> allocate_loop_impl() noexcept {
    // WIN-DEBUG-CTORALLOC (this header's own top comment, and gfx_
    // open_only_fixation.cpp's own header comment, the sibling finding
    // this exact fatia's own briefing named explicitly, tests/gfx_
    // open_only_fixation_test.cpp lines 87-150): a default-constructed
    // loop_impl carries no std::vector/std::string/std::function
    // member - three raw pointers, a frame_cap_schedule that is itself
    // two POD fields (bool, gltfx_time_point), a gltfx_time_point, an
    // enum and a std::uint64_t - so, unlike gl_context_impl's own
    // current_values, there is no MSVC-Debug iterator-proxy allocation
    // this construction could hide. The try/catch below is kept
    // anyway, for the SAME reason gl_context_facade.cpp's own open()
    // keeps one around its own `new (std::nothrow) gl_context_impl{}`
    // (this project's own git history, commit 4dc010d) - symmetry with
    // the one other call site in this project that allocates a
    // facade's own opaque impl this exact way, so a FUTURE field added
    // to loop_impl that DOES allocate never silently reopens the exact
    // defect that commit closed: a noexcept function letting
    // std::bad_alloc escape calls std::terminate() and takes the
    // consumer's whole process down with it (GODS_LAWS.md L-22/docs/
    // api-conventions.md R3).
    try {
        loop_impl *impl = new (std::nothrow) loop_impl{};
        if (impl == nullptr) {
            return gltfx_rslt<loop_impl *>::err(gltfx_err(gltfx_err_code::out_of_memory));
        }
        return gltfx_rslt<loop_impl *>::ok(impl);
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<loop_impl *>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
}

} // namespace glintfx
