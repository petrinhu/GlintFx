// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_err_copy_accessor.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (FAMILIA C - copia de
// `gltfx_err`, TODO.md ERR-COPY-GATE). VEREDITO ESPERADO: ACUSADA.
// A forma "ACESSOR_err_constref" (plano.md secao 3.1, 28 sitios
// reais, 27 defeitos): `X.err()` devolve `const gltfx_err&`, e montar
// o parametro POR VALOR de `::err()` copia condicionalmente (aloca se
// a origem carrega contexto). MESMA FORMA REAL medida hoje em
// src/platform/wayland/seat_adapter.cpp:88 (`seat_proxy.err()`,
// dentro de `open() noexcept`, sem `try`) - por isso este arquivo
// tambem serve de CALIBRACAO deste portao (DEGRAU 4b de run_gate(),
// roda em toda execucao real, nao so' no --selftest).
//
// Namespace NOMEADO, nao anonimo (item TODO.md
// CPPCHECK-ODR-FALSO-NAS-FIXTURES-DE-ERRCOPY): `namespace { ... }`
// sozinho nao evita `ctuOneDefinitionRuleViolation` do cppcheck entre
// as 5 fixtures desta pasta que declaram `gltfx_err`/`gltfx_rslt` -
// medido ao vivo, o matcher `ctu` normaliza o namespace anonimo para
// o MESMO texto de escopo em toda TU. Um nome UNICO por arquivo
// resolve; o TEXTO do tipo continua `gltfx_err` (e' nisso que
// classify_err_copy_arg ancora).
namespace fixture_bad_err_copy_accessor {

struct gltfx_err {
    explicit gltfx_err(int code) : m_code(code) {}
    int m_code;
};

template <typename T> struct gltfx_rslt {
    static gltfx_rslt err(gltfx_err failure) noexcept { return gltfx_rslt{failure}; }
    [[nodiscard]] bool has_error() const noexcept { return true; }
    [[nodiscard]] const gltfx_err &err() const noexcept { return m_err; }
    gltfx_err m_err;
};

gltfx_rslt<void> bind_seat(gltfx_rslt<int> seat_proxy) noexcept {
    if (seat_proxy.has_error()) {
        return gltfx_rslt<void>::err(seat_proxy.err());
    }
    return gltfx_rslt<void>::err(gltfx_err(0));
}

} // namespace fixture_bad_err_copy_accessor
