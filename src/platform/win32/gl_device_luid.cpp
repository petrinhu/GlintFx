// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/gl_device_luid.hpp"

#include <cstring>

// gl_device_luid.cpp - GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec.
// 0/F13, D-W6b-32): GL_DEVICE_LUID_EXT's own 8 raw bytes are, per the
// EXT_external_objects_win32 spec, the SAME byte layout Win32's own
// LUID struct (two little-endian 32-bit halves) already has in memory
// on every target this project ships (x86/x64 Windows is little-
// endian) - a plain std::memcpy() into a std::uint64_t here produces
// the EXACT SAME 64-bit value dxcore_adapter_enumeration.cpp's own
// std::memcpy() of a DXCoreAdapterProperty::InstanceLuid property
// produces, with no byte-swapping needed on either side.

namespace glintfx::platform {

namespace {
constexpr unsigned int k_gl_device_luid_ext = 0x9599; // gl.xml:6912
constexpr unsigned int k_gl_luid_size_ext = 8;        // gl.xml:549
} // namespace

std::uint64_t read_gl_device_luid(gl_get_unsigned_bytev_ext_fn get_unsigned_bytev_ext) noexcept {
    if (get_unsigned_bytev_ext == nullptr) {
        return 0;
    }

    unsigned char bytes[k_gl_luid_size_ext] = {};
    get_unsigned_bytev_ext(k_gl_device_luid_ext, bytes);

    std::uint64_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

} // namespace glintfx::platform
