// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/drm_device_facts.hpp"

#include <array>
#include <climits>
#include <cstddef>
#include <new>
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

#include "platform/nul_terminated_name.hpp"

// NOEXCEPT-ALLOC-B8 fatia F2 (/var/tmp/glintfx-plan/plano-conserto-
// noexcept.md sec. "F2", D-2, GODS_LAWS.md L-04/L-20/L-22): read_drm_
// device_facts() below used to build `const std::string node(node_
// path)` inside a `noexcept` function - an allocating, throw-capable
// constructor inside a boundary that promises `noexcept`, the SAME
// shape F1 already fixed on the OTHER site that copies a bounded,
// well-known string into a stack buffer before handing it to a C API
// (wayland_egl_context_adapter::proc_address(), egl_context_adapter.
// cpp). PATH_MAX (D-2 of the plan: the cap the system's own open()
// call already imposes, not a number invented for this fatia) is the
// cap glintfx::platform::copy_nul_terminated() (platform/nul_
// terminated_name.hpp, F1's own atom) is instantiated with here - the
// SAME template F1 already uses for k_max_proc_name_chars, just a
// different N, so a change to the atom's own contract (never
// truncating, always refusing past the boundary) changes both sites
// at once, by construction.

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

    // GODS_LAWS.md L-22: constructing the std::vector<char>/std::string::
    // assign() below can throw std::bad_alloc inside a noexcept function -
    // the SAME guard gfx_open_only_fixation.cpp's own resolve_gfx_
    // open_only_fixation() already applies. Degrades to `false`, the
    // SAME honest outcome this atom already uses for "the ioctl failed"
    // a few lines above - the caller (read_drm_device_facts()) already
    // treats `false` as "query_ok=false" and moves on without this
    // classification, never reading `out`.
    try {
        std::vector<char> buffer(static_cast<std::size_t>(probe_size.name_len));
        drm_version fetch{};
        fetch.name_len = probe_size.name_len;
        fetch.name = buffer.data();
        if (ioctl(fd, DRM_IOCTL_VERSION, &fetch) != 0) {
            return false;
        }

        out.assign(buffer.data(), static_cast<std::size_t>(fetch.name_len));
    } catch (const std::bad_alloc &) {
        return false;
    }
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

    // GODS_LAWS.md L-22: constructing the std::vector<std::byte> below
    // can throw std::bad_alloc inside a noexcept function - the SAME
    // guard read_driver_name() already applies, in this same file.
    // Degrades to the SAME early return this atom already uses when
    // the ioctl fails - `has_local_out` keeps the default the caller
    // already supplied.
    try {
        std::vector<std::byte> buffer(static_cast<std::size_t>(item.length));
        item.data_ptr = reinterpret_cast<__u64>(buffer.data());
        if (ioctl(fd, DRM_IOCTL_I915_QUERY, &query) != 0) {
            return;
        }

        const auto *regions =
            reinterpret_cast<const drm_i915_query_memory_regions *>(buffer.data());
        for (__u32 i = 0; i < regions->num_regions; ++i) {
            if (regions->regions[i].region.memory_class == I915_MEMORY_CLASS_DEVICE) {
                has_local_out = true;
                return;
            }
        }
        has_local_out = false;
    } catch (const std::bad_alloc &) {
        return;
    }
}

void query_xe_local_memory(int fd, bool &has_local_out) noexcept {
    drm_xe_device_query query{};
    query.query = DRM_XE_DEVICE_QUERY_MEM_REGIONS;

    if (ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query) != 0 || query.size == 0) {
        return;
    }

    // GODS_LAWS.md L-22: same guard as query_i915_local_memory() above,
    // for the same class of allocation (std::vector<std::byte> inside
    // a noexcept function).
    try {
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
    } catch (const std::bad_alloc &) {
        return;
    }
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

    // A path longer than PATH_MAX (counting the terminator, the SAME
    // contract copy_nul_terminated() already has) is a REFUSAL, never
    // a silent truncation (docs/api-conventions.md R3, "internal
    // failure degrades, it never throws across it or aborts" - a
    // truncated path would open a DIFFERENT node than the one asked
    // for, which is worse than refusing). Refusing here is the SAME
    // honest outcome this function already uses just below for "open()
    // failed" - no new state, no new error channel.
    std::array<char, PATH_MAX> node_buffer{};
    if (!glintfx::platform::copy_nul_terminated(node_buffer, node_path)) {
        return facts; // opened=false: path >= PATH_MAX, refused without allocating
    }
    const int fd = open(node_buffer.data(), O_RDWR | O_CLOEXEC);
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
