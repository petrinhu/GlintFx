// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_duplicate_site_merges.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ACUSADA, e
// UMA SO VEZ - nunca duas. Reproduz o defeito medido em 18/09/2026 na
// fatia F6 (achado do time-lead, contra c08d0ed/9dd5917 reais em
// src/gfss/selector_parse.cpp): um `struct { ... };` de nivel de
// namespace logo APOS uma funcao noexcept faz find_functions() varrer
// para tras, atravessar de volta o corpo inteiro da funcao anterior
// (o ramo que pula blocos `{ }` ja casados) e reencontrar a lista de
// parametros da PROPRIA funcao anterior como se fosse o cabecalho do
// struct - produz DUAS entradas em tree["funcs"] para o MESMO
// arquivo:linha:funcao. Sem agrupar por sitio (arquivo, linha, nome)
// antes de contar, o relatorio emitia a MESMA acusacao duas vezes, e
// "familia_B_achados" publicava contagem de LINHA, nunca de SITIO
// (GODS_LAWS.md L-43). Aqui a funcao duplicada tambem e alcancada por
// DOIS caminhos de propagacao transitiva distintos (via helper_a() e
// via helper_b(), nenhum protegido por try eficaz) - o veredito
// esperado e UMA UNICA linha de achado para target(), com os DOIS
// caminhos fundidos no mesmo "herdado", nunca duas linhas identicas
// nem um caminho descartado.
#include <vector>

namespace {

void helper_a(std::vector<int> &out) { out.push_back(1); }
void helper_b(std::vector<int> &out) { out.push_back(2); }

void target(std::vector<int> &out) noexcept {
    helper_a(out);
    helper_b(out);
}

// O struct abaixo e o gatilho do defeito: nao ha funcao nenhuma entre
// ele e target() acima - e exatamente a forma que expos o achado real
// (struct attribute_operator_outcome logo apos parse_pseudo_selector()
// em src/gfss/selector_parse.cpp).
struct trailing_struct {
    bool matched = false;
};

} // namespace
