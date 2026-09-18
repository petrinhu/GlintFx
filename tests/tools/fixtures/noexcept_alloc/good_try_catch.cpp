// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_try_catch.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ABSOLVIDA.
// Degrau 2 da escada (docs/api-conventions.md R3, "capturar e
// converter"): alocacao dentro de `try` com `catch` eficaz
// (`std::bad_alloc`) ao redor, em funcao noexcept - a mesma forma que
// `query_i915_local_memory()` (src/platform/wayland/
// drm_device_facts.cpp) ja usava antes desta fatia.
#include <vector>

namespace {

bool fill_values(std::vector<int> &out) noexcept {
    try {
        out.push_back(1);
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

} // namespace
