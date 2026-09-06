// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_state.hpp"

// fake_gl_context_adapter.hpp - GL-CONTEXT (docs/plano-w6b-placa-e-
// laco.md fatia 2b, GODS_LAWS.md L-19/L-20): the TEST-ONLY adapter that
// proves platform::gl_context_adapter_port is genuinely satisfiable -
// the same benefit fake_display_adapter.hpp already gives display_
// connection_port one directory over. Never part of glintfx_library's
// own object set - lives under tests/, same reasoning that file's own
// header comment already gives.
//
// open() IS NOT PART OF THIS FIXTURE'S OWN PUBLIC SHAPE ON PURPOSE:
// gl_context_adapter_port deliberately excludes open() (that header's
// own comment explains why - a different concrete window-adapter
// parameter type per platform), so this fixture has nothing to
// implement for it either; a future facade-level test (once fatias 3/4
// exist) is free to add a TEST-ONLY open()-shaped method here without
// this file ever needing to satisfy the port concept through it.
//
// STATE, NOT STATIC COUNTERS: unlike fake_display_adapter.hpp (whose
// own header comment explains why IT needs static counters - display_
// connection<A> default-constructs A with no way to inject a per-
// instance log), nothing here prevents ordinary per-instance state:
// this fixture is never wrapped by a generic template the way
// display_connection<A> wraps fake_display_adapter, so a caller that
// constructs one directly can read its own member state back after
// use.
namespace glintfx::test {

class fake_gl_context_adapter {
  public:
    fake_gl_context_adapter() noexcept = default;

    // PINNED, NEVER MOVABLE (FACADE-PIN, docs/plano-conserto-fachadas-
    // uaf.md sec. 6/7.2): gl_context_adapter_port now requires pinned_
    // adapter<A> instead of std::movable<A> (platform/port/pinned_
    // adapter.hpp) - this fixture has to satisfy the SAME contract the
    // real wayland_egl_context_adapter/win32_gl_context_adapter now do,
    // or gl_context_adapter_port_concept_test.cpp's own positive
    // control would stop compiling.
    fake_gl_context_adapter(const fake_gl_context_adapter &) = delete;
    fake_gl_context_adapter &operator=(const fake_gl_context_adapter &) = delete;
    fake_gl_context_adapter(fake_gl_context_adapter &&) = delete;
    fake_gl_context_adapter &operator=(fake_gl_context_adapter &&) = delete;

    ~fake_gl_context_adapter() = default;

    // Test-only, not part of gl_context_adapter_port (see this file's
    // own header comment above): lets a case simulate "the adapter
    // succeeded" without going through a real platform-specific open().
    void simulate_open() noexcept { m_open = true; }

    void close() noexcept { m_open = false; }
    [[nodiscard]] bool is_open() const noexcept { return m_open; }

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

    // Test-only setter, not part of the port: lets a case simulate
    // "this driver told us about a specific GPU" (gpu_kind_state.hpp's
    // own learn()).
    void learn_gpu(glintfx::gltfx_gpu_kind kind, std::string_view name) noexcept {
        m_gpu.learn(kind, name);
    }

    [[nodiscard]] glintfx::gltfx_gpu_info gpu() const noexcept { return m_gpu.read(); }

  private:
    bool m_open = false;
    glintfx::platform::gpu_kind_state m_gpu;
};

} // namespace glintfx::test
