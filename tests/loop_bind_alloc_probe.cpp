// SPDX-License-Identifier: AGPL-3.0-or-later
#include "loop_bind_alloc_probe.hpp"

#include <cstdlib>
#include <new>

#include <glintfx/platform/loop/loop_bind.hpp>

// loop_bind_alloc_probe.cpp - see loop_bind_alloc_probe.hpp's own top
// comment for WHY this lives in its own translation unit. The counting
// technique itself is the SAME "TU-local operator new override, F24"
// shape tests/loop_open_alloc_failure_test.cpp's own header comment
// already documents, one directory over.

namespace {

// probe_app - never named outside this TU: tests/loop_bind_test.cpp
// only ever sees the gltfx_loop_callbacks value allocate_and_adopt_
// probe() below hands back, never this type itself. That is what
// makes the returned destroy_context genuinely opaque to the caller.
struct probe_app {
    bool frame(const glintfx::gltfx_frame_tick &) noexcept { return true; }
    void render(const glintfx::gltfx_frame_tick &) noexcept {}
};

std::size_t g_new_call_count = 0;

} // namespace

void *operator new(std::size_t size) {
    ++g_new_call_count;
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

namespace glintfx_test_probe {

std::size_t new_call_count() { return g_new_call_count; }

glintfx::gltfx_loop_callbacks allocate_and_adopt_probe() {
    auto *heap_app = new probe_app();
    return glintfx::gltfx_adopt_loop_callbacks<&probe_app::frame, &probe_app::render>(heap_app);
}

} // namespace glintfx_test_probe
