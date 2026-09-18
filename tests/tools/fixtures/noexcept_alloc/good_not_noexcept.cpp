// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_not_noexcept.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ABSOLVIDA.
// Degrau 3 da escada (docs/api-conventions.md R3, "o noexcept
// decorativo cai"): a MESMA alocacao de bad_push_back.cpp, mas numa
// funcao que NAO promete noexcept - a excecao propaga normalmente
// pelas regras comuns de C++, sem std::terminate() nenhum.
#include <vector>

namespace {

void append_value(std::vector<int> &spans, int value) { spans.push_back(value); }

} // namespace
