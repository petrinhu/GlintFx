// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gl_memory_facts.hpp"

namespace {

// gl.xml (third_party/khronos/gl.xml:5842-5843, GODS_LAWS.md L-07: no
// vendored header included, only the two numbers cited here).
constexpr unsigned int k_gl_gpu_memory_info_dedicated_vidmem_nvx = 0x9047;
constexpr unsigned int k_gl_gpu_memory_info_total_available_memory_nvx = 0x9048;

// gl.xml:625.
constexpr unsigned int k_gl_invalid_enum = 0x0500;

} // namespace

gl_memory_facts read_gl_memory_facts(gl_get_integerv_fn get_integerv,
                                     gl_get_error_fn get_error) noexcept {
    gl_memory_facts facts;

    if (get_integerv == nullptr || get_error == nullptr) {
        return facts; // nvx_present=false
    }

    // Drain any error already pending before this atom's own two
    // calls, so a stale GL_INVALID_ENUM from earlier caller code is
    // never mistaken for "this extension is absent".
    while (get_error() != 0) {
        // drain
    }

    int dedicated_kb = 0;
    get_integerv(k_gl_gpu_memory_info_dedicated_vidmem_nvx, &dedicated_kb);
    if (get_error() == k_gl_invalid_enum) {
        return facts; // extension absent - nvx_present stays false
    }

    int total_available_kb = 0;
    get_integerv(k_gl_gpu_memory_info_total_available_memory_nvx, &total_available_kb);
    if (get_error() == k_gl_invalid_enum) {
        return facts; // half-present is treated the same as absent
    }

    facts.nvx_present = true;
    facts.dedicated_kb = dedicated_kb;
    facts.total_available_kb = total_available_kb;
    return facts;
}
