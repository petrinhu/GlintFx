// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_err_copy_in_try.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (FAMILIA C - copia de
// `gltfx_err`, TODO.md ERR-COPY-GATE). VEREDITO ESPERADO: ABSOLVIDA.
// Idioma FIX-OOM-B9 (degrau 2 da escada, docs/api-conventions.md R3):
// identificador nomeado ("LVALUE_nomeado" no oraculo, plano.md secao
// 3.1) copiado dentro de `try` com `catch (const std::bad_alloc&)`
// eficaz ao redor. MESMA FORMA REAL, ja' consertada, de
// src/platform/gl/gpu_enumeration_facade.cpp:92 e :154 - por isso
// este arquivo tambem serve de CALIBRACAO deste portao (DEGRAU 4b de
// run_gate(), roda em toda execucao real, nao so' no --selftest).
#include <new>

namespace {

struct gltfx_err {
    explicit gltfx_err(int code) : m_code(code) {}
    int m_code;
};

template <typename T> struct gltfx_rslt {
    static gltfx_rslt err(gltfx_err failure) noexcept { return gltfx_rslt{failure}; }
    gltfx_err m_err;
};

gltfx_rslt<int> forward_error_guarded(const gltfx_err &error) noexcept {
    try {
        return gltfx_rslt<int>::err(error);
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<int>::err(gltfx_err(3));
    }
}

} // namespace
