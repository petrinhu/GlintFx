// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gpu_kind_report.hpp"

#include <array>
#include <string_view>

#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/value.hpp>

#include "core/log/emit.hpp"
#include "platform/gl/gpu_kind_seam.hpp"

namespace glintfx::platform {

namespace {

std::string_view gpu_kind_name(gltfx_gpu_kind kind) noexcept {
    switch (kind) {
    case gltfx_gpu_kind::unknown:
        return "unknown";
    case gltfx_gpu_kind::software:
        return "software";
    case gltfx_gpu_kind::shared:
        return "shared";
    case gltfx_gpu_kind::dedicated:
        return "dedicated";
    }
    return "unknown";
}

std::string_view gpu_kind_path_name(gpu_kind_path path) noexcept {
    switch (path) {
    case gpu_kind_path::unknown:
        return "unknown";
    case gpu_kind_path::kernel:
        return "kernel";
    case gpu_kind_path::exclusion:
        return "exclusion";
    case gpu_kind_path::memory_separation:
        return "memory_separation";
    }
    return "unknown";
}

std::span<const gltfx_log_field> return_prebuilt_fields(void *builder_context) noexcept {
    return *static_cast<std::span<const gltfx_log_field> *>(builder_context);
}

} // namespace

gpu_kind_report
resolve_gpu_kind_report(gltfx_gpu_kind kernel_kind, std::uint32_t enumeration_index,
                        std::span<const gltfx_gpu_kind> enumeration_kinds,
                        bool definitely_not_software,
                        std::optional<gltfx_gpu_kind> memory_separation_kind) noexcept {
    // Caminho 1, MESMA ordem de resolve_kernel_and_exclusion() -
    // kernel ja respondeu, nada mais e olhado.
    if (kernel_kind != gltfx_gpu_kind::unknown) {
        return {kernel_kind, gpu_kind_path::kernel};
    }

    // Caminho 2: delega a via 1 (exclusao) a resolve_kernel_and_
    // exclusion() - se ela resolveu, path=exclusion.
    const gltfx_gpu_kind after_via1 =
        resolve_kernel_and_exclusion(kernel_kind, enumeration_index, enumeration_kinds);
    if (after_via1 != gltfx_gpu_kind::unknown) {
        return {after_via1, gpu_kind_path::exclusion};
    }

    // Caminho 3: delega a via 2 (separacao de memoria) a resolve_
    // memory_separation() - se ela resolveu, path=memory_separation.
    const gltfx_gpu_kind final_kind =
        resolve_memory_separation(after_via1, definitely_not_software, memory_separation_kind);
    if (final_kind != gltfx_gpu_kind::unknown) {
        return {final_kind, gpu_kind_path::memory_separation};
    }

    // Caminho 4: as tres vias esgotadas.
    return {gltfx_gpu_kind::unknown, gpu_kind_path::unknown};
}

void report_gpu_kind_resolved(gpu_kind_report report) noexcept {
    const std::array<gltfx_log_field, 2> fields = {
        gltfx_log_field{"kind", gltfx_log_value::make_text(gpu_kind_name(report.kind))},
        gltfx_log_field{"path", gltfx_log_value::make_text(gpu_kind_path_name(report.path))},
    };
    std::span<const gltfx_log_field> span_view{fields};
    log_emit(gltfx_log_severity::info, "platform.gl", "gpu_kind_resolved", &return_prebuilt_fields,
             &span_view);
}

} // namespace glintfx::platform
