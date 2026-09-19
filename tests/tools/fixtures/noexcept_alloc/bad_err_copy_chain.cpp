// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_err_copy_chain.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (FAMILIA C - copia de
// `gltfx_err`, TODO.md ERR-COPY-GATE). VEREDITO ESPERADO: ACUSADA.
// A forma que responde por 108 dos 179 sitios `::err(` reais medidos
// (plano.md secao 3.1, "CADEIA_with_lvalue", 102 defeitos):
// `with_rejected_value()` devolve `gltfx_err&` - uma REFERENCIA de
// lvalue (include/glintfx/core/err.hpp:254-258), entao
// `gltfx_err(codigo).with_rejected_value(v)` NAO e' prvalue. Montar o
// parametro POR VALOR de `::err()` (err.hpp:299/:391) a partir desse
// lvalue chama o construtor de copia, que aloca (src/core/err.cpp:34)
// e nao e' `noexcept`. Sob falta de memoria, dentro desta funcao
// `noexcept` sem `try`, mata o processo do consumidor (GODS_LAWS.md
// L-22 do projeto). Mimico minimo, mesma forma textual do
// `err.hpp`/`err.cpp` reais (ESCOPO.md Decisao 17) - nao compila
// sozinho, nao precisa: este portao e' analise de texto.
#include <string_view>

struct gltfx_err {
    explicit gltfx_err(int code) : m_code(code) {}
    gltfx_err &with_rejected_value(std::string_view value) {
        m_rejected_value = value;
        return *this;
    }
    int m_code;
    std::string_view m_rejected_value;
};

template <typename T> struct gltfx_rslt {
    static gltfx_rslt err(gltfx_err failure) noexcept { return gltfx_rslt{failure}; }
    gltfx_err m_err;
};

namespace {

gltfx_rslt<int> reject_name(std::string_view name) noexcept {
    return gltfx_rslt<int>::err(gltfx_err(1).with_rejected_value(name));
}

} // namespace
