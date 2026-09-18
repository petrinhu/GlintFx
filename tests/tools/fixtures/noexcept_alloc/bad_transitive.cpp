// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_transitive.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ACUSADA
// (familia B, por PROPAGACAO TRANSITIVA). A forma de B4: uma funcao
// noexcept chama, sem try ao redor, uma funcao COMUM do proprio
// projeto que aloca - a promessa noexcept da funcao de fora e
// quebrada pela chamada, mesmo sem alocacao no proprio texto dela. O
// portao ingenuo que so olha o corpo direto da funcao noexcept perde
// exatamente este caso.
#include <vector>

namespace {

void fill_values(std::vector<int> &out) { out.push_back(1); }

void caller(std::vector<int> &out) noexcept { fill_values(out); }

} // namespace
