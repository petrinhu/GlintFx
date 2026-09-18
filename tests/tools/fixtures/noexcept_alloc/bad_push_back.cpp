// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_push_back.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ACUSADA
// (familia B). A forma de B1/B2/B3 (RELATORIO.md secao 3):
// push_back() dentro de funcao noexcept, sem try ao redor - sob falta
// de memoria, std::bad_alloc escapa e mata o processo do consumidor
// (GODS_LAWS.md L-22 do projeto).
#include <vector>

namespace {

void append_value(std::vector<int> &spans, int value) noexcept { spans.push_back(value); }

} // namespace
