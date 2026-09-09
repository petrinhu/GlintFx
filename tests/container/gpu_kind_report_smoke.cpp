// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/wayland/egl_context_adapter.hpp"
#include "platform/window/display_impl.hpp"
#include "platform/window/window_impl.hpp"

// gpu_kind_report_smoke.cpp - CORE-LOG CL-7 (TODO.md W5, GODS_LAWS.md
// L-04/L-09/L-20): the "caminho real disponivel na plataforma" half
// of Q6 (plano/core-log.md) - registers a recebedor through the
// PUBLIC sink API (core/log/sink.hpp), opens display+window+EGL
// context through the SAME public path facade_pin_smoke.cpp already
// uses (this file's own header comment is the fuller account of that
// open sequence), and asserts the `gpu_kind_resolved` event this
// project's structured log carries for the first time arrives with
// readable category/name/fields - proof this event is genuinely
// USABLE from outside egl_context_adapter.cpp, against a REAL
// compositor, not a synthetic fixture.
//
// THE MUTATION THIS FIXTURE EXISTS TO CATCH (CL-7's own obrigatoria,
// GODS_LAWS.md L-27): removing the report_gpu_kind_resolved() call
// from egl_context_adapter.cpp's own classify_current_gpu() makes
// `g_received` stay false and this fixture exit non-zero - measured
// live, in a copy of this file's own tracked commit, against a copy
// of egl_context_adapter.cpp with that ONE line removed, OUTSIDE this
// repository (never in place, never on a tracked file), rebuilt and
// run against this SAME container image. Baseline: received=true,
// category=platform.gl, name=gpu_kind_resolved, exit 0. Mutated:
// received=false, exit 1. The service order report for this fatia has
// the full transcript.
//
// WHY THIS PROVES PARIDADE FOR REAL, NOT A UNIT-TESTED SEAM: unlike
// gpu_kind_report_test.cpp (which recompiles resolve_gpu_kind_report()
// directly, no OS involved), this fixture goes through the REAL
// wayland_egl_context_adapter::open() - the exact call classify_
// current_gpu() lives inside - talking to the REAL kwin_wayland
// compositor this container runs (GODS_LAWS.md L-09: never the
// session vivo).

namespace {
bool g_received = false;
std::string g_category;
std::string g_name;
std::string g_kind_field;
std::string g_path_field;

void recording_sink(void * /*sink_context*/, const glintfx::gltfx_log_event &event) noexcept {
    g_received = true;
    g_category = std::string(event.category());
    g_name = std::string(event.name());
    for (const auto &field : event.fields()) {
        if (field.name == "kind") {
            g_kind_field = std::string(field.value.text());
        } else if (field.name == "path") {
            g_path_field = std::string(field.value.text());
        }
    }
}
} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_smoke.cpp's own header
    // comment on this exact line).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_log_set_sink(
        glintfx::gltfx_log_sink{&recording_sink, nullptr, glintfx::gltfx_log_severity::trace});

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "gpu_kind_report_smoke: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "gpu_kind_report_smoke",
        .application_id = "org.glintfx.gpu_kind_report_smoke",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(
            stderr, "gpu_kind_report_smoke: gltfx_window::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    glintfx::window_impl *w_impl = glintfx::window_internal_access::get(window);

    auto *egl_ptr = new glintfx::platform::wayland_egl_context_adapter();
    const std::span<const glintfx::gltfx_gfx_option_entry> no_options{};
    if (glintfx::gltfx_rslt<void> egl_opened = egl_ptr->open(w_impl->adapter, no_options);
        egl_opened.has_error()) {
        std::fprintf(stderr, "gpu_kind_report_smoke: egl adapter open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(egl_opened.error().code())).c_str());
        delete egl_ptr;
        return EXIT_FAILURE;
    }

    delete egl_ptr;
    glintfx::gltfx_log_set_sink(glintfx::gltfx_log_sink{});

    // MEASURED lines (tests/tools/collect_measured.py's own token
    // shape) - printed always, before any pass/fail decision, so a
    // future run that behaves differently leaves a record of exactly
    // what it saw (same discipline egl_probe_smoke.cpp's own header
    // comment documents for its own MEASURED lines).
    std::fprintf(stdout, "MEASURED gpu_kind_report_smoke.received=%s\n",
                 g_received ? "true" : "false");
    std::fprintf(stdout, "MEASURED gpu_kind_report_smoke.category=%s\n", g_category.c_str());
    std::fprintf(stdout, "MEASURED gpu_kind_report_smoke.name=%s\n", g_name.c_str());
    std::fprintf(stdout, "MEASURED gpu_kind_report_smoke.kind=%s\n", g_kind_field.c_str());
    std::fprintf(stdout, "MEASURED gpu_kind_report_smoke.path=%s\n", g_path_field.c_str());

    if (!g_received || g_category != "platform.gl" || g_name != "gpu_kind_resolved") {
        std::fprintf(stderr, "gpu_kind_report_smoke: FAIL - gpu_kind_resolved event not "
                             "received or malformed\n");
        return EXIT_FAILURE;
    }

    std::fprintf(stdout,
                 "gpu_kind_report_smoke: ok - gpu_kind_resolved received, kind=%s path=%s\n",
                 g_kind_field.c_str(), g_path_field.c_str());
    return EXIT_SUCCESS;
}
