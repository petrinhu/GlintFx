// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gl_memory_facts.hpp"

namespace {

// gl.xml (third_party/khronos/gl.xml:5842-5843, GODS_LAWS.md L-07: no
// vendored header included, only the two numbers cited here).
constexpr unsigned int k_gl_gpu_memory_info_dedicated_vidmem_nvx = 0x9047;
constexpr unsigned int k_gl_gpu_memory_info_total_available_memory_nvx = 0x9048;

// gl.xml:625.
constexpr unsigned int k_gl_invalid_enum = 0x0500;

// read_capacity_token() - CONSERTO (06/09/2026, GL-GPU-KIND, achado do
// team-lead: "a via da memoria acabou de classificar um renderizador
// por software como placa dedicada"): reads the SAME token TWICE and
// only trusts the value if BOTH reads agree. Both tokens this atom
// reads name a CAPACITY (how much VRAM this device HAS), never a
// live usage counter (that would be GL_NVX_gpu_memory_info's own
// GPU_MEMORY_INFO_CURRENT_AVAILABLE_MEMORY_NVX, a DIFFERENT token
// this atom never reads) - a driver that genuinely implements the
// extension reports the identical capacity number on every read,
// with nothing GL-side that could change it between two calls one
// instruction apart.
//
// MEASURED, 06/09/2026, inside the exact container image this
// project's own CI uses (Fedora latest, Mesa 26.1.8's own llvmpipe):
// glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, ...) raises
// NO error (glGetError() == 0, the extension answers "supported") but
// writes UNINITIALIZED memory back - three separate process runs read
// three different, arbitrary numbers (-21578144, 892646176,
// 237507360) for the exact same query on the exact same driver. This
// is a real defect in THAT Mesa build's own NVX_gpu_memory_info
// implementation, not a hypothesis - classify_by_memory_separation()
// (memory_separation_kind.cpp) was already CORRECT given whatever this
// atom handed it; this atom was the one handing it garbage as if it
// were a real capacity.
//
// Two consecutive reads of an uninitialized value overwhelmingly
// disagree (the two probes above never matched, across three separate
// runs) - the SAME belt this project's own two-call idioms elsewhere
// (DRM_IOCTL_VERSION, DXCore's GetPropertySize+GetProperty) already
// wear, applied here because THIS extension's own contract gives no
// second call to size a buffer against, only a suspiciously easy
// place for a driver to silently skip writing anything at all.
[[nodiscard]] bool read_capacity_token(gl_get_integerv_fn get_integerv, gl_get_error_fn get_error,
                                       unsigned int token, int &out_value) noexcept {
    int first = 0;
    get_integerv(token, &first);
    if (get_error() == k_gl_invalid_enum) {
        return false; // token itself not recognized - extension absent
    }

    int second = 0;
    get_integerv(token, &second);
    if (get_error() == k_gl_invalid_enum) {
        return false;
    }

    if (first != second) {
        return false; // two reads of a CAPACITY disagreed - untrustworthy, never a real answer
    }

    out_value = first;
    return true;
}

} // namespace

gl_memory_facts read_gl_memory_facts(gl_get_integerv_fn get_integerv,
                                     gl_get_error_fn get_error) noexcept {
    gl_memory_facts facts;

    if (get_integerv == nullptr || get_error == nullptr) {
        return facts; // nvx_present=false
    }

    // Drain any error already pending before this atom's own calls, so
    // a stale GL_INVALID_ENUM from earlier caller code is never
    // mistaken for "this extension is absent".
    while (get_error() != 0) {
        // drain
    }

    int dedicated_kb = 0;
    if (!read_capacity_token(get_integerv, get_error, k_gl_gpu_memory_info_dedicated_vidmem_nvx,
                             dedicated_kb)) {
        return facts; // absent, or the double-read disagreed (untrustworthy)
    }

    int total_available_kb = 0;
    if (!read_capacity_token(get_integerv, get_error,
                             k_gl_gpu_memory_info_total_available_memory_nvx, total_available_kb)) {
        return facts; // half-present, or untrustworthy - treated the same as absent
    }

    facts.nvx_present = true;
    facts.dedicated_kb = dedicated_kb;
    facts.total_available_kb = total_available_kb;
    return facts;
}
