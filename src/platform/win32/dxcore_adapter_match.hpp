// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "platform/win32/dxcore_adapter_enumeration.hpp"

// platform/win32/dxcore_adapter_match.hpp - GL-GPU-KIND (docs/plano-
// w6b-fatias-5.md sec. 1/D-W6b-32; docs/plano-w6b-fatias-5b-revisao.md
// sec. 4.1): the PURE atom that answers "which DXCore adapter, if any,
// is the one THIS GL context is actually rendering on" - LUID first
// (GL_EXT_memory_object_win32's own GL_DEVICE_LUID_EXT, exact, F13),
// `DriverDescription` substring of `GL_RENDERER` as a reserve ONLY
// when the LUID route does not resolve (the NVIDIA mismatch F3/sec. 1
// measured: "/PCIe/SSE2" suffix GL adds that DXCore's own Description
// never carries).
//
// AMBIGUITY IS "NOT FOUND", NEVER "THE FIRST ONE" (sec. 1's own
// correction of the plan's original "casar pelo primeiro"): two
// adapters sharing the same nonzero LUID, or two whose Description is
// each a substring of the same GL_RENDERER, both resolve to
// std::nullopt - a wrong guess here would silently mislabel which GPU
// this library reports, and D-W6b-13's own "unknown e o padrao
// honesto" would be broken by a match this atom is not sure of.
//
// WIN32-DOMAIN, NOT WIN32-ONLY: takes only dxcore_adapter_facts (plain
// data), compiles and is tested on all five platforms.

namespace glintfx::platform {

[[nodiscard]] std::optional<std::size_t>
match_dxcore_adapter(std::span<const dxcore_adapter_facts> facts, std::uint64_t gl_luid,
                     std::string_view gl_renderer) noexcept;

} // namespace glintfx::platform
