// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_fixed_buffer.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ABSOLVIDA.
// Degrau 1 da escada (docs/api-conventions.md R3, "nao alocar"): o
// mesmo idioma de F1/F2 (src/platform/nul_terminated_name.hpp,
// src/platform/wayland/drm_device_facts.cpp) - `std::array` de
// capacidade fixa mais copia dimensionada, nunca alocacao no monte.
#include <array>
#include <cstring>

namespace {

constexpr std::size_t k_max_name_chars = 64;

void copy_name(std::array<char, k_max_name_chars> &out, const char *name,
               std::size_t len) noexcept {
    const std::size_t n = len < k_max_name_chars ? len : k_max_name_chars - 1;
    std::memcpy(out.data(), name, n);
    out[n] = '\0';
}

} // namespace
