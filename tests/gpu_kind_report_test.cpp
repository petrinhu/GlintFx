// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/sink.hpp>
#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_report.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gpu_kind_report_test.cpp - CL-7 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-04/L-17/L-19/L-20/L-40): resolve_gpu_kind_report(), the
// PLATFORM-AGNOSTIC atom plano/core-log.md Q6.2/D-LOG-8 calls for -
// same shape as gpu_kind_seam.hpp's own two pure functions
// (gpu_kind_seam_test.cpp is this file's own model), which this atom
// WRAPS instead of duplicating (GODS_LAWS.md L-17: one table of
// logic, not two).
//
// THE CONTROL-FLOW SPACE, enumerated (L-17): resolve_gpu_kind_report()
// has exactly 4 paths - kernel already answered; kernel unknown but
// exclusion (via 1) resolves; both unknown but memory separation
// (via 2) resolves; all three unknown. All 4 have a case below, 0
// without.
//
// report_gpu_kind_resolved() (the actual emission) is tested via the
// REAL sink registry (core/log/sink.hpp) - the same "register, emit,
// read back through the public accessors" shape log_event_test.cpp
// already uses, proving the event this atom builds is genuinely
// USABLE by a sink, not just internally consistent.

using glintfx::gltfx_gpu_kind;
using glintfx::gltfx_log_event;
using glintfx::gltfx_log_set_sink;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_sink;
using glintfx::platform::gpu_kind_path;
using glintfx::platform::gpu_kind_report;
using glintfx::platform::report_gpu_kind_resolved;
using glintfx::platform::resolve_gpu_kind_report;

namespace {
constexpr gltfx_gpu_kind unk = gltfx_gpu_kind::unknown;
constexpr gltfx_gpu_kind shr = gltfx_gpu_kind::shared;
constexpr gltfx_gpu_kind ded = gltfx_gpu_kind::dedicated;
} // namespace

GLINTFX_TEST(resolve_gpu_kind_report_4_caminhos) {
    int analyzed = 0;

    // Caminho 1: kernel ja respondeu - path=kernel, independente do
    // resto (duas variacoes de valor, mesmo caminho de controle).
    {
        const gpu_kind_report r = resolve_gpu_kind_report(ded, 0, {}, false, std::nullopt);
        GLINTFX_CHECK(r.kind == ded);
        GLINTFX_CHECK(r.path == gpu_kind_path::kernel);
        ++analyzed;
    }
    {
        const gpu_kind_report r =
            resolve_gpu_kind_report(gltfx_gpu_kind::software, 0, {}, true, std::optional{ded});
        GLINTFX_CHECK(r.kind == gltfx_gpu_kind::software);
        GLINTFX_CHECK(r.path == gpu_kind_path::kernel);
        ++analyzed;
    }

    // Caminho 2: kernel unknown, via 1 (exclusao) resolve -
    // path=exclusion. Mesma fixture de resolve_kernel_and_exclusion's
    // own "caminho 3" (gpu_kind_seam_test.cpp): um `shared` na lista
    // faz os `unknown` virarem `dedicated`.
    {
        const std::array<gltfx_gpu_kind, 2> kinds{unk, shr};
        const gpu_kind_report r = resolve_gpu_kind_report(unk, 0, kinds, false, std::nullopt);
        GLINTFX_CHECK(r.kind == ded);
        GLINTFX_CHECK(r.path == gpu_kind_path::exclusion);
        ++analyzed;
    }

    // Caminho 3: kernel unknown, via 1 continua unknown, via 2
    // (separacao de memoria) resolve - path=memory_separation.
    {
        const gpu_kind_report r = resolve_gpu_kind_report(
            unk, 0, std::array<gltfx_gpu_kind, 1>{unk}, true, std::optional{shr});
        GLINTFX_CHECK(r.kind == shr);
        GLINTFX_CHECK(r.path == gpu_kind_path::memory_separation);
        ++analyzed;
    }

    // Caminho 4: as tres vias esgotadas, ainda unknown -
    // path=unknown.
    {
        const gpu_kind_report r = resolve_gpu_kind_report(
            unk, 0, std::array<gltfx_gpu_kind, 1>{unk}, false, std::nullopt);
        GLINTFX_CHECK(r.kind == unk);
        GLINTFX_CHECK(r.path == gpu_kind_path::unknown);
        ++analyzed;
    }

    std::println("gpu_kind_report_test: {} celula(s) conferida(s), 4 caminho(s) de controle, "
                 "4 com teste, 0 sem teste",
                 analyzed);
    GLINTFX_CHECK_EQ(analyzed, 5);
}

namespace {

struct received_event {
    bool called = false;
    gltfx_log_severity severity{};
    std::string category;
    std::string name;
    std::string kind_field;
    std::string path_field;
};

void recording_sink(void *sink_context, const gltfx_log_event &event) noexcept {
    auto *received = static_cast<received_event *>(sink_context);
    received->called = true;
    received->severity = event.severity();
    received->category = std::string(event.category());
    received->name = std::string(event.name());
    for (const auto &field : event.fields()) {
        if (field.name == "kind") {
            received->kind_field = std::string(field.value.text());
        } else if (field.name == "path") {
            received->path_field = std::string(field.value.text());
        }
    }
}

} // namespace

GLINTFX_TEST(report_gpu_kind_resolved_reaches_a_registered_sink) {
    gltfx_log_set_sink(gltfx_log_sink{});
    received_event received;
    gltfx_log_set_sink(gltfx_log_sink{&recording_sink, &received, gltfx_log_severity::trace});

    report_gpu_kind_resolved(gpu_kind_report{ded, gpu_kind_path::exclusion});

    GLINTFX_CHECK(received.called);
    GLINTFX_CHECK(received.category == "platform.gl");
    GLINTFX_CHECK(received.name == "gpu_kind_resolved");
    GLINTFX_CHECK(received.kind_field == "dedicated");
    GLINTFX_CHECK(received.path_field == "exclusion");
    gltfx_log_set_sink(gltfx_log_sink{});
}
