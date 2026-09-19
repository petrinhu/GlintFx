// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_err_copy_prvalue.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (FAMILIA C - copia de
// `gltfx_err`, TODO.md ERR-COPY-GATE). VEREDITO ESPERADO: ABSOLVIDA.
// `gltfx_err(codigo)` puro, sem cadeia `.with_*()` - e' prvalue,
// elisao garantida desde C++17, NUNCA copia. Este e' o CONTROLE que
// impede o detector de acusar TODO `::err(` (plano.md secao 6): 26
// dos 179 sitios reais sao exatamente esta forma
// ("PRVALUE_trivial"), e nenhum e' defeito.
//
// Namespace NOMEADO (mesma razao das 4 fixturas-irmas desta pasta -
// ver o comentario em bad_err_copy_accessor.cpp: `namespace { ... }`
// anonimo sozinho nao evita `ctuOneDefinitionRuleViolation` do
// cppcheck, item TODO.md CPPCHECK-ODR-FALSO-NAS-FIXTURES-DE-ERRCOPY).
namespace fixture_good_err_copy_prvalue {

struct gltfx_err {
    explicit gltfx_err(int code) : m_code(code) {}
    int m_code;
};

template <typename T> struct gltfx_rslt {
    static gltfx_rslt err(gltfx_err failure) noexcept { return gltfx_rslt{failure}; }
    gltfx_err m_err;
};

gltfx_rslt<int> out_of_memory() noexcept { return gltfx_rslt<int>::err(gltfx_err(2)); }

} // namespace fixture_good_err_copy_prvalue
