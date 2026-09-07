// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/drm_device_facts.hpp"

#include <cstddef>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// kernel-headers UAPI, the SAME allowlisted category libwayland-client
// already is for the dependency-zero rule (GODS_LAWS.md L-07,
// tests/tools/check_dep_zero.py's own SO_HEADER_ALLOWLIST, this
// fatia's own addition): plain C ioctl argument structs, no runtime
// library behind them at all.
#include <drm/amdgpu_drm.h>
#include <drm/drm.h>
#include <drm/i915_drm.h>
#include <drm/nouveau_drm.h>
#include <drm/xe_drm.h>

// drm_device_facts.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec.
// 4.1, D-W6b-30; docs/plano-w6b-fatias-5b-revisao.md sec. 1.1/3,
// D-W6b-38): the real DRM ioctl calls this machine's own sonda (F3 of
// the plan) already proved against real hardware - DRM_IOCTL_VERSION
// for the driver name, then the ONE query D-W6b-30/38 names for that
// driver.
//
// NOUVEAU'S OWN NVIF `platform` QUERY IS DELIBERATELY NOT IMPLEMENTED
// HERE (revisao.md sec. 1.6 N4): its wire format (a generic ioctl
// envelope, nvif_ioctl_v0, carrying a nested "mthd" call) lives ONLY in
// libdrm's own nvif/cl0080.h, a THIRD-PARTY copy this project's
// dependency-zero rule (L-07) forbids including - and this project has
// no nouveau hardware anywhere (this machine's own NVIDIA runs the
// proprietary nvidia-drm driver, F3) to verify a hand-rolled wire
// format against. `nouveau_platform` stays at its default (-1, "not
// consulted") - classify_drm_gpu() already falls back to `nouveau_bus_
// type` (GETPARAM, fully within the plain UAPI header, implemented
// below) when `nouveau_platform` is unconsulted, and to the two vias
// (gpu_kind_exclusion/memory_separation_kind) when BOTH are (GODS_
// LAWS.md L-36: a mechanism never measured, and never measurable on
// hardware this project has, is not something this file pretends to
// have proven).

namespace {

[[nodiscard]] bool read_driver_name(int fd, std::string &out) noexcept {
    drm_version probe_size{};
    if (ioctl(fd, DRM_IOCTL_VERSION, &probe_size) != 0) {
        return false;
    }
    if (probe_size.name_len == 0) {
        out.clear();
        return true;
    }

    std::vector<char> buffer(static_cast<std::size_t>(probe_size.name_len));
    drm_version fetch{};
    fetch.name_len = probe_size.name_len;
    fetch.name = buffer.data();
    if (ioctl(fd, DRM_IOCTL_VERSION, &fetch) != 0) {
        return false;
    }

    out.assign(buffer.data(), static_cast<std::size_t>(fetch.name_len));
    return true;
}

void query_amdgpu_fusion(int fd, bool &fusion_out) noexcept {
    drm_amdgpu_info_device dev_info{};
    drm_amdgpu_info info{};
    info.return_pointer = reinterpret_cast<__u64>(&dev_info);
    info.return_size = sizeof(dev_info);
    info.query = AMDGPU_INFO_DEV_INFO;

    if (ioctl(fd, DRM_IOCTL_AMDGPU_INFO, &info) != 0) {
        return; // fusion_out keeps its caller-supplied default
    }
    fusion_out = (dev_info.ids_flags & AMDGPU_IDS_FLAGS_FUSION) != 0;
}

void query_i915_local_memory(int fd, bool &has_local_out) noexcept {
    drm_i915_query_item item{};
    item.query_id = DRM_I915_QUERY_MEMORY_REGIONS;
    drm_i915_query query{};
    query.num_items = 1;
    query.items_ptr = reinterpret_cast<__u64>(&item);

    // Two-call idiom (the header's own worked example, i915_drm.h):
    // first with length 0 to learn the blob size.
    if (ioctl(fd, DRM_IOCTL_I915_QUERY, &query) != 0 || item.length <= 0) {
        return;
    }

    std::vector<std::byte> buffer(static_cast<std::size_t>(item.length));
    item.data_ptr = reinterpret_cast<__u64>(buffer.data());
    if (ioctl(fd, DRM_IOCTL_I915_QUERY, &query) != 0) {
        return;
    }

    const auto *regions = reinterpret_cast<const drm_i915_query_memory_regions *>(buffer.data());
    for (__u32 i = 0; i < regions->num_regions; ++i) {
        if (regions->regions[i].region.memory_class == I915_MEMORY_CLASS_DEVICE) {
            has_local_out = true;
            return;
        }
    }
    has_local_out = false;
}

void query_xe_local_memory(int fd, bool &has_local_out) noexcept {
    drm_xe_device_query query{};
    query.query = DRM_XE_DEVICE_QUERY_MEM_REGIONS;

    if (ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query) != 0 || query.size == 0) {
        return;
    }

    std::vector<std::byte> buffer(query.size);
    query.data = reinterpret_cast<__u64>(buffer.data());
    if (ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query) != 0) {
        return;
    }

    const auto *regions = reinterpret_cast<const drm_xe_query_mem_regions *>(buffer.data());
    for (__u32 i = 0; i < regions->num_mem_regions; ++i) {
        if (regions->mem_regions[i].mem_class == DRM_XE_MEM_REGION_CLASS_VRAM) {
            has_local_out = true;
            return;
        }
    }
    has_local_out = false;
}

void query_nouveau_bus_type(int fd, int &bus_type_out) noexcept {
    drm_nouveau_getparam getparam{};
    getparam.param = NOUVEAU_GETPARAM_BUS_TYPE;
    if (ioctl(fd, DRM_IOCTL_NOUVEAU_GETPARAM, &getparam) != 0) {
        return; // bus_type_out keeps its caller-supplied -1 ("not consulted")
    }
    bus_type_out = static_cast<int>(getparam.value);
}

} // namespace

drm_device_facts read_drm_device_facts(std::string_view node_path) noexcept {
    drm_device_facts facts;

    const std::string node(node_path);
    const int fd = open(node.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return facts; // opened=false
    }
    facts.opened = true;

    if (!read_driver_name(fd, facts.driver)) {
        close(fd);
        return facts; // query_ok=false
    }
    facts.query_ok = true;

    if (facts.driver == "amdgpu") {
        query_amdgpu_fusion(fd, facts.amdgpu_fusion);
    } else if (facts.driver == "i915") {
        query_i915_local_memory(fd, facts.has_device_local_memory);
    } else if (facts.driver == "xe") {
        query_xe_local_memory(fd, facts.has_device_local_memory);
    } else if (facts.driver == "nouveau") {
        query_nouveau_bus_type(fd, facts.nouveau_bus_type);
    }

    close(fd);
    return facts;
}
