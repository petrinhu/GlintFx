// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// platform/nul_terminated_name.hpp - NOEXCEPT-ALLOC-B8 fatia F1
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F1",
// GODS_LAWS.md L-04/L-33): the ONE atom that copies a std::string_view
// into a fixed-capacity, NUL-terminated buffer WITHOUT ever
// allocating - src/platform/win32/wgl_proc_address.cpp's own
// copy_name_into() (the shape this atom generalizes) already
// documents the reasoning clang-tidy's bugprone-exception-escape gave
// on the real Windows CI runner (babbd77): `const std::string
// owned(name)` inside a `noexcept` function converts an unlucky
// std::bad_alloc into std::terminate() - the WHOLE consumer process
// dies, with no chance to react (ESCOPO.md, "a biblioteca nunca mata
// o processo do consumidor", Decisao 8, 16/09/2026).
//
// THIS IS THE THIRD OCCURRENCE OF THE SAME SHAPE (win32 proc_address,
// wayland proc_address, and F2's own DRM node path, all copying a
// bounded, well-known name/path into a stack buffer before handing it
// to a C API that demands NUL termination) - CONTRACT.md's own "regra
// de 3" is satisfied HERE, not violated: the plan's own F1 section
// names all three callers by file and line before this header was
// written.
//
// GENERIC OVER THE BUFFER'S OWN CAPACITY (std::array<char, N>&), not
// hard-coded to 256: F1's own caller (proc name, k_max_proc_name_chars
// below) and F2's own caller (a DRM device node path, PATH_MAX) need
// two DIFFERENT teto sizes - D-2 of the plan (paridade da L-04
// aplicada mecanicamente ao numero que o lado Windows ja usa, PATH_MAX
// para caminho de no DRM, "o teto que o proprio open() do sistema ja
// impoe"). One template, two instantiations, paridade guaranteed by
// the COMPILER (the same reasoning gpu_kind_seam.hpp's own header
// comment already gives itself), never by two files read side by
// side.
//
// A `name` too long to fit is a REFUSAL, never a truncation
// (docs/api-conventions.md R3's own "internal failure degrades, it
// never throws across it or aborts" - silently cutting a name short
// would hand a caller a DIFFERENT, wrong name instead of an honest
// lookup miss). The caller decides what a refusal means (F1: an
// ordinary unresolved-name miss; F2: the same `opened=false` the
// function already returns for "did not open").
namespace glintfx::platform {

// The teto F1 uses for a GL/WGL/EGL function name (D-2 of the plan:
// paridade byte a byte with src/platform/win32/wgl_proc_address.cpp's
// own k_max_name_chars, which this atom now replaces there too) -
// every real, published ARB/EXT function name stays well under 100
// characters (wgl_proc_address.cpp's own header comment already
// measured this before this atom existed).
inline constexpr std::size_t k_max_proc_name_chars = 256;

// Copies `name` into `buffer`, NUL-terminated, iff it fits WITH ROOM
// for the terminator (name.size() < buffer.size() - the "<", not
// "<=", is the one byte this atom reserves for '\0': a name exactly
// buffer.size() long has no room left for it and must refuse, the
// same off-by-one wgl_proc_address.cpp's own copy_name_into() already
// got right before this atom existed).
//
// tests/proc_name_buffer_test.cpp's own "estoura por um byte" case
// proves this boundary directly: a name exactly buffer.size() long
// (`>=` triggers, this function refuses) never reaches
// buffer[name.size()] == buffer[buffer.size()] - one past the last
// valid index of a std::array<char, N> (valid indices are 0..N-1),
// which a `>` comparison alone would have let through as undefined
// behaviour. The FIRST cut of this function used `>` instead of `>=`
// on purpose (GODS_LAWS.md L-20's own "o vermelho e de execucao...
// nunca de compilacao") - the red that case captured, and the SHA it
// was captured against, are both cited in this fatia's own commit
// message and closing report, never repeated here (this comment would
// itself rot the next time this file is read, GODS_LAWS.md L-44).
template <std::size_t N>
[[nodiscard]] bool copy_nul_terminated(std::array<char, N> &buffer,
                                       std::string_view name) noexcept {
    if (name.size() >= buffer.size()) {
        return false;
    }
    name.copy(buffer.data(), name.size());
    buffer[name.size()] = '\0';
    return true;
}

} // namespace glintfx::platform
