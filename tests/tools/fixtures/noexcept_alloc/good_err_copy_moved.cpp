// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_err_copy_moved.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (FAMILIA C - copia de
// `gltfx_err`, TODO.md ERR-COPY-GATE). VEREDITO ESPERADO: ABSOLVIDA.
// `std::move(error)` - nunca copia. Fato que o proprio plano registra
// (plano.md secao 3.1): zero dos 179 sitios reais usam `std::move` -
// a biblioteca inteira nunca move um erro, so' copia ou constroi novo.
// Este fixture prova que, SE algum dia um sitio usar `std::move`, o
// detector o absolve corretamente.
#include <utility>

namespace {

struct gltfx_err {
    explicit gltfx_err(int code) : m_code(code) {}
    int m_code;
};

template <typename T> struct gltfx_rslt {
    static gltfx_rslt err(gltfx_err failure) noexcept { return gltfx_rslt{failure}; }
    gltfx_err m_err;
};

gltfx_rslt<int> forward_error(gltfx_err error) noexcept {
    return gltfx_rslt<int>::err(std::move(error));
}

} // namespace
