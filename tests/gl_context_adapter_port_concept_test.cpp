// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "fake/fake_gl_context_adapter.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/port/gl_context_adapter_port.hpp"

// gl_context_adapter_port_concept_test.cpp - GL-CONTEXT (docs/plano-
// w6b-placa-e-laco.md fatia 2b, D-W6b-1, GODS_LAWS.md L-19/L-20/L-40):
// proves platform::gl_context_adapter_port at the TYPE level, the same
// "positive control plus a negative control" shape display_backend_
// port_concept_test.cpp already establishes for its own sibling
// concept. Every check here is a static_assert - the "vermelho"
// GODS_LAWS.md L-20 requires is a COMPILE FAILURE: this file's own
// positive static_assert failed to compile before gl_context_adapter_
// port.hpp/fake_gl_context_adapter.hpp existed, and removing any ONE
// member from the negative control below (proved by hand while writing
// this file) makes the negative static_assert's own assumption -
// "missing exactly one member" - the thing actually under test.
//
// glintfx::test::fake_gl_context_adapter (tests/fake/) is the POSITIVE
// control - it satisfies every member this concept asks for. The
// NEGATIVE control is a small LOCAL type missing exactly gpu() (the
// last member the concept's own requires-clause names) - the same
// "local, deliberately narrow" pattern display_backend_port_concept_
// test.cpp's own local_backend_with_pump established one file over,
// chosen over reusing an unrelated existing fixture because nothing
// else in this tree already satisfies eight of nine members by
// accident.

namespace {

// Deliberately missing gpu() - every other member of gl_context_
// adapter_port is present.
class local_gl_adapter_missing_gpu {
  public:
    local_gl_adapter_missing_gpu() noexcept = default;
    local_gl_adapter_missing_gpu(local_gl_adapter_missing_gpu &&) noexcept = default;
    local_gl_adapter_missing_gpu &operator=(local_gl_adapter_missing_gpu &&) noexcept = default;

    void close() noexcept {}
    [[nodiscard]] bool is_open() const noexcept { return false; }

    [[nodiscard]] glintfx::gltfx_rslt<void> make_current() noexcept {
        return glintfx::gltfx_rslt<void>::ok();
    }

    [[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swap_buffers() noexcept {
        return glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>::ok(
            glintfx::gltfx_present_outcome::presented);
    }

    [[nodiscard]] void *proc_address(std::string_view /*name*/) const noexcept { return nullptr; }

    [[nodiscard]] glintfx::gltfx_rslt<void>
    apply_option(glintfx::gltfx_gfx_option_entry /*entry*/) noexcept {
        return glintfx::gltfx_rslt<void>::ok();
    }

    [[nodiscard]] glintfx::gltfx_gfx_option_support
    option_support(glintfx::gltfx_gfx_option /*id*/) const noexcept {
        return glintfx::gltfx_gfx_option_support::supported;
    }
};

} // namespace

// Positive control: fake_gl_context_adapter satisfies every member.
static_assert(glintfx::platform::gl_context_adapter_port<glintfx::test::fake_gl_context_adapter>,
              "fake_gl_context_adapter must satisfy gl_context_adapter_port");

// Negative control: missing exactly gpu() must NOT satisfy the port -
// a concept that accepts it anyway is the L-40 "aceita qualquer coisa"
// defect.
static_assert(!glintfx::platform::gl_context_adapter_port<local_gl_adapter_missing_gpu>,
              "local_gl_adapter_missing_gpu has no gpu() and must NOT satisfy "
              "gl_context_adapter_port");

GLINTFX_TEST(fake_gl_context_adapter_satisfies_the_port) {
    GLINTFX_CHECK(
        (glintfx::platform::gl_context_adapter_port<glintfx::test::fake_gl_context_adapter>));
}

GLINTFX_TEST(local_gl_adapter_missing_gpu_does_not_satisfy_the_port) {
    GLINTFX_CHECK(!(glintfx::platform::gl_context_adapter_port<local_gl_adapter_missing_gpu>));
}
