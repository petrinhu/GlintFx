// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/egl_device_enumeration.hpp"

#include <array>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <glintfx/core/err_code.hpp>

#include "platform/wayland/drm_device_facts.hpp"
#include "platform/wayland/drm_gpu_kind.hpp"
#include "platform/wayland/egl_device_dedup.hpp"

// egl_device_enumeration.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md
// sec. 4.1, F3, D-W6b-33): eglQueryDevicesEXT resolved through eglGet
// ProcAddress() (never a static link, the extension is not
// guaranteed) - the SAME two-call idiom (max count, then real count)
// the header's own worked example already documents.

namespace glintfx::platform {

namespace {

using egl_query_devices_fn = EGLBoolean (*)(EGLint, EGLDeviceEXT *, EGLint *);

// A generous ceiling (F3's own sonda saw 4 entries for 3 real GPUs on
// this machine) - no machine this project has measured comes close,
// and a machine that somehow has more simply sees the first
// k_max_devices, never a crash.
constexpr EGLint k_max_devices = 32;

} // namespace

gltfx_rslt<std::vector<egl_device_facts>> enumerate_egl_devices() noexcept {
    auto query_devices =
        reinterpret_cast<egl_query_devices_fn>(eglGetProcAddress("eglQueryDevicesEXT"));
    if (query_devices == nullptr) {
        return gltfx_rslt<std::vector<egl_device_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("egl_device_enumeration"));
    }

    std::array<EGLDeviceEXT, k_max_devices> devices{};
    EGLint num_devices = 0;
    if (query_devices(k_max_devices, devices.data(), &num_devices) != EGL_TRUE) {
        return gltfx_rslt<std::vector<egl_device_facts>>::err(
            gltfx_err(gltfx_err_code::unsupported).with_rejected_value("egl_device_enumeration"));
    }

    std::vector<egl_device_facts> raw;
    raw.reserve(static_cast<std::size_t>(num_devices));
    for (EGLint i = 0; i < num_devices; ++i) {
        raw.push_back(
            query_egl_device_facts(reinterpret_cast<void *>(devices[static_cast<std::size_t>(i)])));
    }

    const std::vector<std::size_t> survivor_indices = dedup_egl_devices(raw);
    std::vector<egl_device_facts> survivors;
    survivors.reserve(survivor_indices.size());
    for (const std::size_t idx : survivor_indices) {
        survivors.push_back(raw[idx]);
    }

    return gltfx_rslt<std::vector<egl_device_facts>>::ok(std::move(survivors));
}

gltfx_rslt<std::vector<gltfx_gpu_info>>
enumerate_gpus_egl(std::vector<std::string> &names_out) noexcept {
    const gltfx_rslt<std::vector<egl_device_facts>> devices = enumerate_egl_devices();
    if (devices.has_error()) {
        return gltfx_rslt<std::vector<gltfx_gpu_info>>::err(devices.error());
    }

    const std::vector<egl_device_facts> &survivors = devices.value();

    // Reserved to its FINAL size before a single string_view is taken
    // from it (this header's own top comment) - no push_back below
    // ever reallocates names_out.
    names_out.assign(survivors.size(), std::string{});

    std::vector<gltfx_gpu_info> entries;
    entries.reserve(survivors.size());

    for (std::size_t i = 0; i < survivors.size(); ++i) {
        const egl_device_facts &device = survivors[i];
        gltfx_gpu_kind kind = gltfx_gpu_kind::unknown;

        if (device.software) {
            kind = gltfx_gpu_kind::software;
        } else {
            const std::string &node =
                !device.render_node.empty() ? device.render_node : device.primary_node;
            if (!node.empty()) {
                kind = classify_drm_gpu(read_drm_device_facts(node));
            }
        }

        // `names_out[i]` stays empty on Linux - this header's own top
        // comment names the reason (no per-device GL_RENDERER without
        // a live context on that device).
        entries.push_back(gltfx_gpu_info{.kind = kind,
                                         .name = names_out[i],
                                         .enumeration_index = static_cast<std::uint32_t>(i)});
    }

    return gltfx_rslt<std::vector<gltfx_gpu_info>>::ok(std::move(entries));
}

} // namespace glintfx::platform
