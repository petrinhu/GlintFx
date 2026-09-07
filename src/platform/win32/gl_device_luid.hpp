// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/win32/gl_device_luid.hpp - GL-GPU-KIND (docs/plano-w6b-
// fatias-5.md sec. 0/F13, D-W6b-32): the EXACT match between "which
// adapter is this GL context on" and DXCore's own InstanceLuid - the
// Mesa D3D12 driver (this project's own executor, F4/F13) exposes
// GL_EXT_memory_object_win32 since Mesa 22.2.0, and GL_DEVICE_LUID_EXT
// is the token this atom reads through it.
//
// TOKENS DECLARED BY HAND, NO VENDORED HEADER INCLUDED (GODS_LAWS.md
// L-07, the SAME technique gl_memory_facts.cpp/wgl_context_adapter.cpp
// already use for their own GL constants) - cited against third_party/
// khronos/gl.xml:549 (GL_LUID_SIZE_EXT=8) and :6912 (GL_DEVICE_LUID_
// EXT=0x9599).
//
// glGetUnsignedBytevEXT IS RESOLVED BY THE CALLER (wgl_context_
// adapter.cpp), the SAME "adapter already has the proc address, this
// shared atom does not re-resolve it" shape gl_memory_facts.hpp
// already uses one directory over.
//
// `__stdcall` IS A BARE COMPILER KEYWORD (no <windows.h> needed - this
// file lives under platform/win32/, Windows-only, so unlike gl_memory_
// facts.hpp one directory over there is no "other platform" branch to
// keep empty): every GL entry point on Windows, including an EXT one
// resolved through wglGetProcAddress, is __stdcall (wgl_context_
// adapter.cpp's own extern "C" ... WINAPI declarations) - a bare
// function-pointer typedef with no convention defaults to __cdecl,
// which corrupts the stack on every call through it.
using gl_get_unsigned_bytev_ext_fn = void(__stdcall *)(unsigned int pname, unsigned char *data);

namespace glintfx::platform {

// Packs GL_DEVICE_LUID_EXT's own 8 raw bytes into ONE std::uint64_t,
// little-endian (the byte order DXCore's own LUID struct - two
// adjacent std::uint32_t on a little-endian Windows target - already
// has in memory, so this value compares equal to a dxcore_adapter_
// facts::luid built the SAME way from LUID.LowPart/HighPart, D-W6b-37).
// `get_unsigned_bytev_ext == nullptr` (the extension not resolved at
// all) returns 0 - the SAME "no LUID" sentinel match_dxcore_adapter()
// already treats as "never attempt the LUID route" (dxcore_adapter_
// match.hpp).
[[nodiscard]] std::uint64_t
read_gl_device_luid(gl_get_unsigned_bytev_ext_fn get_unsigned_bytev_ext) noexcept;

} // namespace glintfx::platform
