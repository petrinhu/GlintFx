// SPDX-License-Identifier: AGPL-3.0-or-later
#include <type_traits>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// err_copy_is_nothrow_static_assert_test.cpp - T1 da onda W-ERRCOPY
// (TODO.md ERR-COPY-RED, ESCOPO.md Decisao 17, GODS_LAWS.md
// L-20/L-35): a Decisao 17 fixa que a copia de gltfx_err vira noexcept
// e livre de alocacao (contagem de referencia intrusiva dentro de
// err_context, com copia-na-escrita - plano da onda, secao 5(d)). Hoje
// gltfx_err(const gltfx_err&) e' declarado em include/glintfx/core/
// err.hpp:194 SEM noexcept, porque a copia profunda definida em
// src/core/err.cpp:33-37 aloca (`new err_context(*other.m_context)`,
// forma QUE LANCA) sempre que o erro de origem carrega contexto - o
// proprio compilador reprova este static_assert por isso, ANTES de
// qualquer linha de teste rodar.
//
// VERMELHO DE COMPILACAO, DE GRACA (plano da onda, secao 7, linha T1):
// e' vermelho nos CINCO alvos ao mesmo tempo, sem precisar executar
// nada - o compilador e' o oraculo. Nao toca src/ nem include/: este
// arquivo mora inteiramente em tests/, e o static_assert consulta o
// TIPO publico (std::is_nothrow_copy_constructible_v), nunca reescreve
// a assinatura real.
//
// ESCOPO, DECLARADO: este arquivo prova apenas a CATEGORIA DE TIPO.
// tests/err_no_alloc_test.cpp (T2) prova por CONTAGEM que uma copia
// com contexto anexado nao aloca; err_copy_terminates_window_
// validation_test.cpp (T3) prova que uma funcao noexcept real da
// arvore sobrevive sob alocador armado. As tres sao complementares -
// nenhuma substitui as outras duas.
static_assert(
    std::is_nothrow_copy_constructible_v<glintfx::gltfx_err>,
    "ESCOPO.md Decisao 17: a copia de gltfx_err deve ser noexcept e livre de alocacao "
    "(contagem de referencia intrusiva + copia-na-escrita, ver plano da onda W-ERRCOPY secao "
    "5(d)) - hoje gltfx_err(const gltfx_err&) (include/glintfx/core/err.hpp:194) NAO e' "
    "noexcept porque a copia profunda (src/core/err.cpp:33-37) aloca com `new` que lanca "
    "sempre que o erro de origem carrega contexto; conserta-se na fatia F4 desta onda, nao "
    "aqui");

// O corpo abaixo so' roda no dia em que o static_assert acima ja'
// tiver compilado (F4 em diante) - nesta fatia (F2) a compilacao nunca
// chega aqui. Existe para F4 herdar um teste funcional pronto, em vez
// de F4 ter de escrever um do zero so' para ligar o static_assert a
// alguma execucao real.
GLINTFX_TEST(copy_constructor_type_is_nothrow_after_fix) {
    const glintfx::gltfx_err original(glintfx::gltfx_err_code::not_found);
    const glintfx::gltfx_err copy(original);
    GLINTFX_CHECK(copy.code() == glintfx::gltfx_err_code::not_found);
}
